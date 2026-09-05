#!/usr/bin/env python3
"""Prova que single-thread/multi-thread/bandit ENCERRAM de verdade.

POR QUE ISTO EXISTE (ver a "decima oitava passada" do ./app no CLAUDE.md
raiz): sair do ./app com [q] costumava travar o processo para sempre. A causa
tinha DUAS metades:

  1. um ::send() SEM TETO em shared/xtacview/RealtimeTelemetryServer.cpp --
     um cliente Tacview que conecta e para de ler enchia o buffer do socket e
     o send() bloqueava para sempre, dentro de station->updateData(). Ja
     corrigido com SO_SNDTIMEO, e a correcao vale para as TRES pocs porque a
     lib e a MESMA (shared/xtacview e uma unica copia, sem variante por poc).

  2. a ORDEM de encerramento nao parar a thread de tempo critico nativa ANTES
     do SHUTDOWN_EVENT -- ela sobrevive ao fim do laco de tempo real e corre
     contra o teardown da Simulation (auto-deadlock documentado em
     xclock/ClockStation.hpp) e/ou segue enfileirando registros numa fila sem
     teto depois que ninguem mais a drena. Essa metade so tinha sido aplicada
     ao ./app (app/Shutdown.hpp) -- este teste prova que agora tambem vale
     para single-thread/multi-thread/bandit (mesmo padrao, replicado em
     cada app/Shutdown.hpp/.cpp local), que rodam a MESMA forma de laco
     (createTimeCriticalProcess() + updateData() em laco continuo).

DIFERENCA para tests/scenario/run_app_quit_test.py: nao ha TUI/pty aqui --
estas tres pocs tratam Ctrl+C (SIGINT) direto e nao dependem de terminal
(xclock::ConsoleKeyboard degrada sozinho sem TTY, ver o cabecalho dela). Mais
simples: sobe o binario com stdin/stdout/stderr redirecionados, espera
estabilizar, opcionalmente conecta um cliente Tacview que para de ler
(reproduz o defeito real medido no ./app), manda SIGINT e afirma que o
processo TERMINA dentro do prazo com codigo de saida limpo.
"""

import argparse
import os
import signal
import socket
import subprocess
import sys
import time

# Tempo para o binario subir (parse do .edl, JSBSim, createTimeCriticalProcess())
# antes de mandar qualquer coisa.
ESPERA_ESTAVEL = 3.0
# Tempo rodando com o cliente travado, para o buffer do socket encher.
ESPERA_CLIENTE = 8.0
# Teto para o encerramento. O caminho bom leva bem menos de 1s; isto e folga
# larga para maquina carregada, nao uma expectativa -- ver run_app_quit_test.py.
PRAZO_SAIDA = 20.0


def mata(proc):
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except OSError:
        proc.kill()


def roda(binario, args_extra, porta_tacview, rotulo):
    proc = subprocess.Popen(
        [binario] + args_extra, cwd=os.getcwd(),
        stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        start_new_session=True)
    cliente = None
    try:
        time.sleep(ESPERA_ESTAVEL)
        if proc.poll() is not None:
            print(f"FALHA [{rotulo}]: o processo morreu antes do SIGINT (rc={proc.returncode})")
            return False

        if porta_tacview:
            cliente = socket.socket()
            # SO_RCVBUF minusculo ANTES do connect: encolhe a janela anunciada,
            # entao o buffer de escrita do binario enche em segundos.
            cliente.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 512)
            cliente.settimeout(5)
            try:
                cliente.connect(("127.0.0.1", porta_tacview))
            except OSError as e:
                print(f"AVISO [{rotulo}]: nao conectei em {porta_tacview} ({e}) -- "
                      "pulando o caso do cliente travado")
                cliente = None
            else:
                # NAO le nada, de proposito -- e o defeito real medido no ./app.
                time.sleep(ESPERA_CLIENTE)

        if proc.poll() is not None:
            print(f"FALHA [{rotulo}]: o processo morreu antes do SIGINT (rc={proc.returncode})")
            return False

        proc.send_signal(signal.SIGINT)

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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--porta-tacview", type=int, default=None,
                     help="porta do TacviewOutput desta poc, para exercitar o "
                          "caso do cliente travado (omitir pula esse caso)")
    ap.add_argument("--arg-extra", action="append", default=[],
                     help="argumento extra para o binario (repetivel)")
    args = ap.parse_args()

    binario = os.path.abspath(args.binario)

    ok = roda(binario, args.arg_extra, None, "saida normal")

    if args.porta_tacview:
        ok = roda(binario, args.arg_extra, args.porta_tacview,
                  f"cliente travado :{args.porta_tacview}") and ok

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
