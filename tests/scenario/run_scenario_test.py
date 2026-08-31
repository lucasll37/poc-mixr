#!/usr/bin/env python3
"""Cenario ponta a ponta: roda o binario de verdade e afirma COMPORTAMENTO.

As asserções sao semanticas, sobre os campos das linhas 'frame=' -- e nao
comparacao byte a byte. Um golden exato quebraria por ruido de ponto
flutuante entre maquinas e diria apenas "os arquivos diferem"; aqui a falha
diz qual propriedade do modelo caiu.

Cada modo exercita um ramo diferente da arvore:

  intruder  a cadeia inteira -- falcon1 detecta, evade e avisa; as outras
            recebem e vao apoiar. E o unico modo que fecha o laco entre
            players (AlertDatalink), e por isso o mais valioso.
  lowfuel   o ramo de RTB vence tudo, do primeiro frame ao ultimo.
  terrain   o AltitudeSafetyBehavior (voto 90) ganha do BtBehavior (voto 50).

Todos rodam contra fixtures HERMETICAS geradas por make_fixture.py.
"""

import argparse
import json
import math
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]

ROTULOS_CONHECIDOS = {"PATROL", "EVADE", "BREAK", "SUPPORT", "RTB", "HOME", "SAFETY", "?"}

CAMPO = re.compile(r"(\w+)=(\S+)")


def parse_dump(saida):
    """Linhas 'frame=' -> lista de dicionarios, na ordem em que sairam."""
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


# ------------------------------------------------------------------------------
# Propriedades comuns a qualquer cenario
# ------------------------------------------------------------------------------

def checa_comuns(amostras, falhas):
    if not amostras:
        falhas.append("nenhuma linha 'frame=' na saida")
        return

    players = por_player(amostras)
    if len(players) != 4:
        falhas.append(f"esperava 4 falcons no dump, vieram {len(players)}: {sorted(players)}")

    for nome, série in players.items():
        rotulos = {a["bt"] for a in série}
        desconhecidos = rotulos - ROTULOS_CONHECIDOS
        if desconhecidos:
            falhas.append(f"{nome}: rotulo de comportamento desconhecido {desconhecidos}")

        # CFIT: o Player nativo dispara CRASH_EVENT com AGL < 0 e ai congela
        # (setMode(CRASHED)) -- para de decidir em silencio.
        minimo = min(a["agl"] for a in série)
        if minimo <= 0.0:
            falhas.append(f"{nome}: AGL chegou a {minimo:.1f} m -- voou para dentro do terreno")

        # Uma aeronave congelada mantem a posicao: prova de que ninguem parou.
        primeiro, ultimo = série[0], série[-1]
        andou = abs(ultimo["n"] - primeiro["n"]) + abs(ultimo["e"] - primeiro["e"])
        if andou < 1.0:
            falhas.append(f"{nome}: nao saiu do lugar em {len(série)} amostras -- congelou?")


# ------------------------------------------------------------------------------
# Propriedades por modo
# ------------------------------------------------------------------------------

def checa_intruder(amostras, falhas):
    players = por_player(amostras)

    evadiram = {n for n, s in players.items() if any(a["bt"] in ("EVADE", "BREAK") for a in s)}
    if not evadiram:
        falhas.append("ninguem evadiu: o bandit1 local nao foi detectado pelo radar")

    apoiaram = {n for n, s in players.items() if any(a["bt"] == "SUPPORT" for a in s)}
    if not apoiaram:
        falhas.append("ninguem apoiou: o alerta nao chegou aos outros avioes")

    # quem evade transmite; quem apoia recebe. E o laco entre players.
    for nome in evadiram:
        if max(a["sent"] for a in players[nome]) <= 0:
            falhas.append(f"{nome}: evadiu mas nao transmitiu alerta nenhum")
    for nome in apoiaram:
        if max(a["recv"] for a in players[nome]) <= 0:
            falhas.append(f"{nome}: apoiou sem ter recebido mensagem")
        if a_ultimo(players[nome])["alert"] == "none" and \
           all(a["alert"] == "none" for a in players[nome]):
            falhas.append(f"{nome}: apoiou sem nunca registrar um remetente de alerta")

    # quem apoia nao pode ser quem viu (senao nao ha interacao nenhuma)
    if evadiram and apoiaram and evadiram == apoiaram and len(players) > 1:
        falhas.append("evasao e apoio no mesmo conjunto de avioes -- nao houve coordenacao")

    # pista coerente: quem evade tem de ter visto o bandit1
    for nome in evadiram:
        pistas = {a["track"] for a in players[nome] if a["track"] != "none"}
        if pistas and "bandit1" not in pistas:
            falhas.append(f"{nome}: evadiu de {pistas}, esperado bandit1")


def a_ultimo(série):
    return série[-1]


def checa_lowfuel(amostras, falhas):
    for nome, série in por_player(amostras).items():
        fora = {a["bt"] for a in série} - {"RTB", "HOME", "SAFETY"}
        if fora:
            falhas.append(
                f"{nome}: com a reserva acima do combustivel disponivel, o ramo de RTB tinha de "
                f"vencer sempre; apareceu {sorted(fora)}"
            )


def checa_terrain(amostras, falhas):
    viram_safety = {n for n, s in por_player(amostras).items()
                    if any(a["bt"] == "SAFETY" for a in s)}
    if not viram_safety:
        falhas.append(
            "nenhum aviao entrou em SAFETY, com minAltitude acima da altitude de cruzeiro -- "
            "o AltitudeSafetyBehavior (voto 90) nao esta ganhando do BtBehavior (voto 50)"
        )


CHECAGENS = {
    "intruder": checa_intruder,
    "lowfuel": checa_lowfuel,
    "terrain": checa_terrain,
}


