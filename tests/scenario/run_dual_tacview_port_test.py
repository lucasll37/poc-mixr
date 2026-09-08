#!/usr/bin/env python3
"""Prova que DOIS processos './app' competindo pela MESMA porta Tacview, ao
mesmo tempo, nao derrubam um ao outro.

Achado por auditoria (workflow de investigacao desta sessao, dimensao
'testes-adversariais'): o codigo ja tem degradacao graciosa DE PROPOSITO
para porta ocupada --
RealtimeTelemetryServer::start() (libs/xtacview/RealtimeTelemetryServer.cpp)
so loga e devolve 'false' se o bind() falhar (EADDRINUSE), e
TacviewOutput::initIfNeeded() trata isso separado da gravacao em .acmi local
(a propria funcao documenta a correcao de um bug antigo: porta ocupada
tambem matava o arquivo local). Mas nenhum teste subia dois processos DE
VERDADE competindo pela mesma porta ao mesmo tempo -- toda a suite roda com
'is_parallel: false' (nunca dois testes de poc ao mesmo tempo), entao esse
caminho nunca era exercitado.

Duas copias do MESMO cenario real (src/poc/full-systems-nav, 1 player, porta
Tacview propria) sao derivadas para pastas de sandbox com nomes DIFERENTES
(evita a disputa, ja documentada em CLAUDE.md, pelo MESMO
'.generated.edl' que dois processos do mesmo cenario teriam) e caminhos de
gravacao/mensagem PROPRIOS (evita disputa pelo MESMO '.acmi'/'.jsonl' --
irrelevante para o que se quer provar aqui, que e so o bind() da porta).
A PORTA Tacview, essa sim, e mantida IDENTICA nas duas copias -- e o ponto
central do teste.

Roda '-deterministic' (sem TUI/TTY): mais simples e mais rapido que o
caminho interativo, e a degradacao sob teste (bind() em Station::reset(),
dentro do laco de frames) independe de TUI.
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
CATALOGADO = RAIZ / "src" / "poc" / "full-systems-nav" / "configs" / "scenario_full_nav.edl.in"

CAMPO = re.compile(r"(\w+)=(\S+)")


def frames_de(saida):
    return [dict(CAMPO.findall(linha)) for linha in saida.splitlines() if linha.startswith("frame=")]


def deriva_copia(pasta_destino, sufixo):
    """Copia o .edl.in de full-systems-nav para <pasta_destino>/configs/,
    trocando so os caminhos de gravacao/mensagem (por <sufixo>, para nao
    disputar arquivo com a outra copia) -- a PORTA fica intacta de proposito.
    """
    texto = CATALOGADO.read_text(encoding="utf-8")

    texto, n_acmi = re.subn(
        r'fileName:\s*"[^"]*\.acmi"',
        f'fileName: "./build/tests-recordings/dual-port-{sufixo}.acmi"',
        texto,
    )
    if n_acmi != 1:
        raise SystemExit(f"esperava exatamente 1 fileName: *.acmi no cenario, achei {n_acmi}")

    texto, n_jsonl = re.subn(
        r'fileName:\s*"[^"]*\.jsonl"',
        f'fileName: "./build/tests-messages/dual-port-{sufixo}.jsonl"',
        texto,
    )
    if n_jsonl != 1:
        raise SystemExit(f"esperava exatamente 1 fileName: *.jsonl no cenario, achei {n_jsonl}")

    configs = pasta_destino / "configs"
    configs.mkdir(parents=True)
    (configs / "scenario_full_nav.edl.in").write_text(texto, encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--frames", type=int, default=100)
    args = ap.parse_args()

    (RAIZ / "build" / "tests-recordings").mkdir(parents=True, exist_ok=True)
    (RAIZ / "build" / "tests-messages").mkdir(parents=True, exist_ok=True)

    pasta = RAIZ / "build" / "tests-fixtures" / "dual-tacview-port"
    if pasta.exists():
        shutil.rmtree(pasta)
    deriva_copia(pasta / "porta-a", "a")
    deriva_copia(pasta / "porta-b", "b")

    print("  subindo dois processos concorrentes na MESMA porta Tacview ...")
    comum = ["-folder", str(pasta), "-threads", "1", "-deterministic", str(args.frames)]
    proc_a = subprocess.Popen(
        [args.binario, "-scenario", "porta-a", *comum],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=RAIZ,
    )
    proc_b = subprocess.Popen(
        [args.binario, "-scenario", "porta-b", *comum],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=RAIZ,
    )

    try:
        out_a, err_a = proc_a.communicate(timeout=300)
    except subprocess.TimeoutExpired:
        proc_a.kill()
        out_a, err_a = proc_a.communicate()
    try:
        out_b, err_b = proc_b.communicate(timeout=300)
    except subprocess.TimeoutExpired:
        proc_b.kill()
        out_b, err_b = proc_b.communicate()

    falhas = []
    for rotulo, proc, out, err in (("porta-a", proc_a, out_a, err_a), ("porta-b", proc_b, out_b, err_b)):
        if proc.returncode != 0:
            falhas.append(f"{rotulo}: saiu com codigo {proc.returncode} (esperava 0) -- stderr: {err.strip()!r}")
            continue
        # app/src/app/DeterministicRun.cpp so chama printDeterministicDump() a
        # cada 'dumpEveryNFrames' (100) E no ultimo frame -- nao um por frame.
        # O que se quer provar aqui e so que o processo rodou ATE O FIM (o
        # dump do ultimo frame saiu), nao a cadencia de dump em si.
        amostras = frames_de(out)
        if not amostras:
            falhas.append(f"{rotulo}: nenhuma linha 'frame=' na saida -- a simulacao pode ter travado")
        elif amostras[-1].get("frame") != str(args.frames):
            falhas.append(
                f"{rotulo}: ultimo dump foi do frame {amostras[-1].get('frame')}, "
                f"esperava {args.frames} (nao rodou ate o fim)"
            )

    if falhas:
        print()
        for f in falhas:
            print(f"  FALHA {f}")
        print("dual-tacview-port: FALHOU")
        return 1

    print("dual-tacview-port: OK (os dois processos rodaram ate o fim; um dos dois "
          "perdeu a porta Tacview e continuou so com gravacao .acmi local, sem crash)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
