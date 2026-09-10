#!/usr/bin/env python3
"""Estresse do caminho INTERATIVO do ./app -- o unico que a suite nao cobria.

POR QUE ISTO EXISTE
-------------------
Toda a suite roda '-deterministic': sem TUI, sem TTY e -- o que mais importa --
sem a thread de tempo critico NATIVA (esse modo chama tcFrame() direto na
propria thread). O unico teste interativo era run_app_quit_test.py: ~8 s de
vida, e a unica interacao era '+' e 'q'.

Este teste exercita, em laco, as tres coisas que so existem no modo interativo:

  1) A SAIDA. A Station tem refCount 2 no caminho interativo (o app tem uma, e
     AbstractThread::staticThreadFunc() faz parent->ref() pela thread T/C
     nativa). Logo o station->unref() de shutdownStation() NAO destroi nada --
     quem destroi e a thread T/C, ate um periodo depois, EM PARALELO com o
     exit() do main e com os destrutores estaticos de todo .so carregado.

  2) O PASSO MANUAL [n] da aba F6 contra a thread T/C nativa. 'isPaused()' e um
     bool comum e a thread T/C so o testa na ENTRADA de
     processTimeCriticalTasks() -- ela pode estar dentro de um lote de ate 64
     tcFrame() quando o flag vira. Duas threads em Simulation::updateTC() ao
     mesmo tempo corrompem o pool de SyncThread.

  3) A ABA F6 aberta, que percorre o grafo VIVO do MIXR na thread de DESENHO
     (discoverComponentTree, ~10 Hz) enquanto as threads T/C criam e destroem
     objetos.

Nao le a tela (nao precisa de pyte, que nao esta instalado): so afirma que cada
ciclo termina dentro do prazo e com codigo de saida limpo. Um encerramento por
SINAL aparece em Popen.returncode como um numero NEGATIVO -- e e exatamente o
'core dumped' que este teste existe para pegar.
"""

import argparse
import collections
import fcntl
import os
import pty
import re
import signal
import struct
import subprocess
import sys
import termios
import threading
import time

# Blocos de 64 KiB guardados do pty para diagnostico (ver despeja_diagnostico).
CAUDA_BLOCOS = 64

ESPERA_TUI = 6.0        # o TUI desenhar e a simulacao assentar (settleMs = 1 s)
PRAZO_SAIDA = 30.0      # o caminho bom leva ~0,2 s; folga larga para maquina carregada

F6 = b"\x1b[17~"        # aba "Componentes" -- a que percorre o grafo vivo
F1 = b"\x1bOP"          # aba "Players"
F2 = b"\x1bOQ"          # aba "Mapa"


def sobe_app(binario, pasta, cenario, linhas=50, colunas=200):
    """Sobe o app num pty de verdade -- o FTXUI exige TTY para o modo bruto.

    Devolve (proc, mestre, cauda). 'cauda' e um deque com os ultimos blocos
    lidos do pty: e ali que aparece o relatorio do AddressSanitizer quando o
    binario e instrumentado, e sem guarda-lo uma falha vira so um numero de
    sinal, sem stack trace.
    """
    mestre, escravo = pty.openpty()
    fcntl.ioctl(escravo, termios.TIOCSWINSZ, struct.pack("HHHH", linhas, colunas, 0, 0))

    proc = subprocess.Popen(
        [binario, "-folder", pasta, "-scenario", cenario],
        cwd=os.getcwd(), stdin=escravo, stdout=escravo, stderr=escravo,
        close_fds=True, start_new_session=True)
    os.close(escravo)

    cauda = collections.deque(maxlen=CAUDA_BLOCOS)

    # Drenar o pty numa thread e obrigatorio: com o buffer cheio o app bloqueia
    # no proprio write() e o teste mediria a coisa errada.
    def drena():
        while True:
            try:
                d = os.read(mestre, 65536)
                if not d:
                    return
                cauda.append(d)
            except OSError:
                return

    threading.Thread(target=drena, daemon=True).start()
    return proc, mestre, cauda


def despeja_diagnostico(cauda, rotulo):
    """Imprime o que o app disse antes de morrer -- so as linhas que importam.

    A tela do FTXUI e redesenho puro e nao ajuda em nada; o que interessa e o
    relatorio do ASan/glibc, que sai em stderr NO MEIO do desenho.
    """
    bruto = b"".join(cauda)
    texto = bruto.decode("utf-8", "replace")
    marcas = ("AddressSanitizer", "SUMMARY:", "#0 ", "#1 ", "#2 ", "#3 ", "#4 ",
              "#5 ", "#6 ", "#7 ", "#8 ", "ERROR:", "runtime error", "Assertion")
    linhas = [l.strip() for l in texto.splitlines()
              if any(m in l for m in marcas)]
    print(f"    --- diagnostico [{rotulo}] ---")
    if linhas:
        for l in linhas[:45]:
            print(f"    {l}")
        return
    # Sem marca conhecida: despeja o fim do fluxo sem as sequencias ANSI do
    # redesenho, que sozinhas nao dizem nada.
    limpo = re.sub(r"\x1b\[[0-9;?]*[a-zA-Z]|\x1b[()][A-Z0-9]|\x1b[=>]|\r", " ", texto)
    limpo = re.sub(r"[ \t]{3,}", "  ", limpo)
    uteis = [l.strip() for l in limpo.splitlines() if l.strip()]
    print("    (sem relatorio do sanitizer; ultimas linhas do pty:)")
    for l in uteis[-25:]:
        print(f"    {l[:200]}")


