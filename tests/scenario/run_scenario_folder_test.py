#!/usr/bin/env python3
"""Prova ponta a ponta de './app -folder <pasta>' (a navegacao por pasta de
cenarios de sandbox, ver app/ScenarioFolder.hpp).

Monta uma pasta temporaria com DUAS subpastas -- uma valida (uma copia do
'.edl.in' de 'full-systems-nav', cenario real de um player so, chamado
'a4') e uma deliberadamente malformada ('configs/' vazia) -- e roda o
caminho NAO-INTERATIVO ('-folder <pasta> -scenario <nome-da-subpasta>'),
que pula a tela de navegacao. Isso exercita, de ponta a ponta:

  1. a descoberta em disco (discoverFolderScenarios());
  2. a selecao direta por nome de subpasta, sem UI;
  3. app::discoverFleet() -- a frota GENERICA (nao falcon1..4) que
     'ScenarioEntry::fleet' vazio aciona em main.cpp para cenarios de
     '-folder' (ver o comentario em app/Fleet.hpp).

A subpasta malformada prova que uma pasta 'irma' invalida nao impede a
pasta valida de ser encontrada (aviso em stderr, sem abortar o processo
inteiro).
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

    pasta = RAIZ / "build" / "tests-fixtures" / "scenario-folder"
    if pasta.exists():
        shutil.rmtree(pasta)

    valido = pasta / "voo-de-teste" / "configs"
    valido.mkdir(parents=True)
    shutil.copyfile(CATALOGADO, valido / "scenario_full_nav.edl.in")

    # Subpasta IRMA malformada -- 'configs/' existe mas esta vazia. Prova
    # que discoverFolderScenarios() so pula ELA (com aviso), sem impedir a
    # subpasta valida de ser encontrada.
    (pasta / "malformado" / "configs").mkdir(parents=True)

    print("  '-folder' + '-scenario <subpasta>' (selecao direta, sem UI) ...")
    proc = subprocess.run(
        [args.binario, "-folder", str(pasta), "-scenario", "voo-de-teste",
         "-threads", "1", "-deterministic", str(args.frames)],
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
            falhas.append(f"esperava so o player 'a4' (frota descoberta em runtime), vieram {sorted(jogadores)}")

    if falhas:
        print()
        for f in falhas:
            print(f"  FALHA {f}")
        print("scenario-folder: FALHOU")
        return 1

    print("scenario-folder: OK (subpasta valida encontrada, malformada ignorada, frota generica)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
