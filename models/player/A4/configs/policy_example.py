"""Exemplo de comportamento escrito em Python, rodando DENTRO do frame.

Carregado por bt/nodes/PyDecideAction. Existe para prototipagem: mexa aqui,
rode, veja o efeito -- sem recompilar nada.

CONTRATO

    def decide(obs) -> (heading_deg, altitude_m, speed_kts)

`obs` e uma lista de 28 floats na ORDEM CANONICA de
shared/xrlbridge/ObservationFields.hpp -- a MESMA ordem que a entrada de um
.onnx. Os indices abaixo saem dessa ordem; se ela mudar, este arquivo muda
junto (e por isso a ordem e tratada como contrato, nao como detalhe).

ONDE ISTO RODA, e o que isso obriga:

  * Na FASE 3 do frame de tempo critico, ate 50 Hz, uma vez por aeronave.
    Medido: ~8 us por chamada com uma thread, ~18 us com quatro (o GIL
    serializa). Ha folga de sobra em 20 ms, mas nao faca I/O aqui.

  * Cada aeronave tem o SEU dicionario de globais. Estado guardado em nivel de
    modulo (uma variavel global aqui) e POR AERONAVE, e sobrevive entre ticks
    -- e o jeito de escrever histerese. O que NAO e isolado e o sys.modules:
    um `import` traz o mesmo objeto de modulo para todas.

  * O determinismo dos check-* depende de esta funcao ser PURA em relacao ao
    estado da propria aeronave. Nao leia relogio, nao sorteie sem semente, nao
    escreva arquivo.

A regra abaixo e deliberadamente simples: patrulha em rumo fixo e, se houver
contato, vira para longe dele e acelera. E um ponto de partida, nao uma
politica.
"""

# Indices na ordem canonica (23 floats, depois 5 bools).
ALTITUDE_M = 2
HEADING_DEG = 3
CONTATO_MARCACAO_REL = 14
TEM_CONTATO = 25

ALTITUDE_PATRULHA_M = 1750.0
VELOCIDADE_PATRULHA_KTS = 160.0
VELOCIDADE_EVASAO_KTS = 185.0
QUEBRA_GRAUS = 110.0


def decide(obs):
    rumo = obs[HEADING_DEG]

    if obs[TEM_CONTATO] < 0.5:
        # Sem contato: mantem o que esta fazendo, na altitude de patrulha.
        return (rumo, ALTITUDE_PATRULHA_M, VELOCIDADE_PATRULHA_KTS)

    # Com contato: quebra para o lado OPOSTO ao da marcacao relativa e acelera.
    # A marcacao vem em (-180, 180]; positiva = contato a direita, entao a
    # quebra vai para a esquerda.
    marcacao = obs[CONTATO_MARCACAO_REL]
    quebra = -QUEBRA_GRAUS if marcacao >= 0.0 else QUEBRA_GRAUS
    novo_rumo = (rumo + quebra) % 360.0

    return (novo_rumo, ALTITUDE_PATRULHA_M, VELOCIDADE_EVASAO_KTS)
