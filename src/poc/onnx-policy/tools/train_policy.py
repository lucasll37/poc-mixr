#!/usr/bin/env python3
"""Treina a rede que pilota esta poc e a exporta para configs/policy_barrier.onnx.

    src/rl/.venv/bin/python3 src/poc/onnx-policy/tools/train_policy.py

(rodar da RAIZ do repositorio; precisa de numpy e onnx -- ver o README desta
poc para o passo de venv.)

O ARTEFATO E VERSIONADO. Este script existe para que a rede nao seja um
binario opaco no repositorio: ela e reproduzivel (semente fixa, mesmos pesos)
e o que ela aprendeu esta escrito aqui, em REGRA(), em vez de so nos pesos.

--------------------------------------------------------------------------
O QUE A REDE APRENDE, E POR QUE E ISTO
--------------------------------------------------------------------------

Clonagem de comportamento (behavior cloning): amostra-se o espaco de
observacao, calcula-se o comando que uma REGRA daria, e treina-se um MLP a
reproduzir esse comando. Nao e RL -- e o caminho mais curto entre "uma regra
que eu sei escrever" e "uma rede que voa", e serve ao proposito desta poc,
que e provar a CADEIA (rede -> comando -> Autopilot, dentro do frame), nao
descobrir uma tatica nova. Uma politica de RL de verdade entra no lugar
desta trocando o arquivo: e o mesmo contrato de 28 entradas e 3 saidas
(src/poc/rl-training/tools/export_onnx.py --sb3).

A regra clonada e uma BARREIRA no paralelo norte=0, com quebra ao contato:

    rumo       90 + 90*tanh(norte / L)
               guiagem por erro lateral: ao norte da linha o comando tende a
               180 (sul), ao sul tende a 0 (norte), e sobre a linha e 90
               (leste). A aeronave CONVERGE para a linha e passa a segui-la.
               Com contato: 90 + 90*tanh((norte_do_contato - norte) / L), que
               e a mesma guiagem apontada para o lado oposto ao do intruso.
    altitude   terreno + 900 m, limitada a [1500, 3200] m
    velocidade 160 kt de cruzeiro, 185 kt com contato (os mesmos numeros de
               patrolSpeed/evadeSpeed do cenario)

Medido no binario (falcon1, que comeca 9,3 km ao norte da linha): entra na
faixa de 100 m aos 160 s de tempo simulado e depois fica em |norte| <= 11 m
por 100 s -- convergencia sem oscilacao, que e o que o tanh da.

POR QUE NAO A ORBITA GEOMETRICA DA IRMA python-flight (marcacao para a base
menos 90 graus). Porque uma rede continua NAO CONSEGUE representa-la. O
comando de rumo sai de um tanh mapeado linearmente em [0, 360]: uma orbita
percorre todos os rumos, entao existe um ponto do espaco de observacao onde
o alvo salta de 360 para 0. Uma funcao continua nao tem esse salto -- ela
interpola, e o resultado no ponto do corte e um rumo intermediario, isto e,
uma curva para o lado errado, exatamente quando a aeronave cruza o corte.
Um script Python nao tem esse problema porque nao interpola nada.

A barreira foi escolhida por NAO ter corte: o alvo vive em [0, 180] e e uma
funcao suave da posicao. E a mesma disciplina do resto da poc-mixr: escolher
a formulacao que o mecanismo consegue representar, e escrever por que.

(Se a saida fosse seno e cosseno do rumo, em vez do rumo, o corte
desapareceria -- mas isso muda o CONTRATO de 3 saidas, que e compartilhado
com o treino de src/rl e com o unscale de shared/xrlbridge. Fica registrado
como o caminho, caso um dia se queira uma politica que orbite.)

--------------------------------------------------------------------------
O QUE ESTE SCRIPT NAO INVENTA
--------------------------------------------------------------------------

A ordem dos 28 campos vem do C++ (src/poc/rl-training/tools/export_onnx.py, que a le de
mixr_gym._native, que expande a X-macro de
shared/xrlbridge/ObservationFields.hpp). Nao ha lista de campos escrita
aqui: uma copia divergiria em silencio e a rede voaria errado sem erro
nenhum. Os limites do unscale (0..360, 0..8000, 0..400) idem -- estao em
ObservationFields.hpp e sao repetidos abaixo com a fonte anotada.
"""

