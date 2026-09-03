#!/usr/bin/env python3
"""Deriva um cenario de TESTE do cenario de PRODUCAO.

Por que derivar em vez de versionar uma copia: uma copia comecaria correta e
envelheceria em silencio -- mexer no scenario.epp.in nao quebraria teste
nenhum, que e exatamente o oposto do que se quer. Aqui a fixture e sempre o
cenario de verdade com um delta pequeno e explicito.

Todas as fixtures sao HERMETICAS: o bloco 'networks:' sai. Sem isso o
processo abre a porta DIS 3000 e ingere o que estiver na rede -- observado na
pratica, com um bandit-dis de outra sessao mudando o resultado de duas
execucoes identicas (TacviewOutput/DIS nao fazem parte do que estes testes
querem medir). O Tacview tambem e desviado de porta e de arquivo, para nao
disputar a 1234 com uma poc que esteja rodando de verdade.

Modos:

  intruder  reintroduz o bandit1 LOCAL. Sem ele nao ha como exercitar
            EVADE/SUPPORT num processo so, porque o intruso hoje mora em
            src/poc/bandit-dis e chega apenas por DIS.
  lowfuel   sobe 'fuelReserve' acima da fracao real de combustivel, pelo slot
            que ja existe: o ramo de RTB vence desde o primeiro frame.
  terrain   sobe 'minAltitude' do AltitudeSafetyBehavior acima da altitude de
            cruzeiro: o comportamento de voto 90 assume e a arvore perde.
"""

import argparse
import re
import sys
from pathlib import Path

# O intruso, identico ao de src/poc/bandit-dis/configs/scenario.epp, so que
# local. O rootDir do jsbsim e o mesmo em qualquer poc -- e dado do MODELO
# (models/flight/data/jsbsim), instalado uma unica vez em dist/.
BANDIT = """
         // ------------------------------------------------------------------
         // FIXTURE DE TESTE -- bandit1 LOCAL.
         //
         // Em producao o intruso vive no processo src/poc/bandit-dis e chega por
         // DIS. Num teste de processo unico isso tornaria EVADE e SUPPORT
         // inalcancaveis, entao ele volta para ca, com os mesmos valores
         // iniciais do lado de la.
         // ------------------------------------------------------------------
         bandit1: ( Aircraft
            id: 201
            side: red
            type: "C310"
            signature: ( SigSphere radius: 4.0 )
            initXPos:     ( NauticalMiles 12.0 )
            initYPos:     ( NauticalMiles 12.0 )
            initAlt:      ( Meters 2400.0 )
            initHeading:  ( Degrees 225 )
            initVelocity: 92.0
            dataLogTime: ( Seconds 0.1 )
            interpolateTerrain: true
            components: {
               dynamicsModel: ( JSBSimModel
                  rootDir: "./dist/share/mixr-plugins/flight/jsbsim/"  model: "c310"  debugLevel: 0
               )
               pilot: ( Autopilot
                  navMode: false  headingHoldMode: true  altitudeHoldMode: true
                  velocityHoldMode: true  maxRateOfTurnDps: 3.0  maxBankAngle: 30.0
                  maxPitchAngle: 10.0  maxClimbRateMps: 8.0  maxAcceleration: 2.0
               )
            }
         )
"""


def remove_block(texto, marcador):
    """Remove 'marcador { ... }' casando as chaves."""
    i = texto.find(marcador)
    if i < 0:
        raise SystemExit(f"marcador nao encontrado no cenario: {marcador!r}")

    abre = texto.index("{", i)
    nivel = 0
    for j in range(abre, len(texto)):
        if texto[j] == "{":
            nivel += 1
        elif texto[j] == "}":
            nivel -= 1
            if nivel == 0:
                return texto[:i] + texto[j + 1:]
    raise SystemExit(f"bloco {marcador!r} nao fecha")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--poc", required=True, help="single-thread ou multi-thread")
    ap.add_argument("--mode", required=True, choices=("intruder", "lowfuel", "terrain"))
    ap.add_argument("--out", required=True, help="caminho do .epp.in a gerar")
    args = ap.parse_args()

    raiz = Path(__file__).resolve().parents[2]
    origem = raiz / "src" / "poc" / args.poc / "configs" / "scenario.epp.in"
    texto = origem.read_text(encoding="utf-8")

    # 1) hermetico: fora a rede
    texto = remove_block(texto, "networks: {")

    # 2) Desviar os arquivos de saida para dentro de build/, cada um para o seu.
    #
    # ATENCAO: estas substituicoes sao ANCORADAS NA EXTENSAO de proposito. A
    # versao anterior era um re.sub global em 'fileName:\s*"[^"]*"', e so
    # funcionava porque havia exatamente UM fileName: no cenario (o do
    # TacviewOutput). No instante em que o MsgFileSink acrescentou o segundo,
    # os dois passariam a apontar para o MESMO .acmi -- dois ofstream
    # truncando a mesma gravacao, em silencio, em todos os testes de cenario.
    # Nenhum teste afirma sobre o .acmi, entao a corrupcao passaria batida
    # justamente no arquivo que um humano abre para depurar.
    texto = texto.replace("port: 1234", "port: 12341")
    texto = re.sub(r'fileName:\s*"[^"]*\.acmi"',
                   f'fileName: "./build/tests-recordings/{args.poc}-{args.mode}.acmi"',
                   texto)
    texto = re.sub(r'fileName:\s*"[^"]*\.jsonl"',
                   f'fileName: "./build/tests-messages/{args.poc}-{args.mode}.jsonl"',
                   texto)

    # 3) o delta do modo
    if args.mode == "intruder":
        marcador = "      } // players"
        if marcador not in texto:
            raise SystemExit("nao achei o fim da lista de players")
        texto = texto.replace(marcador, BANDIT + "\n" + marcador, 1)
    elif args.mode == "lowfuel":
        antes = texto
        texto = re.sub(r"fuelReserve:(\s*)0\.35", r"fuelReserve:\g<1>0.99", texto)
        if texto == antes:
            raise SystemExit("nenhum slot fuelReserve ajustado")
    elif args.mode == "terrain":
        antes = texto
        texto = texto.replace("minAltitude:      ( Meters 1200 )",
                              "minAltitude:      ( Meters 2600 )")
        texto = texto.replace("minAltitude: ( Meters 1200 )",
                              "minAltitude: ( Meters 2600 )")
        if texto == antes:
            raise SystemExit("nenhum slot minAltitude ajustado")

    # O parser EDL deste fork recusa o arquivo INTEIRO se houver um unico
    # caractere acentuado, mesmo dentro de um comentario -- e aponta a linha
    # certa sem dizer o motivo. Melhor falhar aqui, com a causa na mao.
    try:
        texto.encode("ascii")
    except UnicodeEncodeError as e:
        trecho = texto[max(0, e.start - 60):e.start + 60]
        raise SystemExit(f"fixture tem caractere nao-ASCII (o edl_parser recusa):\n...{trecho}...")

    saida = Path(args.out)
    saida.parent.mkdir(parents=True, exist_ok=True)
    (raiz / "build" / "tests-recordings").mkdir(parents=True, exist_ok=True)
    (raiz / "build" / "tests-messages").mkdir(parents=True, exist_ok=True)
    saida.write_text(texto, encoding="ascii")
    print(f"  fixture {args.mode} ({args.poc}) -> {saida}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
