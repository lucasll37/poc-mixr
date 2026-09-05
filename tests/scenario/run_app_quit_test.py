#!/usr/bin/env python3
"""Prova que o ./app ENCERRA de verdade quando o usuario aperta [q].

POR QUE ISTO EXISTE, e por que o run_app_test.py nao cobria:
run_app_test.py roda o app em '-deterministic' -- sem TUI, sem TTY, e sobretudo
sem a thread de tempo critico nativa (esse modo chama tcFrame() direto). O
caminho INTERATIVO de saida nunca era exercitado por teste nenhum, e era
justamente ele que travava.

O caso 'cliente-travado' abaixo e a reproducao do defeito real, medida antes da
correcao: um cliente que CONECTA na porta do Tacview e para de ler enche o
buffer do socket; sem SO_SNDTIMEO o ::send() de RealtimeTelemetryServer::
sendRaw() bloqueia PARA SEMPRE, e ele roda dentro de station->updateData(), ou
seja dentro do laco de background do app. Resultado medido: a thread do laco
parada em 'sendto', a main parada no join() dela, o terminal nunca voltando, e
a thread de tempo critico enfileirando registros numa base::List SEM TETO (RSS
subindo ~1,9 MB/s). Depois da correcao: cliente descartado, RSS plano, saida em
0,2 s.

Nao le a tela (nao precisa de pyte): so afirma que o processo termina dentro do
prazo e com codigo de saida limpo.
"""

import argparse
import fcntl
import os
import pty
import signal
import socket
import struct
import subprocess
import sys
import termios
import threading
import time

# Espera o TUI desenhar antes de mandar tecla.
ESPERA_TUI = 8.0
# Tempo rodando com o cliente travado, para o buffer do socket encher.
ESPERA_CLIENTE = 20.0
# Teto para o encerramento. O caminho bom leva ~0,2 s; isto e folga larga para
# maquina carregada, nao uma expectativa.
PRAZO_SAIDA = 30.0


def sobe_app(binario, cenario):
    """Sobe o app num pty de verdade -- o FTXUI exige TTY para o modo bruto."""
    mestre, escravo = pty.openpty()
    # Terminal largo o suficiente para o layout nao reclamar.
    fcntl.ioctl(escravo, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 200, 0, 0))

    proc = subprocess.Popen(
        [binario, "-scenario", cenario],
        cwd=os.getcwd(), stdin=escravo, stdout=escravo, stderr=escravo,
        close_fds=True, start_new_session=True)
    os.close(escravo)

    # Drenar o pty numa thread e obrigatorio: com o buffer cheio o app bloqueia
    # no proprio write() e o teste mediria a coisa errada.
    def drena():
        while True:
            try:
                if not os.read(mestre, 65536):
                    return
            except OSError:
                return

    threading.Thread(target=drena, daemon=True).start()
    return proc, mestre


def mata(proc):
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except OSError:
        proc.kill()


def roda(binario, cenario, porta_tacview, rotulo):
    proc, mestre = sobe_app(binario, cenario)
    cliente = None
    try:
        time.sleep(ESPERA_TUI)
        if proc.poll() is not None:
            print(f"FALHA [{rotulo}]: o app morreu antes do 'q' (rc={proc.returncode})")
            return False

        if porta_tacview:
            cliente = socket.socket()
            # SO_RCVBUF minusculo ANTES do connect: encolhe a janela anunciada,
            # entao o buffer de escrita do app enche em segundos, nao em minutos.
            cliente.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 512)
            cliente.settimeout(5)
            try:
                cliente.connect(("127.0.0.1", porta_tacview))
            except OSError as e:
                print(f"AVISO [{rotulo}]: nao conectei em {porta_tacview} ({e}) -- "
                      "pulando o caso do cliente travado")
                cliente = None
            else:
                # NAO le nada, de proposito. Acelera o tempo para o gravador
                # produzir depressa (a escada de xclock vai ate 64x).
                for _ in range(8):
                    os.write(mestre, b"+")
                    time.sleep(0.2)
                time.sleep(ESPERA_CLIENTE)

        if proc.poll() is not None:
            print(f"FALHA [{rotulo}]: o app morreu antes do 'q' (rc={proc.returncode})")
            return False

        # [q] abre o dialogo de confirmacao; o Enter e que confirma.
        os.write(mestre, b"q")
        time.sleep(1.0)
        os.write(mestre, b"\r")

        try:
            rc = proc.wait(timeout=PRAZO_SAIDA)
        except subprocess.TimeoutExpired:
            print(f"FALHA [{rotulo}]: nao encerrou em {PRAZO_SAIDA:.0f}s "
                  "-- o processo ficou pendurado")
            return False

        if rc != 0:
            print(f"FALHA [{rotulo}]: encerrou com rc={rc} (esperado 0)")
            return False

        print(f"ok [{rotulo}]: encerrou limpo")
        return True
    finally:
        if cliente is not None:
            cliente.close()
        if proc.poll() is None:
            mata(proc)


def porta_do_cenario(cenario):
    """Le a porta do TacviewOutput do .edl ja expandido que o app gera.

    Nao fixa o numero aqui de proposito: o fragmento compartilhado
    (app/configs/fragments/tacview_recorder.edl.frag) e quem manda, e ele ja
    mudou de porta uma vez.
    """
    caminho = os.path.join("app", "configs", f"{cenario}.generated.edl")
    if not os.path.exists(caminho):
        return None
    dentro = False
    with open(caminho, encoding="utf-8", errors="replace") as f:
        for linha in f:
            if "TacviewOutput" in linha:
                dentro = True
            if dentro and "port:" in linha:
                try:
                    return int(linha.split("port:")[1].split()[0])
                except (IndexError, ValueError):
                    return None
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--scenario", default="patrol")
    args = ap.parse_args()

    binario = os.path.abspath(args.binario)

    # 1) Saida normal. Roda primeiro porque tambem GERA o .generated.edl de que
    #    o passo 2 precisa para descobrir a porta.
    ok = roda(binario, args.scenario, None, "saida normal")

    # 2) Saida com um cliente de Tacview conectado que parou de ler -- o defeito
    #    real. Sem a porta nao da para montar o caso; ai o teste vale so pelo (1).
    porta = porta_do_cenario(args.scenario)
    if porta is None:
        print("AVISO: nao descobri a porta do TacviewOutput -- caso do cliente "
              "travado nao exercitado")
    else:
        ok = roda(binario, args.scenario, porta, f"cliente travado :{porta}") and ok

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
