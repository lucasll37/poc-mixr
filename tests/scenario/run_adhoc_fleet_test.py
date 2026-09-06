#!/usr/bin/env python3
"""Prova ponta a ponta do conserto de 'adHocScenario()' (ScenarioCatalog.cpp):
'-f <arquivo>' que aponta para o MESMO arquivo do templatePath de uma entrada
JA CATALOGADA deve reusar essa entrada -- frota inclusive -- em vez de
assumir sempre falcon1..4.

'full-systems-nav' e o caso real: sua entrada no catalogo declara a frota
{"a4"} (um unico player, chamado 'a4', nao 'falcon1..4'). Antes deste
conserto, './app -f <o .edl.in dele>' abortava com "player 'falcon1' nao
encontrado!", porque adHocScenario() nunca consultava o catalogo -- so
'-scenario full-systems-nav' funcionava.

NAO existe um segundo caso "arquivo desconhecido do catalogo deveria
descobrir a frota sozinho": foi tentado (discoverFleet(), descoberta
generica de AirVehicle) e revertido -- quebra o modo 'intruder' de
tests/scenario/make_fixture.py, que acrescenta um bandit1 LOCAL de proposito
FORA da frota rastreada; uma descoberta generica pegava esse bandit1
tambem. Um arquivo de '-f' desconhecido do catalogo continua assumindo
falcon1..4 (o unico caso real que cai nesse fallback hoje sao as fixtures de
teste, que sempre tem essa frota) -- um cenario custom de verdade deve ser
CADASTRADO no catalogo, como 'full-systems-nav' ja e.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
CATALOGADO = RAIZ / "src" / "poc" / "full-systems-nav" / "configs" / "scenario_full_nav.edl.in"

CAMPO = re.compile(r"(\w+)=(\S+)")


def frames_de(saida):
    amostras = []
    for linha in saida.splitlines():
        if not linha.startswith("frame="):
            continue
        amostras.append(dict(CAMPO.findall(linha)))
    return amostras


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--frames", type=int, default=50)
    args = ap.parse_args()

    print("  '-f' apontando para um templatePath ja cadastrado (full-systems-nav) ...")
    proc = subprocess.run(
        [args.binario, "-f", str(CATALOGADO), "-threads", "1", "-deterministic", str(args.frames)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        cwd=RAIZ, timeout=300,
    )

    falhas = []
    if proc.returncode != 0:
        falhas.append(f"binario saiu com codigo {proc.returncode}")
    if "nao encontrado" in proc.stderr:
        falhas.append(f"stderr acusou player nao encontrado -- {proc.stderr.strip()!r}")

    amostras = frames_de(proc.stdout)
    if not amostras:
        falhas.append("nenhuma linha 'frame=' na saida")
    else:
        jogadores = {a.get("player") for a in amostras}
        if jogadores != {"a4"}:
            falhas.append(f"esperava so o player 'a4', vieram {sorted(jogadores)}")

    if falhas:
        print()
        for f in falhas:
            print(f"  FALHA {f}")
        print("adhoc-fleet: FALHOU")
        return 1

    print("adhoc-fleet: OK (frota do catalogo reusada -- 'a4', nao falcon1..4)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
