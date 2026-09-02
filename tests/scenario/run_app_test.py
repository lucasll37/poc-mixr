#!/usr/bin/env python3
"""Cenario ponta a ponta do dashboard -- roda '-deterministic' (sem TUI, sem
TTY) e afirma comportamento basico sobre as linhas 'frame='.

Ao contrario de run_scenario_test.py (single-thread/multi-thread), o
dashboard NAO precisa de fixture nenhuma: seus tres cenarios (ver
app/ScenarioCatalog.hpp) ja sao herméticos por si so, escolhidos por
'-scenario <chave>' em vez de '-f <arquivo>'. Por isso este script e mais
simples -- so confere que os .epp.in carregam e rodam sao, sem tentar provar
de novo o mecanismo de determinismo 1/2/4-threads (isso ja e nativo, e as
outras pocs ja provam onde importa).
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]

ROTULOS_CONHECIDOS = {"PATROL", "EVADE", "BREAK", "SUPPORT", "RTB", "HOME", "SAFETY", "LAUNCH", "?"}

CAMPO = re.compile(r"(\w+)=(\S+)")

# bandit1 NUNCA aparece no dump -- app/DeterministicDump.cpp so imprime a
# Fleet (playerNames = falcon1..4 em app/main.cpp), o mesmo em toda poc deste
# repositorio (nem o modo 'intruder' de single-thread/multi-thread mostra
# bandit1 no dump, so os quatro falcons -- ver run_scenario_test.py).
JOGADORES_POR_CENARIO = {
    "patrol": {"falcon1", "falcon2", "falcon3", "falcon4"},
    "intercept": {"falcon1", "falcon2", "falcon3", "falcon4"},
    "intercept_missile": {"falcon1", "falcon2", "falcon3", "falcon4"},
}


def parse_dump(saida):
    amostras = []
    for linha in saida.splitlines():
        if not linha.startswith("frame="):
            continue
        d = dict(CAMPO.findall(linha))
        d["frame"] = int(d["frame"])
        for k in ("n", "e", "alt", "agl", "elev", "hdg", "spd", "fuel", "trackRange"):
            if k in d:
                d[k] = float(d[k])
        for k in ("sent", "recv", "dec"):
            if k in d:
                d[k] = int(d[k])
        amostras.append(d)
    return amostras


def por_player(amostras):
    saida = {}
    for a in amostras:
        saida.setdefault(a["player"], []).append(a)
    return saida


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--scenario", required=True, choices=tuple(JOGADORES_POR_CENARIO))
    ap.add_argument("--frames", type=int, default=300)
    args = ap.parse_args()

    esperados = JOGADORES_POR_CENARIO[args.scenario]

    print(f"  rodando app / {args.scenario} por {args.frames} frames ...")
    proc = subprocess.run(
        [args.binario, "-scenario", args.scenario, "-threads", "1",
         "-deterministic", str(args.frames)],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
        cwd=RAIZ, timeout=900,
    )
    if proc.returncode != 0:
        print(f"  FALHA o binario saiu com codigo {proc.returncode}")
        return 1

    amostras = parse_dump(proc.stdout)
    falhas = []

    if not amostras:
        falhas.append("nenhuma linha 'frame=' na saida")

    players = por_player(amostras)
    if set(players) != esperados:
        falhas.append(f"esperava os players {sorted(esperados)}, vieram {sorted(players)}")

    for nome, serie in players.items():
        rotulos = {a["bt"] for a in serie}
        desconhecidos = rotulos - ROTULOS_CONHECIDOS
        if desconhecidos:
            falhas.append(f"{nome}: rotulo de comportamento desconhecido {desconhecidos}")

        minimo = min(a["agl"] for a in serie)
        if minimo <= 0.0:
            falhas.append(f"{nome}: AGL chegou a {minimo:.1f} m -- voou para dentro do terreno")

        primeiro, ultimo = serie[0], serie[-1]
        andou = abs(ultimo["n"] - primeiro["n"]) + abs(ultimo["e"] - primeiro["e"])
        if andou < 1.0:
            falhas.append(f"{nome}: nao saiu do lugar em {len(serie)} amostras -- congelou?")

    print(f"  {len(amostras)} amostras, {len(players)} players: {sorted(players)}")

    if falhas:
        print()
        for f in falhas:
            print(f"  FALHA {f}")
        print(f"app/{args.scenario}: FALHOU")
        return 1

    print(f"app/{args.scenario}: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
