#!/usr/bin/env python3
"""Deriva um cenario de TESTE do cenario de PRODUCAO.

Por que derivar em vez de versionar uma copia: uma copia comecaria correta e
envelheceria em silencio -- mexer no scenario.edl.in nao quebraria teste
nenhum, que e exatamente o oposto do que se quer. Aqui a fixture e sempre o
cenario de verdade com um delta pequeno e explicito.

Todas as fixtures sao HERMETICAS: o bloco 'networks:' sai. Sem isso o
processo abre a porta DIS 3000 e ingere o que estiver na rede -- observado na
pratica, com um bandit de outra sessao mudando o resultado de duas
execucoes identicas (TacviewOutput/DIS nao fazem parte do que estes testes
querem medir). O Tacview tambem e desviado de porta e de arquivo, para nao
disputar a 1234 com uma poc que esteja rodando de verdade.

Modos:

  intruder  reintroduz o bandit1 LOCAL. Sem ele nao ha como exercitar
            EVADE/SUPPORT num processo so, porque o intruso hoje mora em
            src/poc/dis/bandit e chega apenas por DIS.
  lowfuel   sobe 'fuelReserve' acima da fracao real de combustivel, pelo slot
            que ja existe: o ramo de RTB vence desde o primeiro frame.
  terrain   sobe 'minAltitude' do AltitudeSafetyBehavior acima da altitude de
            cruzeiro: o comportamento de voto 90 assume e a arvore perde.
  plain     so o hermetico -- nenhum delta de comportamento. Existe para
            testes que nao querem EVADE/RTB/SAFETY no caminho, so PATROL
            (ex.: check_patrol_seed.sh, que precisa isolar o efeito da
            semente sem outro ramo da arvore competindo).

--patrol-seed, opcional e composto com QUALQUER --mode: substitui o valor de
TODAS as ocorrencias de 'patrolMasterSeed:' no cenario (mesmo literal
repetido nos 4 falcons, ver CLAUDE.md secao shared/xrandom) por um numero
escolhido na hora -- para comparar duas fixtures com sementes diferentes sem
duplicar o .edl.in inteiro.
"""

import argparse
import re
import sys
from pathlib import Path

# O intruso, identico ao de src/poc/dis/bandit/configs/scenario.edl, so que
# local. O rootDir do jsbsim e o mesmo em qualquer poc -- e dado do MODELO
# (models/player/A4/data/jsbsim), instalado uma unica vez em dist/.
BANDIT = """
         // ------------------------------------------------------------------
         // FIXTURE DE TESTE -- bandit1 LOCAL.
         //
         // Em producao o intruso vive no processo src/poc/dis/bandit e chega por
         // DIS. Num teste de processo unico isso tornaria EVADE e SUPPORT
         // inalcancaveis, entao ele volta para ca, com os mesmos valores
         // iniciais do lado de la.
         // ------------------------------------------------------------------
         bandit1: ( Aircraft
            id: 201
            side: red
            type: "A4"
            signature: ( SigSphere radius: 4.0 )
            initXPos:     ( NauticalMiles 12.0 )
            initYPos:     ( NauticalMiles 12.0 )
            initAlt:      ( Meters 2400.0 )
            initHeading:  ( Degrees 225 )
            initVelocity: 195.0
            dataLogTime: ( Seconds 0.1 )
            interpolateTerrain: true
            components: {
               dynamicsModel: ( JSBSimModel
                  rootDir: "./dist/share/mixr-plugins/flight/jsbsim/"  model: "A4"  debugLevel: 0
               )
               pilot: ( Autopilot
                  navMode: false  headingHoldMode: true  altitudeHoldMode: true
                  velocityHoldMode: true  maxRateOfTurnDps: 6.0  maxBankAngle: 45.0
                  maxPitchAngle: 20.0  maxClimbRateMps: 40.0  maxAcceleration: 6.0
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
    ap.add_argument("--poc", required=True, help="nome da pasta em src/poc/ (single-thread, multi-thread, python-flight)")
    ap.add_argument("--mode", required=True, choices=("intruder", "lowfuel", "terrain", "plain"))
    ap.add_argument("--out", required=True, help="caminho do .edl.in a gerar")
    ap.add_argument("--patrol-seed", type=int, default=None,
                     help="substitui patrolMasterSeed em todo player (composto com --mode)")
    args = ap.parse_args()

    raiz = Path(__file__).resolve().parents[2]
    # A poc e procurada, nao montada por concatenacao: desde que as tres pocs
    # de DIS foram agrupadas em src/poc/dis/, o caminho deixou de ser
    # 'src/poc/<poc>' para todas.
    candidatos = sorted((raiz / "src" / "poc").glob("**/configs/scenario.edl.in"))
    achados = [c for c in candidatos if c.parent.parent.name == args.poc]
    if not achados:
        raise SystemExit(f"poc '{args.poc}' nao encontrada sob src/poc/ "
                         f"(procurei por */configs/scenario.edl.in)")
    origem = achados[0]
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
    # Cada poc tem a SUA porta de Tacview (1234 nas gemeas, 1237 na
    # python-flight); a fixture desvia qualquer uma delas acrescentando um
    # digito, para nunca disputar a porta com uma poc rodando de verdade.
    # So a faixa 123x e tocada: o DIS usa 3000/300x e ja saiu com o
    # bloco 'networks:'.
    texto = re.sub(r'port:\s*(123\d)\b', lambda m: f'port: {m.group(1)}1', texto)
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
    elif args.mode == "plain":
        pass  # so o hermetico -- nenhum delta de comportamento

    if args.patrol_seed is not None:
        antes = texto
        texto = re.sub(r"(patrolMasterSeed:\s*)\d+", rf"\g<1>{args.patrol_seed}", texto)
        if texto == antes:
            raise SystemExit("nenhum slot patrolMasterSeed ajustado -- "
                              "este cenario declara o slot?")

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
