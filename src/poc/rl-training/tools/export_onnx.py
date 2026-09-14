#!/usr/bin/env python3
"""Exporta uma politica treinada para .onnx, no contrato que o modelo espera.

O fecho do ciclo treino -> producao. O que sai daqui e carregado por
`bt/nodes/OnnxPolicyAction` (models/players/A-4) e roda DENTRO do frame de tempo
critico, sem Python nenhum.

O CONTRATO, e por que ele nao esta escrito aqui:

    entrada  float32[1, N]   a observacao, nos campos e na ORDEM que --fields
                             resolver (default: os 28 historicos, "classic28")
    saida    float32[1, 3]   [heading, altitude, speed], normalizados em [-1,1]

Os nomes NAO sao uma lista neste arquivo -- por default vem de
`mixr_gym._native.classic_schema_28()` (os 28 nomes historicos, hardcoded do
lado C++ em `xrlbridge::classicSchema28()`); com `--fields all` ou uma lista
explicita, de `observation_field_names()` (os 38 campos completos, incluindo
RWR/navegacao). As duas expandem a MESMA X-macro de
`libs/xrlbridge/ObservationFields.hpp` que o C++ expande contra
`domain::WorldView` na hora de inferir. O .onnx exportado grava a lista
exata usada como metadata (`xrlbridge.fields`) -- e' o que permite
`libs/xinfer::fields()` validar por IDENTIDADE, nao so contagem, do lado C++
(ver `bt/nodes/OnnxPolicyAction`/`OnnxScoreCondition`).

USO

    # gera um .onnx de pesos ALEATORIOS para exercitar a cadeia (sem treino)
    python3 src/poc/rl-training/tools/export_onnx.py --random -o models/players/A-4/configs/policy_example.onnx

    # exporta uma politica treinada com Stable-Baselines3
    python3 src/poc/rl-training/tools/export_onnx.py --sb3 src/poc/rl-training/runs/ppo_falcon1.zip -o politica.onnx

    # exporta so um subconjunto de campos, para um no OnnxPolicy com
    # schema="northM eastM altitudeM" no XML da arvore
    python3 src/poc/rl-training/tools/export_onnx.py --random --fields "northM eastM altitudeM" -o politica.onnx

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
    """Os 38 nomes completos, na ordem, vindos do C++ -- nunca de uma lista
    local. Inclui RWR/navegacao (ver libs/xrlbridge/ObservationFields.hpp) --
    campos que os NOS de arvore (OnnxPolicyAction/OnnxScoreCondition) ja
    conseguem resolver via schema, mas que este exportador so usa se
    --fields pedir 'all' ou uma lista explicita que os inclua."""
    try:
        return list(carregar_native().observation_field_names())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        sys.exit(f"nao consegui ler a ordem canonica do C++: {exc}")


def classic_schema_28() -> list[str]:
    """Os 28 nomes historicos, na ordem historica -- o DEFAULT deste
    exportador (ver xrlbridge::classicSchema28() em libs/xrlbridge/RLBridge.hpp:
    hardcoded do lado C++, nunca derivado do tamanho atual da macro
    canonica, para nao invalidar um .onnx ja treinado se a macro crescer de
    novo)."""
    try:
        return list(carregar_native().classic_schema_28())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        sys.exit(f"nao consegui ler o schema 'classic28' do C++: {exc}")


def resolver_campos(valor: str | None) -> list[str]:
    """Traduz --fields: omitido -> 'classic28' (os 28 historicos, o default
    de sempre); 'all' -> os 38 completos; qualquer outro valor -> lista
    ad-hoc, nomes separados por espaco (ex.: 'northM eastM altitudeM')."""
    if valor is None or valor == "classic28":
        return classic_schema_28()
    if valor == "all":
        return ordem_canonica()
    return valor.split()


def gravar_metadata_campos(modelo, nomes: list[str]) -> None:
    """Grava a lista exata de campos, NA ORDEM, como metadata do proprio
    .onnx -- e' o que permite libs/xinfer::fields() validar por IDENTIDADE
    (nao so contagem) contra o schema que um consumidor C++ resolveu. Risco
    que so passou a existir com schema variavel: dois .onnx do MESMO tamanho
    podem esperar campos DIFERENTES, ou na ordem errada, e a checagem de
    contagem sozinha nao pegaria isso."""
    from onnx import helper
    helper.set_model_props(modelo, {"xrlbridge.fields": ",".join(nomes)})


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
    gravar_metadata_campos(modelo, nomes)
    onnx.checker.check_model(modelo)
    onnx.save(modelo, caminho)
    print(f"escrito {caminho}: float32[1,{n_in}] -> float32[1,{n_out}], opset 17")
    print("  ATENCAO: pesos aleatorios -- serve para testar a cadeia, nao para voar.")


def exportar_sb3(caminho_zip: str, saida: str, nomes: list[str]) -> None:
    """Exporta uma politica do Stable-Baselines3.

    Segue a receita oficial (docs do SB3, 'Exporting models'): um wrapper que
    chama a policy com deterministic=True. ACHADO POR AUDITORIA, CORRIGIDO
    (nao redescobrir): ao contrario do que o comentario desta funcao dizia
    antes, o SB3 padrao (PPO/MlpPolicy, sem squash_output+use_sde -- o que
    train.py usa) NAO aplica Tanh nenhum no forward -- `ActorCriticPolicy.
    forward()` devolve a media crua de uma `DiagGaussianDistribution` sobre
    `action_net` (uma Linear comum), e squash_output so e permitido com
    use_sde=True (common/policies.py, SB3 2.9.0). A acao sai em UNIDADES
    FISICAS -- a MESMA escala de `action_space` (Box(0..360, 0..8000,
    0..400), os defaults de MixrFlightEnv) -- nunca em [-1,1]. Confirmado
    lendo o fonte do SB3 e inspecionando o grafo ONNX exportado da forma
    antiga (saida = Gemm cru, sem Tanh final).

    Sem correcao, uma politica DE FATO treinada (que converge pra valores
    fisicos tipo heading~90) satura no extremo do intervalo fisico ao passar
    por `xrlbridge::unscaleCommand()` (que faz clamp([-1,1]) antes de
    reescalar) -- silencioso, sem erro, so a aeronave voando errado. A
    correcao fecha o contrato desta funcao (ver o docstring do modulo:
    "saida ... normalizados em [-1,1]") de verdade: o grafo exportado agora
    aplica a MESMA equacao de `unscaleCommand()`, na direcao OPOSTA (fisico
    -> [-1,1] aqui; unscaleCommand faz [-1,1] -> fisico no C++), usando os
    limites REAIS de `modelo.action_space` (nao um valor fixo -- MixrFlightEnv
    aceita heading_range/altitude_range_m/speed_range_kts customizados no
    construtor, e o .onnx exportado tem de refletir o que ESTE modelo
    realmente aprendeu, nao os defaults).
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

    baixo = modelo.action_space.low.tolist()
    alto = modelo.action_space.high.tolist()

    class SoAcao(torch.nn.Module):
        def __init__(self, policy, baixo, alto):
            super().__init__()
            self.policy = policy
            # buffers (constantes de escala, nao parametros treinaveis) --
            # precisam viajar com o modulo pra aparecer no grafo ONNX
            # exportado, no dtype/forma certos.
            self.register_buffer("baixo", torch.tensor(baixo, dtype=torch.float32))
            self.register_buffer("alto", torch.tensor(alto, dtype=torch.float32))

        def forward(self, obs):
            acao_fisica = self.policy(obs, deterministic=True)[0]
            # policy() cru NAO recorta pro action_space (isso so acontece no
            # laco de rollout do SB3, fora do grafo) -- recorta aqui pra
            # exportar exatamente o que o SB3 de fato comandaria em inferencia
            # real, antes de normalizar.
            acao_fisica = torch.clamp(acao_fisica, self.baixo, self.alto)
            return 2.0 * (acao_fisica - self.baixo) / (self.alto - self.baixo) - 1.0

    n_in = len(nomes)
    esperado = modelo.observation_space
    print(f"observation_space do modelo: {esperado}")
    print(f"action_space do modelo: baixo={baixo} alto={alto}")
    print(f"contrato do C++: {n_in} campos -- {','.join(nomes)}")

    dummy = torch.zeros(1, n_in, dtype=torch.float32)
    torch.onnx.export(
        SoAcao(modelo.policy, baixo, alto), dummy, saida,
        input_names=["obs"], output_names=["action"],
        opset_version=17, dynamo=False,
    )

    # Mesma armadilha (e mesma correcao) de exportar_aleatorio()/
    # train_policy.py: o exportador legado (dynamo=False) grava ir_version=8
    # por padrao HOJE, mas isso e um efeito colateral da tabela interna do
    # torch, nunca verificado por asercao nenhuma -- fixado aqui explicitamente
    # pra nao depender desse acidente de implementacao se uma versao futura do
    # torch mudar de exportador padrao (o proprio torch ja avisa disso).
    import onnx
    modelo_onnx = onnx.load(saida)
    modelo_onnx.ir_version = 8
    gravar_metadata_campos(modelo_onnx, nomes)
    onnx.checker.check_model(modelo_onnx)
    onnx.save(modelo_onnx, saida)

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
                       help="so imprime os campos resolvidos (--fields, ou 'classic28' "
                            "por default) e sai")
    ap.add_argument("-o", "--out", help="arquivo .onnx de saida (nao usado com --campos)")
    ap.add_argument("--hidden", type=int, default=64, help="tamanho da camada oculta (--random)")
    ap.add_argument("--seed", type=int, default=0, help="semente (--random)")
    ap.add_argument("--fields", metavar="LISTA", default=None,
                    help="quais campos entram, e em que ordem: omitido = 'classic28' "
                         "(os 28 historicos, o default de sempre); 'all' = os 38 "
                         "completos (RWR + navegacao); ou uma lista separada por "
                         "espaco entre aspas (ex.: 'northM eastM altitudeM')")
    args = ap.parse_args()

    nomes = resolver_campos(args.fields)

    if args.campos:
        for i, nome in enumerate(nomes):
            print(f"{i:2d} {nome}")
        return

    if not args.out:
        ap.error("-o/--out e obrigatorio para exportar")

    print(f"campos resolvidos ({len(nomes)}), lidos do C++")

    if args.random:
        exportar_aleatorio(args.out, nomes, args.hidden, args.seed)
    else:
        exportar_sb3(args.sb3, args.out, nomes)


if __name__ == "__main__":
    main()