from __future__ import annotations

import argparse
import importlib.util
import pathlib
import sys

RAIZ = pathlib.Path(__file__).resolve().parents[4]
SAIDA_PADRAO = RAIZ / "src/poc/onnx-policy/configs/policy_barrier.onnx"

# XRLBRIDGE_ACTION_FIELDS, de shared/xrlbridge/ObservationFields.hpp. A
# desnormalizacao de verdade e a de xrlbridge::unscaleCommand(), chamada pelo
# no OnnxPolicy; estes numeros so precisam BATER com ela.
ACAO_LIMITES = [("headingDeg", 0.0, 360.0),
                ("altitudeM", 0.0, 8000.0),
                ("speedKts", 0.0, 400.0)]

# Os numeros da regra. Os tres primeiros sao os do cenario (patrolSpeed,
# evadeSpeed e a folga de terreno do BtBehavior); os demais so existem aqui.
CRUZEIRO_KTS = 160.0
EVASAO_KTS = 185.0
FOLGA_TERRENO_M = 900.0
ALT_MIN_M, ALT_MAX_M = 1500.0, 3200.0
ALT_SEM_TERRENO_M = 1750.0
ESCALA_BARREIRA_M = 4000.0    # quao "apertada" e a virada da barreira


# ------------------------------------------------------------------------------
# A ordem canonica dos campos -- lida do C++, nunca escrita aqui
# ------------------------------------------------------------------------------

def ordem_canonica() -> list[str]:
    caminho = RAIZ / "src/poc/rl-training/tools/export_onnx.py"
    spec = importlib.util.spec_from_file_location("export_onnx", caminho)
    modulo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(modulo)
    return modulo.ordem_canonica()


# ------------------------------------------------------------------------------
# A regra a clonar
# ------------------------------------------------------------------------------

def regra(obs, idx):
    """Comando (rumo, altitude, velocidade) para cada linha de 'obs'.

    Vetorizada: 'obs' e (N, 28) e a saida e (N, 3). 'idx' mapeia nome de campo
    para coluna, para nao haver indice magico neste arquivo.
    """
    import numpy as np

    norte = obs[:, idx["northM"]]
    tem_contato = obs[:, idx["hasContact"]] > 0.5
    norte_contato = obs[:, idx["contactNorthM"]]
    terreno_ok = obs[:, idx["terrainValid"]] > 0.5
    terreno = obs[:, idx["terrainElevM"]]

    # rumo: 0 (norte) .. 180 (sul), sem descontinuidade
    eixo = np.where(tem_contato, norte_contato - norte, norte)
    rumo = 90.0 + 90.0 * np.tanh(eixo / ESCALA_BARREIRA_M)

    altitude = np.where(terreno_ok,
                        np.clip(terreno + FOLGA_TERRENO_M, ALT_MIN_M, ALT_MAX_M),
                        ALT_SEM_TERRENO_M)

    velocidade = np.where(tem_contato, EVASAO_KTS, CRUZEIRO_KTS)

    return np.stack([rumo, altitude, velocidade], axis=1)


# ------------------------------------------------------------------------------
# Amostragem do espaco de observacao
#
# A distribuicao tem de COBRIR o que a simulacao produz, senao a rede acerta
# no treino e erra em voo. Os campos que a regra ignora tambem sao sorteados
# (e nao zerados): e assim que a rede aprende a ignora-los.
#
# Os campos de contato/alerta sao ZERADOS quando a flag correspondente e 0 --
# e o que xnative::FlightState faz (o Snapshot nasce zerado a cada frame).
# ------------------------------------------------------------------------------

