#!/usr/bin/env python3
"""Exporta uma politica treinada para .onnx, no contrato que o modelo espera.

O fecho do ciclo treino -> producao. O que sai daqui e carregado por
`bt/nodes/OnnxPolicyAction` (models/A4) e roda DENTRO do frame de tempo
critico, sem Python nenhum.

O CONTRATO, e por que ele nao esta escrito aqui:

    entrada  float32[1, 28]   a observacao, na ORDEM CANONICA
    saida    float32[1, 3]    [heading, altitude, speed], normalizados em [-1,1]

A ordem dos 28 campos NAO e uma lista neste arquivo -- ela vem de
`mixr_gym._native.observation_field_names()`, que por sua vez expande a
X-macro de `shared/xrlbridge/ObservationFields.hpp`. E a mesma macro que o
C++ expande contra `domain::WorldView` na hora de inferir. Uma lista escrita
aqui poderia divergir em silencio, e o sintoma seria uma politica que voa
errado -- nao um erro.

USO

    # gera um .onnx de pesos ALEATORIOS para exercitar a cadeia (sem treino)
    python3 src/poc/rl-training/tools/export_onnx.py --random -o models/A4/configs/policy_example.onnx

    # exporta uma politica treinada com Stable-Baselines3
    python3 src/poc/rl-training/tools/export_onnx.py --sb3 runs/ppo_falcon.zip -o politica.onnx

Rode com cwd na raiz do repositorio e PYTHONPATH=./dist/python -- a mesma
convencao de src/rl/tests/test_smoke.py.
"""

from __future__ import annotations

import argparse
import os
import pathlib
import sys


def carregar_native():
    """Carrega mixr_gym._native SEM passar pelo __init__ do pacote.

    O __init__ importa env.py, que importa gymnasium e numpy. Este script so
    precisa da lista de nomes -- exigir o venv de treino para gerar um .onnx
    de teste seria uma dependencia gratuita. Carregar o .so direto tambem
    preserva a ordem de dlopen que o __init__ documenta (RTLD_GLOBAL antes de
    qualquer outra extensao C), porque aqui nada mais foi importado ainda.
    """
    import glob
    import importlib.util

    raiz = pathlib.Path(__file__).resolve().parents[4]
    padrao = str(raiz / "dist" / "python" / "mixr_gym" / "_native*.so")
    achados = glob.glob(padrao)
    if not achados:
        sys.exit(f"nao achei o modulo nativo em {padrao}\n  rode 'make install' primeiro.")

    anterior = sys.getdlopenflags()
    sys.setdlopenflags(anterior | os.RTLD_GLOBAL)
    try:
        spec = importlib.util.spec_from_file_location("_native", achados[0])
        modulo = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(modulo)
    finally:
        sys.setdlopenflags(anterior)
    return modulo


def ordem_canonica() -> list[str]:
    """Os 28 nomes, na ordem, vindos do C++ -- nunca de uma lista local."""
    try:
        return list(carregar_native().observation_field_names())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        sys.exit(f"nao consegui ler a ordem canonica do C++: {exc}")


