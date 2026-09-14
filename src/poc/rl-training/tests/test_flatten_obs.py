#!/usr/bin/env python3
"""FlattenedObservation -- confirma que o achatamento DEFAULT (sem 'fields'
explicito) continua produzindo o Box(28,) de sempre, na mesma ordem
historica -- mesmo agora que mixr_gym._native.observation_field_names()
devolve 38 nomes (RWR + navegacao, ver libs/xrlbridge/ObservationFields.hpp).

REGRESSAO QUE ISTO TRAVA: antes desta passada, FlattenedObservation(env) sem
argumento usava observation_field_names() diretamente -- se a lista canonica
cresce (como cresceu, de 28 para 38), o vetor de entrada de qualquer politica
ja treinada mudaria de forma por baixo, em silencio. Este teste falha
(Box(38,) em vez de Box(28,)) se essa reversao acontecer de novo.

Processo PROPRIO: cria a UNICA Station deste processo (mesma armadilha
documentada em src/rl/tests/test_smoke.py).
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

if not os.path.exists("./src/rl/configs/scenario_rl.edl"):
    print("ERRO: rode este script com cwd na raiz do repositorio", file=sys.stderr)
    sys.exit(1)

from mixr_gym import MixrFlightEnv, _native  # noqa: E402  (import antes de numpy)
from flatten_obs import FlattenedObservation  # noqa: E402


def main() -> int:
    falhou = False

    env = FlattenedObservation(MixrFlightEnv(max_episode_steps=2))

    esperado = list(_native.classic_schema_28())
    if env._names != esperado:
        print(f"FALHOU: default deveria usar classic_schema_28() ({len(esperado)} campos), "
              f"usa {len(env._names)}: {env._names}", file=sys.stderr)
        falhou = True
    else:
        print("OK  FlattenedObservation() default usa classic_schema_28(), na mesma ordem")

    if env.observation_space.shape != (28,):
        print(f"FALHOU: observation_space deveria ser Box(28,), e {env.observation_space.shape}",
              file=sys.stderr)
        falhou = True
    else:
        print("OK  observation_space e Box(28,)")

    obs, _info = env.reset()
    if obs.shape != (28,):
        print(f"FALHOU: reset() deveria devolver vetor de 28 posicoes, devolveu {obs.shape}",
              file=sys.stderr)
        falhou = True
    else:
        print("OK  reset() devolve vetor de 28 posicoes")
    env.close()

    if falhou:
        return 1
    print("\ntest_flatten_obs: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
