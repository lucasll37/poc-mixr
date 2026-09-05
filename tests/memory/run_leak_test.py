#!/usr/bin/env python3
"""Deteccao de vazamento pelo metadado de instancias do proprio MIXR.

Le as linhas 'meta=<classe> count=<n> mc=<n> tc=<n>' que app/MetaObjectReport
imprime no fim de uma corrida deterministica (ver o cabecalho daquele header
para a mecanica dos contadores).

A assercao e COMPARATIVA, e nao um retrato unico -- e o que distingue
"vazando" de "legitimamente retido":

  * 'count' tem de ser IGUAL nas duas duracoes: nada acumula com o tempo.
  * 'tc' da classe alocada por frame tem de CRESCER na proporcao dos frames.
    Sem essa segunda metade o teste passaria por inercia: uma classe que
    nunca e instanciada tem count estavel em zero para sempre.
  * 'mc' (pico simultaneo) tem de ficar pequeno. Um count final zerado com
    pico da ordem dos frames ainda e retencao -- so que liberada no fim.

Roda contra uma fixture HERMETICA (sem 'networks:'): com o bloco de rede o
processo ingere PDUs DIS de quem estiver na LAN, e duas execucoes iguais dao
contagens diferentes -- observado com um bandit de outra sessao no ar.

Roda com '-threads 1' de proposito: os contadores do framework nao sao
atomicos (macros.hpp:249), entao com os agentes decidindo em paralelo no pool
de tempo critico o numero perde exatidao.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]

CURTA = 500
LONGA = 1000
PLAYERS = 4

# Classes criadas uma unica vez, no parse do EDL: uma por aviao.
UMA_POR_AVIAO = (
    "FlightState",
    "BtBehavior",
    "AltitudeSafetyBehavior",
    "AlertDatalink",
)

# NOTA: TODAS as classes de UMA_POR_AVIAO e POR_DECISAO vem de um PLUGIN --
# domain/, bt/, ubf/ e xnative/ nao estao mais dentro do executavel. Elas
# aparecem no relatorio porque o descritor do plugin exporta os MetaObject
# delas (campo 'metas' de PluginDescV1) e app/MetaObjectReport imprime no MESMO
# formato 'meta='. Este script nao precisa saber de nada disso -- e exatamente
# essa a prova de que a fronteira ficou no lugar certo.
#
# Um detalhe medido, e que este teste tolera de proposito: o 'tc' de cada classe
# nasce em 1 e nao em 0, porque o registro de plugins constroi uma instancia de
# cada nome declarado na CARGA, para conferir que a fabrica entrega o que o
# descritor promete. O 'count' volta a zero, entao nada aqui e afetado.

# Alocada a cada ciclo de decisao, por aviao. E a classe mais exposta a
# vazamento e a unica de que se exige crescimento de 'tc'.
POR_DECISAO = "FlightAction"

LINHA = re.compile(
    r"^meta=(?P<classe>\S+)\s+count=(?P<count>-?\d+)\s+mc=(?P<mc>-?\d+)\s+tc=(?P<tc>-?\d+)\s*$"
)


def roda(binario, cenario, frames):
    proc = subprocess.run(
        [binario, "-f", cenario, "-threads", "1", "-deterministic", str(frames)],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=1800,
    )
    if proc.returncode != 0:
        raise SystemExit(f"FALHA: {binario} saiu com codigo {proc.returncode}")

    contadores = {}
    for linha in proc.stdout.splitlines():
        m = LINHA.match(linha)
        if m:
            contadores[m.group("classe")] = {
                "count": int(m.group("count")),
                "mc": int(m.group("mc")),
                "tc": int(m.group("tc")),
            }
    if not contadores:
        raise SystemExit(
            f"FALHA: {binario} nao imprimiu nenhuma linha 'meta=' "
            "(app::printMetaObjectReport() foi chamado?)"
        )
    return contadores, ultimo_states(cenario)


def ultimo_states(cenario):
    """Blocos de estado por player vivos no MsgFeed, da ultima msgHealth.

    Essa estrutura fica FORA do ref-counting do MIXR, entao o relatorio 'meta='
    nao a enxerga: um mapa que crescesse a cada player visto passaria batido
    pelos contadores de instancia. E o unico vazamento plausivel do shared/xmsg
    que o resto deste teste nao pegaria.
    """
    nome = Path(cenario).name.replace(".edl.in", ".jsonl")
    caminho = RAIZ / "build" / "tests-messages" / nome
    if not caminho.exists():
        return None

    visto = None
    for linha in caminho.read_text(encoding="utf-8").splitlines():
        linha = linha.strip()
        if not linha or "msgHealth" not in linha:
            continue
        try:
            visto = json.loads(linha).get("states")
        except json.JSONDecodeError:
            return None
    return visto


def main():
    if len(sys.argv) != 3:
        raise SystemExit("uso: run_leak_test.py <binario> <poc>")
    binario, poc = sys.argv[1], sys.argv[2]

    # Gera a propria fixture: o teste nao pode depender de outro ter rodado antes.
    cenario = RAIZ / "build" / "tests-fixtures" / f"{poc}-intruder.edl.in"
    gerar = subprocess.run(
        [sys.executable, str(RAIZ / "tests/scenario/make_fixture.py"),
         "--poc", poc, "--mode", "intruder", "--out", str(cenario)],
        cwd=RAIZ, text=True,
    )
    if gerar.returncode != 0:
        return 1
    (RAIZ / "build" / "tests-recordings").mkdir(parents=True, exist_ok=True)
    cenario = str(cenario)

    print(f"  rodando {CURTA} frames com -threads 1 ...")
    curta, states_curta = roda(binario, cenario, CURTA)
    print(f"  rodando {LONGA} frames com -threads 1 ...")
    longa, states_longa = roda(binario, cenario, LONGA)

    falhas = []

    # 1) nada pode acumular com o tempo
    for classe, c in curta.items():
        if classe not in longa:
            falhas.append(f"{classe}: sumiu do relatorio na corrida longa")
            continue
        l = longa[classe]
        if c["count"] != l["count"]:
            falhas.append(
                f"{classe}: instancias vivas crescem com a duracao "
                f"({c['count']} em {CURTA} frames -> {l['count']} em {LONGA}) -- VAZAMENTO"
            )

    # 2) as de criacao unica: uma por aviao, parada
    for classe in UMA_POR_AVIAO:
        if classe not in longa:
            falhas.append(f"{classe}: ausente do relatorio")
            continue
        if longa[classe]["count"] != PLAYERS:
            falhas.append(
                f"{classe}: count={longa[classe]['count']}, esperado {PLAYERS} (uma por aviao)"
            )

    # 3) a classe por decisao: prova de vida + limite de pico
    if POR_DECISAO not in longa:
        falhas.append(f"{POR_DECISAO}: ausente do relatorio")
    else:
        tc_c, tc_l = curta[POR_DECISAO]["tc"], longa[POR_DECISAO]["tc"]
        if tc_c <= 0:
            falhas.append(
                f"{POR_DECISAO}: tc=0 -- nenhuma instancia criada; o teste passaria por inercia"
            )
        else:
            razao = tc_l / tc_c
            esperado = LONGA / CURTA
            if abs(razao - esperado) > 0.15:
                falhas.append(
                    f"{POR_DECISAO}: tc cresceu {razao:.2f}x entre {CURTA} e {LONGA} frames, "
                    f"esperado ~{esperado:.2f}x -- a decisao nao esta amarrada ao frame"
                )
        pico = longa[POR_DECISAO]["mc"]
        if pico > 4 * PLAYERS:
            falhas.append(
                f"{POR_DECISAO}: pico simultaneo mc={pico} alto demais -- ha retencao "
                "durante a corrida, mesmo que o count final feche"
            )

    # 4) o estado por player do MsgFeed, que o 'meta=' nao alcanca
    if states_curta is None or states_longa is None:
        print("  (sem msgHealth no .jsonl -- checagem de 'states' pulada)")
    elif states_curta != states_longa:
        falhas.append(
            f"MsgFeed: blocos de estado por player crescem com a duracao "
            f"({states_curta} em {CURTA} frames -> {states_longa} em {LONGA}) -- VAZAMENTO"
        )
    else:
        print(f"  MsgFeed: {states_longa} blocos de estado, estaveis entre as duas duracoes")

    largura = max(len(c) for c in longa)
    print()
    print(f"  {'classe':<{largura}}  {'count':>6} {'mc':>6} {'tc(' + str(CURTA) + ')':>10} {'tc(' + str(LONGA) + ')':>10}")
    for classe in longa:
        print(f"  {classe:<{largura}}  {longa[classe]['count']:>6} {longa[classe]['mc']:>6} "
              f"{curta.get(classe, {}).get('tc', 0):>10} {longa[classe]['tc']:>10}")
    print()

    if falhas:
        for f in falhas:
            print(f"  FALHA {f}")
        print("vazamento: FALHOU")
        return 1

    print("vazamento: OK (instancias vivas estaveis, criacao/destruicao em regime)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