def amostrar(n, nomes, rng):
    import numpy as np

    idx = {nome: i for i, nome in enumerate(nomes)}
    o = np.zeros((n, len(nomes)), dtype=np.float64)

    def u(nome, lo, hi):
        o[:, idx[nome]] = rng.uniform(lo, hi, n)

    u("northM", -25000, 25000)
    u("eastM", -25000, 25000)
    u("altitudeM", 800, 4000)
    u("headingDeg", 0, 360)
    u("speedKts", 90, 260)
    u("rollDeg", -45, 45)
    u("pitchDeg", -15, 15)
    u("fuelFraction", 0.0, 1.0)
    u("mach", 0.10, 0.45)
    u("gLoad", 0.0, 3.0)
    u("alphaDeg", -5, 15)
    u("terrainElevM", 0, 2100)
    o[:, idx["altitudeAglM"]] = o[:, idx["altitudeM"]] - o[:, idx["terrainElevM"]]

    o[:, idx["valid"]] = 1.0
    o[:, idx["terrainValid"]] = (rng.random(n) < 0.9).astype(float)
    o[:, idx["hasContact"]] = (rng.random(n) < 0.5).astype(float)
    o[:, idx["hasAlert"]] = (rng.random(n) < 0.4).astype(float)
    o[:, idx["weaponReady"]] = (rng.random(n) < 0.2).astype(float)

    # terreno invalido -> os dois campos de terreno vao a zero (FlightState)
    sem_terreno = o[:, idx["terrainValid"]] < 0.5
    o[sem_terreno, idx["terrainElevM"]] = 0.0
    o[sem_terreno, idx["altitudeAglM"]] = 0.0

    com = o[:, idx["hasContact"]] > 0.5
    k = int(com.sum())
    o[com, idx["contactRangeM"]] = rng.uniform(500, 75000, k)
    o[com, idx["contactRelBearingDeg"]] = rng.uniform(-60, 60, k)
    o[com, idx["contactDeltaAltM"]] = rng.uniform(-2000, 2000, k)
    o[com, idx["contactNorthM"]] = rng.uniform(-40000, 40000, k)
    o[com, idx["contactEastM"]] = rng.uniform(-40000, 40000, k)
    o[com, idx["contactAltitudeM"]] = rng.uniform(500, 5000, k)

    ale = o[:, idx["hasAlert"]] > 0.5
    k = int(ale.sum())
    o[ale, idx["alertNorthM"]] = rng.uniform(-40000, 40000, k)
    o[ale, idx["alertEastM"]] = rng.uniform(-40000, 40000, k)
    o[ale, idx["alertAltitudeM"]] = rng.uniform(500, 5000, k)
    o[ale, idx["alertRangeM"]] = rng.uniform(500, 90000, k)

    return o, idx


# ------------------------------------------------------------------------------
# O MLP, em numpy
#
# 28 -> 64 -> 64 -> 3, tanh nas ocultas e tanh na saida (a acao sai em [-1,1],
# que e o que xrlbridge::unscaleCommand() espera -- a mesma forma que um
# export do Stable-Baselines3 produz). Adam, sem dependencia de framework:
# a rede e pequena, o alvo e liso, e uma dependencia de torch neste
# repositorio custaria mais do que estas trinta linhas.
# ------------------------------------------------------------------------------

class Mlp:
    def __init__(self, dims, rng):
        import numpy as np
        self.w, self.b = [], []
        for ent, sai in zip(dims[:-1], dims[1:]):
            # Xavier: mantem a escala da ativacao ao longo das camadas
            self.w.append(rng.normal(0.0, (2.0 / (ent + sai)) ** 0.5, (ent, sai)))
            self.b.append(np.zeros(sai))
        self.mw = [w * 0 for w in self.w]
        self.vw = [w * 0 for w in self.w]
        self.mb = [b * 0 for b in self.b]
        self.vb = [b * 0 for b in self.b]
        self.passo = 0

    def frente(self, x):
        import numpy as np
        ativ = [x]
        for w, b in zip(self.w, self.b):
            ativ.append(np.tanh(ativ[-1] @ w + b))
        return ativ

    def treinar_lote(self, x, y, lr):
        import numpy as np
        ativ = self.frente(x)
        n = x.shape[0]
        perda = float(np.mean((ativ[-1] - y) ** 2))

        # retropropagacao (todas as camadas tem tanh, inclusive a ultima)
        d = (2.0 / n) * (ativ[-1] - y) * (1.0 - ativ[-1] ** 2)
        gw, gb = [None] * len(self.w), [None] * len(self.b)
        for i in range(len(self.w) - 1, -1, -1):
            gw[i] = ativ[i].T @ d
            gb[i] = d.sum(axis=0)
            if i > 0:
                d = (d @ self.w[i].T) * (1.0 - ativ[i] ** 2)

        self.passo += 1
        b1, b2, eps = 0.9, 0.999, 1e-8
        c1 = 1.0 - b1 ** self.passo
        c2 = 1.0 - b2 ** self.passo
        for i in range(len(self.w)):
            self.mw[i] = b1 * self.mw[i] + (1 - b1) * gw[i]
            self.vw[i] = b2 * self.vw[i] + (1 - b2) * gw[i] ** 2
            self.w[i] -= lr * (self.mw[i] / c1) / (np.sqrt(self.vw[i] / c2) + eps)
            self.mb[i] = b1 * self.mb[i] + (1 - b1) * gb[i]
            self.vb[i] = b2 * self.vb[i] + (1 - b2) * gb[i] ** 2
            self.b[i] -= lr * (self.mb[i] / c1) / (np.sqrt(self.vb[i] / c2) + eps)
        return perda


