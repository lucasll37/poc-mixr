"""PY-RTB -- voltar para a base quando o combustivel cai.

Folha do ramo 1 (o de maior prioridade) de
configs/flight_tree_python.xml. Quem decide QUANDO este script roda e o
( FuelLow margin="0.05" ) nativo, contra o slot 'fuelReserve' do cenario.

A base e a ORIGEM DA AREA DE JOGO -- o mesmo ponto em torno do qual o
patrol.py orbita, e o mesmo que domain::RtbPlan usa por padrao. Nao ha pista
nem procedimento: chegar perto basta.

Duas fases, como no ( ReturnToBase ) de producao:

  * longe    voa direto para a base, subindo SUBIDA_M acima do cruzeiro
             (mais alto gasta menos, e tira a aeronave do trafego de quem
             ainda esta patrulhando na orbita de 5 NM)
  * chegando dentro do raio de chegada, mantem o rumo e reduz a velocidade,
             esperando o reabastecimento -- que e do FuelSystem nativo, nao
             deste script

A altitude de cruzeiro e a que a aeronave TROUXE, latcheada na primeira
decisao, pelo mesmo motivo e do mesmo jeito que em patrol.py: cada falcon
nasce numa altitude propria, calculada contra o pico do circuito dele, e o
script nao precisa saber qual e.

Sobre isso vai o piso de terreno, que aqui e menos decorativo do que parece:
o caminho reto ate a origem cruza a serra, e a altitude de cruzeiro de um
falcon do outro lado do circuito pode estar dentro dela.
"""

import math

# Indices na ordem canonica de shared/xrlbridge/ObservationFields.hpp.
NORTH_M = 0
EAST_M = 1
ALTITUDE_M = 2
HEADING_DEG = 3
TERRAIN_ELEV_M = 11
TERRAIN_VALID = 24

BASE_N_M = 0.0
BASE_E_M = 0.0
RAIO_DE_CHEGADA_M = 2.0 * 1852.0      # 2 NM, o mesmo arrivalRadius do cenario

SUBIDA_M = 300.0
VELOCIDADE_KTS = 170.0
FATOR_DE_ESPERA = 0.6                 # velocidade em cima da base

FOLGA_TERRENO_M = 850.0
PISO_ABSOLUTO_M = 200.0

# Estado POR AERONAVE.
_altitude_cruzeiro_m = None


def _wrap360(deg):
    """Normaliza para [0, 360) -- mesma conta de domain::wrap360()."""
    return deg % 360.0


def _rumo_para(de_n, de_e, para_n, para_e):
    """Rumo verdadeiro em graus -- mesma conta de domain::headingToDeg()."""
    return _wrap360(math.degrees(math.atan2(para_e - de_e, para_n - de_n)))


def _piso(obs):
    """Altitude minima comandavel -- mesma regra de domain::clampToTerrain()."""
    if obs[TERRAIN_VALID] >= 0.5:
        return max(PISO_ABSOLUTO_M, obs[TERRAIN_ELEV_M] + FOLGA_TERRENO_M)
    return PISO_ABSOLUTO_M


def decide(obs):
    global _altitude_cruzeiro_m

    if _altitude_cruzeiro_m is None:
        _altitude_cruzeiro_m = obs[ALTITUDE_M]

    norte = obs[NORTH_M]
    leste = obs[EAST_M]
    altitude = max(_altitude_cruzeiro_m + SUBIDA_M, _piso(obs))

    if math.hypot(norte - BASE_N_M, leste - BASE_E_M) <= RAIO_DE_CHEGADA_M:
        return (_wrap360(obs[HEADING_DEG]), altitude,
                VELOCIDADE_KTS * FATOR_DE_ESPERA)

    return (_rumo_para(norte, leste, BASE_N_M, BASE_E_M), altitude,
            VELOCIDADE_KTS)
