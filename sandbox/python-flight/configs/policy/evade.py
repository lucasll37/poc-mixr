"""PY-EVADE -- a manobra de quebra, quando ha intruso.

Folha do ramo 2 de configs/flight_tree_python.xml. Quem decide QUANDO este
script roda e o ( ContactDetected ) nativo, que consulta a histerese de
domain::ThreatPolicy: ele continua verdadeiro por 'evadeHold' segundos DEPOIS
de a pista sumir. Isso importa aqui, e muito -- ver "O ALVO E FIXADO" abaixo.

O ( ReportAndEvade ) roda entre a condicao e este script, e e ele que marca o
pedido de alerta tatico para os outros falcons. Este script sobrescreve o
comando de voo que ele fixou, mas nao o alerta: quem avisa e o C++, quem
manobra e o Python.

A REGRA (a mesma de domain::ThreatPolicy::breakCommand, reescrita aqui)

  1. A marcacao ABSOLUTA do contato e 'meu rumo + marcacao relativa dele'.
  2. Fugir e sair dessa marcacao pela quebra, para o lado OPOSTO ao que ele
     ocupa: contato a boreste (marcacao relativa >= 0) => quebra a bombordo.
     O rumo de fuga e relativo AO CONTATO, nao ao proprio nariz -- senao para
     onde a curva termina depende de para onde a aeronave ja estava apontando.
  3. Desconflito vertical no sentido contrario ao dele: se o contato esta
     acima, desce; se esta abaixo, sobe.
  4. Nunca abaixo do terreno mais a folga.

O ALVO E FIXADO NA ENTRADA DA MANOBRA, E ESSE E O PONTO DELICADO

Recalcular o alvo a cada tick e a armadilha classica: 'meu_rumo + 110 graus'
devolvido 50 vezes por segundo e um setpoint que foge na mesma velocidade em
que a aeronave gira, entao a curva nunca termina. O mesmo vale para a
altitude. O C++ resolve isso com um `if (!engaged_)` -- ele tem a maquina de
estados na mao.

Um script daqui nao tem: `decide()` e chamado sem contexto nenhum sobre o que
aconteceu antes. O que ele TEM e o dicionario de globais proprio de cada
aeronave, que sobrevive entre ticks (ver shared/xpyembed/README.md), e a
propria posicao. Dai o detector de episodio abaixo: entre dois ticks
CONSECUTIVOS a aeronave anda ~1,6 m (82 m/s a 50 Hz), enquanto entre dois
episodios de evasao ela passa minutos patrulhando. Um salto de posicao maior
que SALTO_DE_EPISODIO_M significa, portanto, "faz tempo que este script nao e
chamado" -- ou seja, uma quebra NOVA, que merece um alvo novo.

E dt-livre e deterministico: so olha para posicao, que e a mesma em toda
execucao. Nao ha relogio nem contador de tempo em lugar nenhum desta pasta.
"""

import math

# Indices na ordem canonica de shared/xrlbridge/ObservationFields.hpp.
NORTH_M = 0
EAST_M = 1
ALTITUDE_M = 2
HEADING_DEG = 3
TERRAIN_ELEV_M = 11
CONTACT_REL_BEARING_DEG = 14
CONTACT_DELTA_ALT_M = 15
TERRAIN_VALID = 24
HAS_CONTACT = 25

# Os mesmos numeros dos slots breakTurn/evadeClimb/evadeSpeed do cenario --
# que agora sao daqui: mexer neles e mexer neste arquivo.
QUEBRA_DEG = 110.0
DESCONFLITO_M = 700.0
VELOCIDADE_KTS = 185.0

FOLGA_TERRENO_M = 800.0
PISO_ABSOLUTO_M = 200.0

# Acima disto, dois ticks nao sao consecutivos: houve um episodio no meio.
# Com folga larga -- 50 m sao ~30 ticks a 50 Hz e ~6 a 10 Hz.
SALTO_DE_EPISODIO_M = 50.0

# Estado POR AERONAVE.
_alvo = None            # (rumo, altitude) fixado na entrada da manobra
_ultima_posicao = None  # (norte, leste) da chamada anterior


def _wrap360(deg):
    """Normaliza para [0, 360) -- mesma conta de domain::wrap360()."""
    return deg % 360.0


def _piso(obs):
    """Altitude minima comandavel -- mesma regra de domain::clampToTerrain()."""
    if obs[TERRAIN_VALID] >= 0.5:
        return max(PISO_ABSOLUTO_M, obs[TERRAIN_ELEV_M] + FOLGA_TERRENO_M)
    return PISO_ABSOLUTO_M


def _comando_de_quebra(obs):
    """O alvo da manobra, calculado UMA vez por episodio."""
    marcacao_relativa = obs[CONTACT_REL_BEARING_DEG]
    marcacao_absoluta = _wrap360(obs[HEADING_DEG] + marcacao_relativa)

    quebra = -QUEBRA_DEG if marcacao_relativa >= 0.0 else QUEBRA_DEG
    rumo = _wrap360(marcacao_absoluta + quebra)

    if obs[CONTACT_DELTA_ALT_M] >= 0.0:
        altitude = obs[ALTITUDE_M] - DESCONFLITO_M   # contato acima: desce
    else:
        altitude = obs[ALTITUDE_M] + DESCONFLITO_M   # contato abaixo: sobe

    return (rumo, max(altitude, _piso(obs)))


def decide(obs):
    global _alvo, _ultima_posicao

    posicao = (obs[NORTH_M], obs[EAST_M])

    episodio_novo = (
        _ultima_posicao is None
        or math.hypot(posicao[0] - _ultima_posicao[0],
                      posicao[1] - _ultima_posicao[1]) > SALTO_DE_EPISODIO_M
    )
    _ultima_posicao = posicao

    # Fixa o alvo na entrada. Rever o contato durante a manobra apenas re-arma
    # a histerese la no C++ -- nao move o alvo, que e o que fazia a curva
    # nunca terminar.
    if episodio_novo or _alvo is None:
        if obs[HAS_CONTACT] >= 0.5:
            _alvo = _comando_de_quebra(obs)
        else:
            # Chamado no arrasto da histerese sem nunca ter visto o contato
            # (o alvo se perdeu junto com um episodio anterior): mantem o que
            # esta fazendo, so afastando do terreno.
            _alvo = (_wrap360(obs[HEADING_DEG]), max(obs[ALTITUDE_M], _piso(obs)))

    rumo, altitude = _alvo
    return (rumo, altitude, VELOCIDADE_KTS)