# ------------------------------------------------------------------------------
# Normalizacao: treina-se com entrada padronizada, exporta-se com ela EMBUTIDA
#
# O contrato do .onnx e receber a observacao CRUA (metros, graus, nos) -- e o
# que o no OnnxPolicy monta a partir do WorldView. Mas treinar com colunas que
# vao de 0..1 e outras que vao a 40000 nao converge. A saida e padronizar no
# treino e dobrar a padronizacao dentro da PRIMEIRA camada na hora de
# exportar, o que e exato:
#
#     y = ((x - mu)/sigma) @ W + b  ==  x @ (W/sigma) + (b - (mu/sigma) @ W)
# ------------------------------------------------------------------------------

def embutir_padronizacao(w1, b1, mu, sigma):
    return w1 / sigma[:, None], b1 - (mu / sigma) @ w1


def normalizar_acao(cmd):
    """comando fisico -> [-1,1], com os limites de XRLBRIDGE_ACTION_FIELDS."""
    import numpy as np
    y = np.empty_like(cmd)
    for j, (_, lo, hi) in enumerate(ACAO_LIMITES):
        y[:, j] = 2.0 * (cmd[:, j] - lo) / (hi - lo) - 1.0
    return y


def desnormalizar_acao(y):
    import numpy as np
    cmd = np.empty_like(y)
    for j, (_, lo, hi) in enumerate(ACAO_LIMITES):
        cmd[:, j] = lo + (y[:, j] + 1.0) * 0.5 * (hi - lo)
    return cmd


# ------------------------------------------------------------------------------

