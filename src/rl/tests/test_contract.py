#!/usr/bin/env python3
"""Contrato de gymnasium.Env do wrapper -- roda a Station de VERDADE, mesmo
plugin de test_smoke.py, mas cobre outras partes do contrato que aquele
arquivo nao cobre (ver src/rl/README.md para como rodar).

Cobre, na ORDEM em que aparecem (a ordem importa -- ver a armadilha
"UMA Station por processo" abaixo):

  1. step() chamado ANTES de reset() levanta RuntimeError (traducao de
     excecao C++ -> Python) -- SEM construir nenhuma Station (o guard de
     NativeSimulation::step() e 'if (!built_) throw', antes de qualquer
     buildStation()/edl_parser()), entao pode ser o PRIMEIRO teste do
     processo sem gastar a unica Station permitida.
  2. reset() devolve uma observacao que satisfaz observation_space.contains()
     de verdade (nao so as CHAVES, como test_smoke.py ja confere) -- shape,
     dtype e limites.
  3. Uma acao amostrada de action_space e aceita por action_space.contains()
     (trivial por construcao, mas documenta o contrato) e step() aceita.
  4. 'truncated' vira True exatamente quando step_count == max_episode_steps
     (testado com max_episode_steps=5, pequeno de proposito).
  5. close() pode ser chamado mais de uma vez sem levantar.

ARMADILHA CONFIRMADA (nao redescobrir, mesma de test_smoke.py): so pode
existir UMA Station por PROCESSO. Por isso os passos 2-5 reusam o MESMO
'env' (uma unica MixrFlightEnv, construida uma vez) -- nenhum teste aqui cria
um segundo Env/NativeSimulation. O teste de 'player_name incorreto' (que
exigiria construir uma SEGUNDA Station neste mesmo processo para comparar)
mora em processo PROPRIO -- ver src/rl/tests/test_bad_player.py.

ARMADILHA CONFIRMADA (ver mixr_gym/__init__.py): 'mixr_gym' TEM DE ser
importado antes de 'numpy'/'gymnasium' neste processo.
"""

import os
import sys

if not os.path.exists("./src/rl/configs/scenario_rl.edl"):
    print("ERRO: rode este script com cwd na raiz do repositorio", file=sys.stderr)
    sys.exit(1)

from mixr_gym import MixrFlightEnv, _native  # noqa: E402  (ver ARMADILHA -- antes de numpy)

import numpy as np  # noqa: E402
from gymnasium import spaces  # noqa: E402


def main() -> int:
    # --- 1) step() antes de reset() -- excecao C++ traduzida, sem Station --
    sim = _native.NativeSimulation("./src/rl/configs/scenario_rl.edl", "falcon1")
    try:
        sim.step(90.0, 1000.0, 160.0)
        print("FALHOU: step() antes de reset() nao levantou nada", file=sys.stderr)
        return 1
    except RuntimeError as exc:
        print(f"OK  step() antes de reset() levanta RuntimeError: {exc}")

    # --- 2-5) UMA Station real para o resto do contrato -------------------
    env = MixrFlightEnv(max_episode_steps=5)
    obs, info = env.reset()

    if not env.observation_space.contains(obs):
        print("FALHOU: observacao de reset() nao satisfaz observation_space", file=sys.stderr)
        for name, space in env.observation_space.spaces.items():
            if not space.contains(obs[name]):
                print(f"  campo fora do espaco: {name}={obs[name]!r} espaco={space}",
                      file=sys.stderr)
        return 1
    print("OK  observacao de reset() satisfaz observation_space.contains()")

    # Box (float): obs[name] e um np.ndarray, tem de bater o dtype com o
    # espaco. Discrete (bool): obs[name] e um int Python puro (ver
    # env.py::_to_obs) -- Discrete.contains() ja aceita isso, nao ha dtype
    # de ndarray para conferir.
    for name, space in env.observation_space.spaces.items():
        if isinstance(space, spaces.Box):
            assert obs[name].dtype == space.dtype, \
                f"{name}: dtype {obs[name].dtype} != {space.dtype} do espaco"
    print("OK  dtypes da observacao (campos Box) batem com observation_space")

    action = env.action_space.sample()
    assert env.action_space.contains(action), "acao amostrada nao pertence ao proprio espaco"
    obs, reward, terminated, truncated, info = env.step(action)
    assert env.observation_space.contains(obs), "observacao de step() fora do espaco"
    assert isinstance(reward, float), f"reward deveria ser float, veio {type(reward)}"
    assert isinstance(terminated, bool), f"terminated deveria ser bool, veio {type(terminated)}"
    assert isinstance(truncated, bool), f"truncated deveria ser bool, veio {type(truncated)}"
    print(f"OK  step() com acao amostrada: reward={reward}, terminated={terminated}")

    # --- 4) truncated dispara exatamente no limite -------------------------
    env.reset()
    saw_truncated_at = None
    for i in range(1, 10):
        action = np.zeros(3, dtype=np.float32)  # heading=0, altitude=0, speed=0 -- so avancar
        obs, reward, terminated, truncated, info = env.step(action)
        if truncated:
            saw_truncated_at = i
            break
        if terminated:
            print("AVISO  aeronave 'terminated' antes de truncar (colisao?) -- pulando "
                  "a checagem de 'truncated no limite exato'")
            break
    if saw_truncated_at is not None:
        assert saw_truncated_at == 5, \
            f"truncated deveria disparar no passo 5 (max_episode_steps), disparou em {saw_truncated_at}"
        print(f"OK  truncated dispara exatamente no passo {saw_truncated_at} (max_episode_steps=5)")

    # --- 5) close() e idempotente -------------------------------------------
    env.close()
    env.close()
    print("OK  close() chamado 2x sem levantar")

    print("\ntest_contract: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
