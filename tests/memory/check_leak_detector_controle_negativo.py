#!/usr/bin/env python3
"""O controle negativo do detector de vazamento.

'memory-<poc>' (run_leak_test.py) prova que a producao de HOJE nao vaza.
Isso nao prova que o detector PEGARIA um vazamento de verdade -- um teste que
so passa nao vale nada ate falhar de proposito (ver tests/README.md).

Esse "falhar de proposito" ja foi feito uma vez, manualmente: um ref() a mais
foi acrescentado em BtBehavior::genAction() (a fabrica de FlightAction), a
suite 'memory' acusou o vazamento (count 2000 -> 4000, proporcional aos
frames), e a mudanca foi revertida. Ficou so a linha na tabela do README --
nada impedia uma regressao futura no PROPRIO detector de passar batido.

Este script torna esse "falhar de proposito" PERMANENTE, sem tocar o binario
de producao: 'model_leak' (models/players/A-4/meson.build, atras da opcao
'variants') e compilada do MESMO fonte de 'libflight.so', com uma UNICA
diferenca -- POC_LEAK_ONE_REF_PER_DECISION acrescenta um action->ref() logo
apos "new FlightAction()" em BtBehavior.cpp, nunca balanceado por um
unref(). O mecanismo de troca de plugin e o MESMO de run_stub_model.py/
run_plugin_hotswap.py: gera a fixture 'intruder' de producao (make_fixture.py)
e troca so o 'file:' do ( PluginModule ).

A assercao usa uma propriedade EXATA do defeito injetado, nao um limiar
arbitrario: como a instancia NUNCA e destruida (o refcount extra impede que
chegue a zero), toda 'FlightAction' ja criada por uma decisao continua viva.
A unica excecao e a instancia UNICA que o proprio registro de plugins cria
(e libera) na CARGA, so para conferir que a fabrica entrega o que o
descritor promete -- ver run_leak_test.py: "o tc de cada classe nasce em 1 e
nao em 0". Fora essa, tc == count + 1 o tempo todo, ao contrario da producao
normal, onde count fica preso em ~PLAYERS enquanto tc cresce sem parar. Se
essa relacao NAO se sustentar, ou se 'count' nao crescer entre as duas
duracoes, o vazamento passou batido -- e e exatamente isso que este script
existe para acusar.
"""

import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]

CURTA = 500
LONGA = 1000
CLASSE = "FlightAction"

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
    return contadores


def main():
    if len(sys.argv) != 3:
        raise SystemExit("uso: check_leak_detector_controle_negativo.py <binario> <so-model-leak>")
    binario, so_leak = sys.argv[1], sys.argv[2]

    if not Path(so_leak).is_file():
        print(f"FALHA: {so_leak} nao existe -- compile models/players/A-4 com "
              "'meson setup -Dvariants=true' (ou 'make models', que ja passa "
              "VARIANTS=true por padrao)")
        return 1

    # Gera a propria fixture, exatamente como run_leak_test.py -- este teste
    # nao pode depender de outro ter rodado antes.
    out = RAIZ / "build" / "tests-fixtures"
    out.mkdir(parents=True, exist_ok=True)
    base = out / "flight-leak-base.edl.in"
    gerar = subprocess.run(
        [sys.executable, str(RAIZ / "tests/scenario/make_fixture.py"),
         "--poc", "flight", "--mode", "intruder", "--out", str(base)],
        cwd=RAIZ,
    )
    if gerar.returncode != 0:
        return 1
    (RAIZ / "build" / "tests-recordings").mkdir(parents=True, exist_ok=True)

    # A UNICA edicao -- mesmo mecanismo de run_stub_model.py/
    # run_plugin_hotswap.py: troca so o 'file:' do ( PluginModule ).
    texto = base.read_text()
    novo, n = re.subn(r'file:\s*"[^"]*"', f'file:     "{so_leak}"', texto, count=1)
    if n != 1:
        print("FALHA: nao achei o 'file:' do ( PluginModule ) na fixture")
        return 1
    fixture = out / "flight-leak.edl.in"
    fixture.write_text(novo)
    cenario = str(fixture)

    print(f"  rodando {CURTA} frames contra 'model_leak' (vaza de proposito) ...")
    curta = roda(binario, cenario, CURTA)
    print(f"  rodando {LONGA} frames contra 'model_leak' (vaza de proposito) ...")
    longa = roda(binario, cenario, LONGA)

    falhas = []

    if CLASSE not in curta or CLASSE not in longa:
        print(f"FALHA: '{CLASSE}' nao apareceu no relatorio 'meta=' -- "
              "o .so certo foi carregado? (esperava ver '[plugin] carregado' na saida)")
        return 1

    c, l = curta[CLASSE], longa[CLASSE]
    print()
    print(f"  {'':6} {'count':>8} {'mc':>8} {'tc':>8}")
    print(f"  {CURTA:<6} {c['count']:>8} {c['mc']:>8} {c['tc']:>8}")
    print(f"  {LONGA:<6} {l['count']:>8} {l['mc']:>8} {l['tc']:>8}")
    print()

    # 1) A propria assercao que 'memory-<poc>' faz para provar ausencia de
    #    vazamento -- aqui espera-se que ELA FALHE. Se count ficasse igual
    #    entre as duas duracoes, o detector teria dado um FALSO NEGATIVO
    #    diante de um vazamento real.
    if l["count"] <= c["count"]:
        falhas.append(
            f"{CLASSE}.count NAO cresceu entre {CURTA} e {LONGA} frames "
            f"({c['count']} -> {l['count']}) -- o vazamento injetado passou "
            "batido pelo detector"
        )

    # 2) A assinatura EXATA deste defeito: nada e destruido a partir de uma
    #    decisao real, entao tc == count + 1 nas DUAS duracoes -- o "+1" e a
    #    UNICA instancia que morre de verdade, a do probe de carga do
    #    proprio registro de plugins (ver run_leak_test.py). Uma relacao
    #    EXATA (e nao um "cresceu um pouco") evita que um crescimento
    #    incidental e pequeno (ex.: ordem de inicializacao) seja confundido
    #    com deteccao.
    for rotulo, snapshot in (("curta", c), ("longa", l)):
        if snapshot["tc"] - snapshot["count"] != 1:
            falhas.append(
                f"{CLASSE}.tc ({snapshot['tc']}) - {CLASSE}.count ({snapshot['count']}) "
                f"= {snapshot['tc'] - snapshot['count']} na corrida {rotulo}, esperava 1 "
                "(so a instancia do probe de carga morre de verdade); ou o ref() extra "
                "nao esta mais la, ou algo mudou no ciclo de vida de FlightAction"
            )

    # 3) 'mc' (pico simultaneo) e so um espelho de 'count' quando ninguem
    #    morre -- confirma que o pico observado bate com o nivel atual, nao
    #    com um transiente isolado.
    if l["mc"] != l["count"]:
        falhas.append(
            f"{CLASSE}.mc ({l['mc']}) != {CLASSE}.count ({l['count']}) na corrida longa"
        )

    if falhas:
        for f in falhas:
            print(f"  FALHA {f}")
        print("controle negativo: FALHOU -- o detector NAO pegaria um vazamento real")
        return 1

    print(f"controle negativo: OK -- o vazamento injetado FOI detectado "
          f"({CLASSE}.count cresceu {l['count'] - c['count']} entre {CURTA} e "
          f"{LONGA} frames, e tc==count+1 confirma que nenhuma decisao real "
          "liberou a propria acao)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