# ------------------------------------------------------------------------------
# O canal de mensagens (shared/xmsg) -- grava por fora do stdout
# ------------------------------------------------------------------------------

def checa_mensagens(poc, mode, falhas):
    """Asserçoes semanticas sobre o .jsonl, nao comparacao byte a byte.

    Um golden exato quebraria por ruido de ponto flutuante entre maquinas e
    diria apenas "os arquivos diferem". Aqui a falha diz qual propriedade caiu.
    """
    caminho = RAIZ / "build" / "tests-messages" / f"{poc}-{mode}.jsonl"
    if not caminho.exists():
        falhas.append(f"o canal de mensagens nao gravou {caminho.name}")
        return

    registros = []
    for n, linha in enumerate(caminho.read_text(encoding="utf-8").splitlines(), 1):
        linha = linha.strip()
        if not linha:
            continue
        try:
            registros.append(json.loads(linha))
        except json.JSONDecodeError as e:
            # NaN/inf sairiam como 'nan' e nenhum leitor de JSON aceitaria --
            # por isso o RecordWriter os emite como null.
            falhas.append(f"{caminho.name}:{n} nao e JSON valido ({e})")
            return

    if not registros:
        falhas.append(f"{caminho.name} esta vazio -- nenhuma mensagem foi emitida")
        return

    # 1) tempo simulado, monotonico e sem relogio de parede
    ts = [r.get("t") for r in registros]
    if any(t is None for t in ts):
        falhas.append("ha registro sem campo 't'")
    elif any(b < a for a, b in zip(ts, ts[1:])):
        falhas.append("o campo 't' nao e monotonico -- entrou tempo de parede?")

    # 2) nenhum numero degenerado escapou como texto
    for r in registros:
        for k, v in r.items():
            if isinstance(v, float) and not math.isfinite(v):
                falhas.append(f"campo '{k}' saiu como {v} em vez de null")
                break

    # 3) a telemetria periodica tem de cobrir as quatro falcons
    tele = {r.get("player") for r in registros if r.get("msg") == "telemetria"}
    for n in ("falcon1", "falcon2", "falcon3", "falcon4"):
        if n not in tele:
            falhas.append(f"{n} nao aparece na telemetria")

    # 4) o deadband da mudanca de altitude, ponta a ponta: dois eventos
    #    consecutivos do MESMO player tem de diferir pelo menos o 'by:' (100 m).
    #    E a prova de que a comparacao e contra o ultimo EMITIDO, e nao contra a
    #    amostra anterior -- senao sairia um evento por ciclo.
    ultimo = {}
    for r in registros:
        if r.get("msg") != "mudanca-altitude":
            continue
        p, alt = r.get("player"), r.get("altMslM")
        if p is None or alt is None:
            continue
        if p in ultimo and abs(alt - ultimo[p]) < 99.0:
            falhas.append(
                f"{p}: dois eventos de mudanca de altitude a {abs(alt - ultimo[p]):.1f} m "
                "um do outro -- o deadband nao esta medindo contra o ultimo emitido"
            )
            break
        ultimo[p] = alt

    # 5) a mensagem de saude nao pode estar escondendo falha
    saude = [r for r in registros if r.get("msg") == "msgHealth"]
    if saude:
        s = saude[-1]
        if s.get("sendFailed", 0):
            falhas.append(f"msgHealth reporta sendFailed={s['sendFailed']}")
        if s.get("overflow", 0):
            falhas.append(f"msgHealth reporta overflow={s['overflow']} (linha JSON truncada)")
        if s.get("overCap", 0):
            falhas.append(f"msgHealth reporta overCap={s['overCap']} (maxPlayers estourado)")

    print(f"    mensagens: {len(registros)} registros, "
          f"{len({r.get('msg') for r in registros})} tipos, "
          f"{len({r.get('player') for r in registros if r.get('player')})} players")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--poc", required=True)
    ap.add_argument("--mode", required=True, choices=tuple(CHECAGENS))
    ap.add_argument("--frames", type=int, default=800)
    args = ap.parse_args()

    fixture = RAIZ / "build" / "tests-fixtures" / f"{args.poc}-{args.mode}.epp.in"
    gerar = subprocess.run(
        [sys.executable, str(RAIZ / "tests/scenario/make_fixture.py"),
         "--poc", args.poc, "--mode", args.mode, "--out", str(fixture)],
        cwd=RAIZ, text=True,
    )
    if gerar.returncode != 0:
        return 1

    (RAIZ / "build" / "tests-recordings").mkdir(parents=True, exist_ok=True)

    print(f"  rodando {args.poc} / {args.mode} por {args.frames} frames ...")
    proc = subprocess.run(
        [args.binario, "-f", str(fixture), "-threads", "1",
         "-deterministic", str(args.frames)],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
        cwd=RAIZ, timeout=1800,
    )
    if proc.returncode != 0:
        print(f"  FALHA o binario saiu com codigo {proc.returncode}")
        return 1

    amostras = parse_dump(proc.stdout)
    falhas = []
    checa_comuns(amostras, falhas)
    CHECAGENS[args.mode](amostras, falhas)
    checa_mensagens(args.poc, args.mode, falhas)

    players = por_player(amostras)
    print(f"  {len(amostras)} amostras, {len(players)} players")
    for nome, série in sorted(players.items()):
        rotulos = []
        for a in série:
            if not rotulos or rotulos[-1] != a["bt"]:
                rotulos.append(a["bt"])
        print(f"    {nome}: {' -> '.join(rotulos)}")

    if falhas:
        print()
        for f in falhas:
            print(f"  FALHA {f}")
        print(f"cenario {args.mode}: FALHOU")
        return 1

    print(f"cenario {args.mode}: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
