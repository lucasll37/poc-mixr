#!/usr/bin/env python3
"""Treina PPO (Stable-Baselines3) contra MixrFlightEnv e salva o checkpoint.

Caso de uso completo do ambiente de src/rl -- nao um brinquedo: PPO de
verdade, contra a Station de verdade (mesmo plugin libflight_tc.so que
single-thread/multi-thread/app usam), com checkpoints periodicos (o
episodio inteiro depende do JSBSim integrando em tempo real de CPU; perder
progresso a um crash no meio de 200k passos custa caro) e salvamento no
Ctrl+C.

USO

    make train                                  # via o Makefile desta pasta
    make train ARGS="--timesteps 200000 --seed 1"

    # ou direto, com o venv ja criado ('make venv'):
    cd <raiz-do-repositorio>
    PYTHONPATH=./dist/python src/poc/rl-training/.venv/bin/python3 \\
        src/poc/rl-training/train.py --timesteps 200000

Rode sempre com cwd na RAIZ do repositorio -- MixrFlightEnv resolve
'scenario_path' por caminho relativo (mesma convencao de todo binario deste
projeto; ver src/rl/README.md). O alvo 'train' do Makefile local ja faz
esse 'cd' por voce.
"""

from __future__ import annotations

# ARMADILHA CONFIRMADA (nao redescobrir -- ver mixr_gym/__init__.py e
# src/rl/README.md, secao "Limites conhecidos"): 'mixr_gym' TEM DE ser
# importado antes de 'numpy'/'gymnasium'/'stable_baselines3' (que traz os
# dois) neste processo, senao a primeira chamada a reset() segfauta dentro
# de libstdc++. E por isso que este import vem primeiro, fora de ordem
# alfabetica/PEP8 -- de proposito, e por isso que todo import daqui pra
# baixo (inclusive stable_baselines3) so acontece DEPOIS dele.
from mixr_gym import MixrFlightEnv
from mixr_gym.env import DEFAULT_PLAYER, DEFAULT_SCENARIO
from flatten_obs import FlattenedObservation

import argparse
import pathlib
import signal
import sys

from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import CheckpointCallback


# Ancorado no proprio arquivo, NUNCA relativo a cwd -- train.py roda com
# cwd na RAIZ do repositorio (o Makefile faz 'cd' pra la, pra
# MixrFlightEnv resolver 'scenario_path'), e uma string relativa tipo
# "./runs" acabaria escrevendo em <raiz-do-repo>/runs/, fora do
# .gitignore desta pasta -- MEDIDO acontecendo (a mesma armadilha bateu
# via notebooks/train.ipynb, que tambem muda cwd pra raiz).
DEFAULT_OUT_DIR = pathlib.Path(__file__).resolve().parent / "runs"


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--scenario", default=DEFAULT_SCENARIO,
                    help=f"cenario .edl (default: {DEFAULT_SCENARIO})")
    ap.add_argument("--player", default=DEFAULT_PLAYER,
                    help=f"player controlado -- tem que ter RLBridgeBehavior no cenario (default: {DEFAULT_PLAYER})")
    ap.add_argument("--timesteps", type=int, default=200_000,
                    help="total de passos de treino (default: 200000)")
    ap.add_argument("--max-episode-steps", type=int, default=2000,
                    help="truncamento de episodio, em passos do env (default: 2000, ver MixrFlightEnv)")
    ap.add_argument("--seed", type=int, default=0, help="semente do PPO e do env (default: 0)")
    ap.add_argument("--checkpoint-freq", type=int, default=20_000,
                    help="salva um checkpoint a cada N passos (default: 20000)")
    ap.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR),
                    help=f"diretorio de saida (default: {DEFAULT_OUT_DIR})")
    ap.add_argument("--tensorboard-log", default=None,
                    help="diretorio de log do TensorBoard (default: desligado)")
    return ap.parse_args()


def main() -> None:
    args = parse_args()

    out_dir = pathlib.Path(args.out_dir)
    checkpoints_dir = out_dir / "checkpoints"
    checkpoints_dir.mkdir(parents=True, exist_ok=True)
    final_path = out_dir / f"ppo_{args.player}"

    # reward_fn nao e passado -- MixrFlightEnv cai no default_reward() dela
    # mesma (custo pequeno por passo, penalidade grande se 'terminated'). E
    # so um ponto de partida (ver src/rl/README.md, secao "Contrato de
    # dados"); troque por reward_fn=minha_reward no construtor abaixo para
    # pesquisa de verdade.
    env = MixrFlightEnv(
        scenario_path=args.scenario,
        player_name=args.player,
        max_episode_steps=args.max_episode_steps,
    )
    env = FlattenedObservation(env)

    print(f"observation_space: {env.observation_space}")
    print(f"action_space:      {env.action_space}")

    # 'MlpPolicy' sobre o Box(28,) achatado -- ver FlattenedObservation
    # acima para o "porque" (o contrato .onnx de producao e flat, nao
    # Dict/Discrete). Um unico env, nao vetorizado: so pode existir UMA
    # Station por processo (ver o cabecalho de NativeSimulation.hpp), entao
    # nao ha como paralelizar rollout com SubprocVecEnv aqui -- o gargalo de
    # throughput e o passo do JSBSim, nao a GPU, daí 'device' fica no
    # default ('auto') sem forcar CPU nem GPU.
    model = PPO(
        "MlpPolicy",
        env,
        seed=args.seed,
        verbose=1,
        tensorboard_log=args.tensorboard_log,
    )

    checkpoint_callback = CheckpointCallback(
        save_freq=args.checkpoint_freq,
        save_path=str(checkpoints_dir),
        name_prefix=f"ppo_{args.player}",
    )

    # Ctrl+C salva o que ja foi treinado em vez de perder tudo -- o
    # episodio inteiro roda contra o JSBSim de verdade (tempo de CPU real,
    # nao um step() barato de simulador de brinquedo), entao interromper no
    # meio de uma corrida longa e o caso comum, nao a excecao.
    def salvar_e_sair(signum, frame):
        print(f"\ninterrompido -- salvando checkpoint parcial em {final_path}.zip")
        model.save(str(final_path))
        env.close()
        sys.exit(130)

    signal.signal(signal.SIGINT, salvar_e_sair)

    model.learn(total_timesteps=args.timesteps, callback=checkpoint_callback)

    model.save(str(final_path))
    env.close()
    print(f"treino concluido -- checkpoint final em {final_path}.zip")
    print(
        "exportar para producao (ver src/poc/onnx-policy e models/A4/docs/POLITICAS.md):\n"
        f"  make export ARGS=\"--sb3 {final_path}.zip -o meu_policy.onnx\""
    )


if __name__ == "__main__":
    main()
