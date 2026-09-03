"""PY-PATROL -- a patrulha, quando nada mais esta acontecendo.

Folha do ramo 4 de configs/flight_tree_python.xml. Editar este arquivo muda o
voo da proxima execucao: nao ha compilacao, nem no host nem no plugin.

CONTRATO (o mesmo dos quatro scripts desta pasta)

    def decide(obs) -> (heading_deg, altitude_m, speed_kts)

`obs` sao 28 floats na ORDEM CANONICA de shared/xrlbridge/ObservationFields.hpp
-- a MESMA ordem da entrada de um .onnx, o que permite trocar um script destes
por uma politica treinada sem mexer em mais nada.

O QUE ESTE SCRIPT FAZ, E POR QUE NAO E O QUE O C++ FAZ

O ( Patrol ) nativo voa PERNAS CRONOMETRADAS: 60 s em rumo fixo, curva de 90
graus, repete. Funciona, mas deriva -- a perna depende de 'dt', e um erro de
velocidade vira erro de posicao que nunca se corrige.

Aqui a patrulha e GEOMETRICA: uma orbita de raio fixo em torno da base
(a origem da area de jogo, o mesmo ponto para onde o rtb.py volta). O comando
sai da posicao ATUAL a cada tick, entao qualquer desvio se corrige sozinho e
a figura fecha. E tambem o unico desenho possivel aqui: um script Python NAO
recebe 'dt' (a fronteira de shared/xpyembed e "28 floats entram, 3 saem"),
entao nada nesta pasta pode integrar tempo.

    rumo = marcacao_para_a_base - 90 graus + correcao_de_raio

O termo de 90 graus e a tangente (base sempre a bombordo, orbita horaria); a
correcao inclina a tangente para dentro ou para fora conforme o raio esteja
grande ou pequeno demais. E uma malha proporcional simples, e e o suficiente.

ALTITUDE: a de cruzeiro que a aeronave TROUXE, com piso de terreno.

O cenario da uma altitude inicial diferente para cada falcon (1750 a 2100 m),
calculada a mao contra o pico do circuito de cada um. Este script nao precisa
saber disso: ele guarda a altitude da PRIMEIRA decisao como a sua altitude de
cruzeiro -- cada aeronave tem o seu proprio dicionario de globais, entao a
variavel abaixo e POR AERONAVE (ver shared/xpyembed/README.md). Sobre isso
vai um piso de terreno + FOLGA_TERRENO_M, que substitui a conta manual: onde
a serra sobe, a orbita sobe junto.

A folga (850 m) fica DE PROPOSITO acima do 'recoverClearance' de 800 m do
( AltitudeSafetyBehavior ) do cenario. Esse comportamento tem voto 90 contra
os 50 desta arvore: mantendo-se acima dele, a politica em Python nunca entra
em disputa com a rede de seguranca em C++ -- que continua la, intocada, para
o caso de um script novo fazer besteira.

DETERMINISMO: nao ha relogio, nao ha sorteio, nao ha arquivo. Tudo o que sai
daqui e funcao da observacao mais o estado desta propria aeronave, que e a
condicao para os dumps de 1, 2 e 4 threads sairem identicos.

(As duas funcoes de angulo estao repetidas nos quatro scripts, de proposito:
um `import` traria o MESMO objeto de modulo para as quatro aeronaves --
sys.modules e compartilhado, e so o dicionario de globais e que nao e. Sao
oito linhas de matematica pura; a alternativa seria a unica porta de estado
compartilhado desta pasta.)
"""

import math

# Indices na ordem canonica de shared/xrlbridge/ObservationFields.hpp.
NORTH_M = 0
EAST_M = 1
ALTITUDE_M = 2
HEADING_DEG = 3
TERRAIN_ELEV_M = 11
TERRAIN_VALID = 24

# A orbita. O raio e o mesmo com que o cenario posiciona os quatro falcons
# (initXPos/initYPos = 5 NM da origem), entao a aeronave ja nasce em cima
# dela e nao ha transiente de captura.
BASE_N_M = 0.0
BASE_E_M = 0.0
RAIO_ORBITA_M = 5.0 * 1852.0          # 5 NM

# Malha de raio: graus de inclinacao da tangente por metro de erro, saturada.
GANHO_RAIO_DEG_POR_M = 60.0 / 2000.0  # erro de 2 km satura a correcao
CORRECAO_MAX_DEG = 60.0

# Piso de altitude. Ver a nota sobre o AltitudeSafetyBehavior no cabecalho.
FOLGA_TERRENO_M = 850.0
PISO_ABSOLUTO_M = 200.0

VELOCIDADE_KTS = 160.0

# Estado POR AERONAVE (um dicionario de globais por aeronave).
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
        # Primeira decisao desta aeronave: a altitude com que ela chegou e a
        # altitude que ela mantem. Ver o cabecalho.
        _altitude_cruzeiro_m = obs[ALTITUDE_M]

    norte = obs[NORTH_M]
    leste = obs[EAST_M]

    marcacao_base = _rumo_para(norte, leste, BASE_N_M, BASE_E_M)
    raio = math.hypot(norte - BASE_N_M, leste - BASE_E_M)

    # Longe demais da base: correcao positiva aproxima o rumo da marcacao da
    # base (fecha para dentro). Perto demais: negativa (abre para fora).
    erro = raio - RAIO_ORBITA_M
    correcao = max(-CORRECAO_MAX_DEG,
                   min(CORRECAO_MAX_DEG, erro * GANHO_RAIO_DEG_POR_M))

    rumo = _wrap360(marcacao_base - 90.0 + correcao)
    altitude = max(_altitude_cruzeiro_m, _piso(obs))

    return (rumo, altitude, VELOCIDADE_KTS)