def exportar_aleatorio(caminho: str, nomes: list[str], oculta: int, semente: int) -> None:
    """Um MLP de pesos aleatorios com a forma certa.

    Existe para exercitar carga, forma, inferencia, desnormalizacao e atuacao
    sem depender de uma sessao de treino. NAO e uma politica -- as acoes sao
    ruido correlacionado com a observacao.
    """
    try:
        import numpy as np
        import onnx
        from onnx import TensorProto, helper, numpy_helper
    except ImportError as exc:
        sys.exit(f"--random precisa de 'onnx' e 'numpy': {exc}")

    n_in, n_out = len(nomes), 3
    rng = np.random.default_rng(semente)

    def denso(nome: str, ent: int, sai: int):
        # Xavier: mantem a escala da ativacao ao longo das camadas, entao o
        # Tanh final nao satura em +-1 para toda observacao.
        w = rng.normal(0.0, (2.0 / (ent + sai)) ** 0.5, size=(ent, sai)).astype(np.float32)
        b = np.zeros((sai,), dtype=np.float32)
        return (numpy_helper.from_array(w, f"{nome}_w"),
                numpy_helper.from_array(b, f"{nome}_b"))

    w1, b1 = denso("fc1", n_in, oculta)
    w2, b2 = denso("fc2", oculta, oculta)
    w3, b3 = denso("fc3", oculta, n_out)

    nos = [
        helper.make_node("Gemm", ["obs", "fc1_w", "fc1_b"], ["h1"]),
        helper.make_node("Tanh", ["h1"], ["a1"]),
        helper.make_node("Gemm", ["a1", "fc2_w", "fc2_b"], ["h2"]),
        helper.make_node("Tanh", ["h2"], ["a2"]),
        helper.make_node("Gemm", ["a2", "fc3_w", "fc3_b"], ["h3"]),
        # Tanh final: a acao sai em [-1,1], que e o que
        # xrlbridge::unscaleCommand() espera (e o que o SB3 produz).
        helper.make_node("Tanh", ["h3"], ["action"]),
    ]

    grafo = helper.make_graph(
        nos, "politica_aleatoria",
        [helper.make_tensor_value_info("obs", TensorProto.FLOAT, [1, n_in])],
        [helper.make_tensor_value_info("action", TensorProto.FLOAT, [1, n_out])],
        [w1, b1, w2, b2, w3, b3],
    )
    modelo = helper.make_model(grafo, opset_imports=[helper.make_opsetid("", 17)])

    # ARMADILHA MEDIDA (ver src/poc/onnx-policy): o ONNX Runtime deste pacote
    # Conan aceita IR ate 9, e o pacote Python 'onnx' >= 1.19 grava 13 por
    # padrao. O sintoma nao aparece aqui -- aparece EM VOO, com xinfer::open()
    # recusando o arquivo ("Unsupported model IR version: 13, max supported
    # IR version: 9") e a arvore caindo no Fallback. IR 8 e o do
    # policy_example.onnx ja versionado.
    modelo.ir_version = 8

    modelo.doc_string = (
        "PESOS ALEATORIOS, nao treinados. Ordem de entrada: " + ",".join(nomes)
    )
    onnx.checker.check_model(modelo)
    onnx.save(modelo, caminho)
    print(f"escrito {caminho}: float32[1,{n_in}] -> float32[1,{n_out}], opset 17")
    print("  ATENCAO: pesos aleatorios -- serve para testar a cadeia, nao para voar.")


def exportar_sb3(caminho_zip: str, saida: str, nomes: list[str]) -> None:
    """Exporta uma politica do Stable-Baselines3.

    Segue a receita oficial (docs do SB3, 'Exporting models'): um wrapper que
    chama a policy com deterministic=True e devolve SO a acao. A acao sai
    NORMALIZADA -- o SB3 nao aplica o unscale do action_space no forward --
    que e exatamente o que OnnxPolicyAction espera com normalized="true".
    """
    try:
        import torch
        from stable_baselines3 import PPO
    except ImportError as exc:
        sys.exit(
            f"--sb3 precisa de 'stable-baselines3' e 'torch': {exc}\n"
            "  instale no venv de src/poc/rl-training (ver 'make venv-rl-training')."
        )

    modelo = PPO.load(caminho_zip, device="cpu")

    class SoAcao(torch.nn.Module):
        def __init__(self, policy):
            super().__init__()
            self.policy = policy

        def forward(self, obs):
            return self.policy(obs, deterministic=True)[0]

    n_in = len(nomes)
    esperado = modelo.observation_space
    print(f"observation_space do modelo: {esperado}")
    print(f"contrato do C++: {n_in} campos -- {','.join(nomes)}")

    dummy = torch.zeros(1, n_in, dtype=torch.float32)
    torch.onnx.export(
        SoAcao(modelo.policy), dummy, saida,
        input_names=["obs"], output_names=["action"],
        opset_version=17, dynamo=False,
    )
    print(f"escrito {saida}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    grupo = ap.add_mutually_exclusive_group(required=True)
    grupo.add_argument("--random", action="store_true",
                       help="gera um .onnx de pesos aleatorios com a forma certa")
    grupo.add_argument("--sb3", metavar="ZIP",
                       help="exporta uma politica treinada do Stable-Baselines3")
    grupo.add_argument("--campos", action="store_true",
                       help="so imprime a ordem canonica dos campos e sai")
    ap.add_argument("-o", "--out", help="arquivo .onnx de saida (nao usado com --campos)")
    ap.add_argument("--hidden", type=int, default=64, help="tamanho da camada oculta (--random)")
    ap.add_argument("--seed", type=int, default=0, help="semente (--random)")
    args = ap.parse_args()

    nomes = ordem_canonica()

    if args.campos:
        for i, nome in enumerate(nomes):
            print(f"{i:2d} {nome}")
        return

    if not args.out:
        ap.error("-o/--out e obrigatorio para exportar")

    print(f"ordem canonica ({len(nomes)} campos), lida do C++")

    if args.random:
        exportar_aleatorio(args.out, nomes, args.hidden, args.seed)
    else:
        exportar_sb3(args.sb3, args.out, nomes)


if __name__ == "__main__":
    main()