def sobe_carga(n):
    """Ocupa N nucleos com laco vazio, e devolve os processos.

    POR QUE ISTO E PARTE DO TESTE, e nao ruido:
    as corridas que este teste caca sao janelas de TOCTOU e de interleaving
    entre threads. Numa maquina ociosa cada thread corre do inicio ao fim do
    seu trecho sem ser desagendada, e a janela praticamente nao abre -- medido:
    20/20 ciclos limpos com E sem as correcoes. Sob contencao (3 compilacoes em
    paralelo, no caso em que o defeito apareceu pela primeira vez) o
    escalonador interrompe as threads no meio dos trechos criticos e a mesma
    bateria acusou 2 falhas em 10 ciclos. Gerar a contencao aqui e o que torna
    o teste REPRODUTIVEL em vez de dependente de quem mais esta usando a
    maquina.
    """
    return [subprocess.Popen(["sh", "-c", "while :; do :; done"],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             start_new_session=True)
            for _ in range(n)]


def mata_carga(procs):
    for p in procs:
        try:
            os.killpg(os.getpgid(p.pid), signal.SIGKILL)
        except OSError:
            pass
    for p in procs:
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass


def mata(proc):
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except OSError:
        proc.kill()


def descreve_rc(rc):
    """Popen.returncode negativo == morto por sinal. E o caso que interessa."""
    if rc is None:
        return "ainda rodando"
    if rc < 0:
        nome = signal.Signals(-rc).name if -rc in signal.Signals._value2member_map_ else "?"
        return f"morto por sinal {-rc} ({nome})"
    return f"rc={rc}"


def teclas(mestre, seq, pausa=0.15):
    for t in seq:
        os.write(mestre, t)
        time.sleep(pausa)


def ciclo(binario, pasta, cenario, indice, agitar):
    proc, mestre, cauda = sobe_app(binario, pasta, cenario)
    try:
        time.sleep(ESPERA_TUI)
        if proc.poll() is not None:
            print(f"FALHA [ciclo {indice}]: morreu antes de qualquer tecla "
                  f"({descreve_rc(proc.returncode)})")
            despeja_diagnostico(cauda, "antes das teclas")
            return False

        if agitar:
            # (3) F6 aberta: discoverComponentTree() percorre o grafo vivo na
            #     thread de DESENHO enquanto as threads T/C o mutam.
            teclas(mestre, [F6], pausa=1.5)

            # (2) Escala alta ANTES do passo: no topo da escada a thread T/C
            #     fica dentro de um lote de ate 64 tcFrame() por chamada, entao
            #     marcar o freeze nao a interrompe -- e a simThread entra em
            #     tcFrame() ao mesmo tempo.
            teclas(mestre, [b"+"] * 8, pausa=0.1)

            # [n] (passo) alternado com [espaco] (pausa/despausa): o despause
            # dentro da janela do passo e o pior dos dois caminhos.
            for _ in range(20):
                teclas(mestre, [b" ", b"n", b"n", b"n", b" "], pausa=0.04)

            # Redimensionar durante a execucao -- gatilho classico de
            # intermitencia em TUI.
            for lin, col in ((30, 100), (60, 220), (40, 150)):
                fcntl.ioctl(mestre, termios.TIOCSWINSZ,
                            struct.pack("HHHH", lin, col, 0, 0))
                time.sleep(0.3)

            # Passear pelas abas com a simulacao acelerada.
            teclas(mestre, [F1, F2, F6, F1], pausa=0.4)

            if proc.poll() is not None:
                print(f"FALHA [ciclo {indice}]: morreu DURANTE a interacao "
                      f"({descreve_rc(proc.returncode)})")
                despeja_diagnostico(cauda, "durante a interacao")
                return False

        # (1) A saida: [q] abre o dialogo de confirmacao, o Enter confirma.
        os.write(mestre, b"q")
        time.sleep(1.0)
        os.write(mestre, b"\r")

        try:
            rc = proc.wait(timeout=PRAZO_SAIDA)
        except subprocess.TimeoutExpired:
            print(f"FALHA [ciclo {indice}]: nao encerrou em {PRAZO_SAIDA:.0f}s "
                  "-- processo pendurado")
            return False

        if rc != 0:
            print(f"FALHA [ciclo {indice}]: {descreve_rc(rc)} (esperado rc=0)")
            despeja_diagnostico(cauda, "no encerramento")
            return False

        print(f"ok [ciclo {indice}]: encerrou limpo")
        return True
    finally:
        if proc.poll() is None:
            mata(proc)
        try:
            os.close(mestre)
        except OSError:
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--folder", default="src/poc")
    ap.add_argument("--scenario", default="built-in_mixr_1")
    ap.add_argument("--ciclos", type=int, default=3)
    ap.add_argument("--sem-agitar", action="store_true",
                    help="so sobe e sai -- isola a causa do ENCERRAMENTO das demais")
    ap.add_argument("--carga", type=int, default=-1,
                    help="quantos nucleos ocupar para abrir as janelas de corrida "
                         "(default: 2x os CPUs; 0 desliga -- ver sobe_carga)")
    args = ap.parse_args()

    binario = os.path.abspath(args.binario)
    carga_n = (os.cpu_count() or 4) * 2 if args.carga < 0 else args.carga
    carga = sobe_carga(carga_n)
    if carga_n:
        print(f"(carga: {carga_n} nucleos ocupados para abrir as janelas de corrida)")
    falhas = 0
    try:
        for i in range(1, args.ciclos + 1):
            if not ciclo(binario, args.folder, args.scenario, i, not args.sem_agitar):
                falhas += 1
    finally:
        mata_carga(carga)

    total = args.ciclos
    print(f"\n{total - falhas}/{total} ciclos limpos")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
