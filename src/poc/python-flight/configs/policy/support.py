"""PY-SUPPORT -- ir ao ponto avisado por outro falcon.

Folha do ramo 3 de configs/flight_tree_python.xml. Quem decide QUANDO este
script roda e o ( AlertReceived ) nativo: ha um alerta tatico valido na
percepcao desta aeronave.

O alerta chega pelo ( AlertDatalink ) do modelo, na fase 1 do frame, emitido
por quem esta evadindo -- que neste subprojeto e outra aeronave rodando o
evade.py desta mesma pasta. Nenhum script fala com outro; o que atravessa e o
evento nativo do MIXR.

O alerta NAO manda fazer nada: ele so diz onde esta o intruso. A decisao de
apoiar e desta aeronave, e e esta funcao.

A regra e a mesma do ( SupportAlert ) de producao -- voar para a posicao
avisada, na altitude avisada -- com um acrescimo que a versao C++ nao tem: o
piso de terreno. A altitude vem de OUTRA aeronave, sobre OUTRO ponto da
serra; aplicar o piso local antes de comandar e barato e evita herdar uma
altitude que era segura la e nao e aqui.

Sem estado entre ticks: a resposta e funcao so da observacao. Nada aqui
precisa lembrar de nada.
"""

import math

# Indices na ordem canonica de shared/xrlbridge/ObservationFields.hpp.
NORTH_M = 0
EAST_M = 1
TERRAIN_ELEV_M = 11
ALERT_NORTH_M = 19
ALERT_EAST_M = 20
ALERT_ALTITUDE_M = 21
TERRAIN_VALID = 24

# O mesmo numero do slot supportSpeed do cenario, que agora e daqui.
VELOCIDADE_KTS = 180.0

FOLGA_TERRENO_M = 800.0
PISO_ABSOLUTO_M = 200.0


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
    rumo = _rumo_para(obs[NORTH_M], obs[EAST_M],
                      obs[ALERT_NORTH_M], obs[ALERT_EAST_M])
    altitude = max(obs[ALERT_ALTITUDE_M], _piso(obs))
    return (rumo, altitude, VELOCIDADE_KTS)