def exportar(caminho, rede, mu, sigma, nomes, erro_texto):
    import numpy as np
    from onnx import TensorProto, checker, helper, numpy_helper, save

    w1, b1 = embutir_padronizacao(rede.w[0], rede.b[0], mu, sigma)
    pesos = [(w1, b1)] + list(zip(rede.w[1:], rede.b[1:]))

    inits, nos, entrada = [], [], "obs"
    for i, (w, b) in enumerate(pesos):
        inits.append(numpy_helper.from_array(w.astype(np.float32), f"fc{i}_w"))
        inits.append(numpy_helper.from_array(b.astype(np.float32), f"fc{i}_b"))
        nos.append(helper.make_node("Gemm", [entrada, f"fc{i}_w", f"fc{i}_b"], [f"h{i}"]))
        # tanh em TODAS as camadas, inclusive a ultima: e o Tanh final que
        # garante acao em [-1,1] (ver o comentario de Mlp)
        saida = "action" if i == len(pesos) - 1 else f"a{i}"
        nos.append(helper.make_node("Tanh", [f"h{i}"], [saida]))
        entrada = saida

    grafo = helper.make_graph(
        nos, "barreira_norte_sul",
        [helper.make_tensor_value_info("obs", TensorProto.FLOAT, [1, len(nomes)])],
        [helper.make_tensor_value_info("action", TensorProto.FLOAT, [1, 3])],
        inits)
    modelo = helper.make_model(grafo, opset_imports=[helper.make_opsetid("", 17)])

    # ARMADILHA MEDIDA, nao redescobrir: o ONNX Runtime deste pacote Conan
    # aceita IR ate 9, e o pacote Python 'onnx' 1.22 grava IR 13 por padrao.
    # O sintoma nao e um erro de exportacao -- e a poc voando com bt=PATROL,
    # porque xinfer::open() recusa o arquivo em tempo de execucao e o
    # Fallback da arvore assume:
    #
    #   [xinfer] falha ao abrir '...': Unsupported model IR version: 13,
    #            max supported IR version: 9
    #
    # IR 8 e o mesmo do policy_example.onnx ja versionado em models/flight.
    modelo.ir_version = 8

    modelo.doc_string = (
        "poc-mixr src/poc/onnx-policy: barreira norte-sul com quebra ao contato, "
        "clonada por comportamento (tools/train_policy.py). " + erro_texto +
        " Ordem de entrada: " + ",".join(nomes))
    checker.check_model(modelo)
    save(modelo, str(caminho))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", default=str(SAIDA_PADRAO))
    ap.add_argument("--amostras", type=int, default=60000)
    ap.add_argument("--epocas", type=int, default=120)
    ap.add_argument("--lote", type=int, default=512)
    ap.add_argument("--oculta", type=int, default=64)
    ap.add_argument("--semente", type=int, default=20260903)
    args = ap.parse_args()

    try:
        import numpy as np
        import onnx  # noqa: F401  (so para falhar aqui, com mensagem, e nao no fim)
    except ImportError as exc:
        sys.exit(f"faltam dependencias Python ('numpy' e 'onnx'): {exc}\n"
                 "  src/rl/.venv/bin/pip install numpy onnx")

    nomes = ordem_canonica()
    print(f"ordem canonica ({len(nomes)} campos), lida do C++")

    rng = np.random.default_rng(args.semente)
    obs, idx = amostrar(args.amostras, nomes, rng)
    alvo = normalizar_acao(regra(obs, idx))

    corte = int(0.9 * args.amostras)
    xtr, ytr = obs[:corte], alvo[:corte]
    xte, yte = obs[corte:], alvo[corte:]

    mu = xtr.mean(axis=0)
    sigma = xtr.std(axis=0)
    sigma[sigma < 1e-6] = 1.0        # coluna constante (valid) nao se padroniza
    xtr_n = (xtr - mu) / sigma
    xte_n = (xte - mu) / sigma

    rede = Mlp([len(nomes), args.oculta, args.oculta, 3], rng)
    lotes = max(1, corte // args.lote)
    for epoca in range(args.epocas):
        ordem = rng.permutation(corte)
        # decaimento cosseno: os ultimos passos e que dao o ultimo grau
        lr = 3e-3 * (0.5 * (1 + np.cos(np.pi * epoca / args.epocas))) + 1e-5
        perda = 0.0
        for i in range(lotes):
            sel = ordem[i * args.lote:(i + 1) * args.lote]
            perda = rede.treinar_lote(xtr_n[sel], ytr[sel], lr)
        if (epoca + 1) % 20 == 0 or epoca == 0:
            print(f"  epoca {epoca + 1:4d}/{args.epocas}  perda {perda:.3e}  lr {lr:.1e}")

    # Erro em unidade FISICA, no conjunto separado -- e o unico numero que
    # diz se a rede voa como a regra.
    prev = desnormalizar_acao(rede.frente(xte_n)[-1])
    real = desnormalizar_acao(yte)
    dif = np.abs(prev - real)
    linhas = []
    for j, (nome, _, _) in enumerate(ACAO_LIMITES):
        linhas.append(f"{nome}: medio {dif[:, j].mean():.2f}, maximo {dif[:, j].max():.2f}")
    erro_texto = "Erro contra a regra (holdout): " + "; ".join(linhas) + "."
    print("  " + erro_texto)

    saida = pathlib.Path(args.out)
    saida.parent.mkdir(parents=True, exist_ok=True)
    exportar(saida, rede, mu, sigma, nomes, erro_texto)
    tam = saida.stat().st_size
    print(f"escrito {saida} ({tam} bytes): float32[1,{len(nomes)}] -> float32[1,3], opset 17")


if __name__ == "__main__":
    main()
