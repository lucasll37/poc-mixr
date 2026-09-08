#!/usr/bin/env python3
"""Prova ponta a ponta de './app -folder <pasta>' (a navegacao por pasta de
cenarios de sandbox, ver app/ScenarioFolder.hpp).

Monta uma pasta temporaria com TRES subpastas -- uma valida (uma copia do
'.edl.in' de 'full-systems-nav', cenario real de um player so, chamado
'a4'), uma deliberadamente malformada ('configs/' vazia) e uma valida mas
com ZERO players (so 'Station'/'WorldModel') -- e roda o caminho
NAO-INTERATIVO ('-folder <pasta> -scenario <nome-da-subpasta>'), que pula
a tela de navegacao. Isso exercita, de ponta a ponta:

  1. a descoberta em disco (discoverFolderScenarios());
  2. a selecao direta por nome de subpasta, sem UI;
  3. app::discoverFleet() -- a frota GENERICA (nao falcon1..4) que
     'ScenarioEntry::fleet' vazio aciona em main.cpp para cenarios de
     '-folder' (ver o comentario em app/Fleet.hpp);
  4. a DEGRADACAO GRACIOSA com zero players -- caso ja robusto no codigo
     (Fleet vazia, printDeterministicDump() nunca entra no laco) mas sem
     nenhuma regressao automatizada ate este teste (achado por auditoria:
     'BURACO: cenario com ZERO players', workflow de investigacao desta
     sessao). '-f' bare NAO serve para provar isso -- ele sempre assume a
     frota falcon1..4 fixa (app::adHocScenario()) e abortaria antes de
     rodar um frame sequer; só '-folder' (descoberta em runtime) alcança
     o caminho zero-players de verdade.

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

    # Subpasta ZERO-PLAYERS -- so Station/WorldModel, 'players: { }' vazio.
    # Gerada aqui, nao versionada em lugar nenhum: mesma filosofia de
    # 'fixtures sao DERIVADAS, nao copias estaticas' que o resto da suite
    # 'scenario' ja segue (ver make_fixture.py). Sem 'terrain:'/'dataRecorder:'
    # de proposito -- nenhum dos dois e obrigatorio para o parser aceitar o
    # arquivo, e o teste quer o caso MINIMO, nao um cenario hermetico completo.
    zero = pasta / "zero-players" / "configs"
    zero.mkdir(parents=True)
    (zero / "scenario_zero_players.edl.in").write_text(
        "// cenario minimo -- prova que o app roda sem nenhum player quando\n"
        "// carregado via -folder (frota descoberta em runtime, vazia aqui).\n"
        "( Station\n"
        "   simulation: ( WorldModel\n"
        "      numTcThreads: @NUM_TC_THREADS@\n"
        "      players: { }\n"
        "   )\n"
        ")\n"
    )

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

    print("  '-folder' + '-scenario zero-players' (Station sem nenhum player) ...")
    proc_zero = subprocess.run(
        [args.binario, "-folder", str(pasta), "-scenario", "zero-players",
         "-threads", "1", "-deterministic", str(args.frames)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        cwd=RAIZ, timeout=300,
    )
    if proc_zero.returncode != 0:
        falhas.append(
            f"zero-players: binario saiu com codigo {proc_zero.returncode} "
            f"(esperava degradacao graciosa, exit 0) -- stderr: {proc_zero.stderr.strip()!r}"
        )
    if frames_de(proc_zero.stdout):
        falhas.append("zero-players: saiu linha 'frame=' com player, mas a Fleet deveria estar vazia")

    if falhas:
        print()
        for f in falhas:
            print(f"  FALHA {f}")
        print("scenario-folder: FALHOU")
        return 1

    print("scenario-folder: OK (subpasta valida encontrada, malformada ignorada, "
          "frota generica, zero-players degrada com exit 0)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
