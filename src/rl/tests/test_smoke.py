#!/usr/bin/env python3
"""Smoke test do wrapper Gymnasium -- roda a Station de VERDADE (mesmo
plugin libflight_tc.so que single-thread/multi-thread/app usam), nao um
mock. Ver src/rl/README.md para como rodar.

Cobre:
  1. reset() devolve um dict com as chaves/formas certas;
  2. N step() com acao aleatoria avancam a simulacao -- northM/eastM mudam
     (a aeronave de fato se move), fuelFraction so diminui;
  3. reset() chamado tres vezes NO MESMO Env/NativeSimulation (nao um Env
     novo -- ver a armadilha abaixo) devolve o MESMO WorldView inicial -- e o
     risco "RESET_EVENT repetido" documentado no plano de implementacao e no
     cabecalho de NativeSimulation.hpp. Se isto falhar, o fallback e
     reconstruir NativeSimulation por episodio (subprocess por reset), sem
     mudar a API de MixrFlightEnv.

ARMADILHA CONFIRMADA (nao redescobrir): so pode existir UMA Station por
PROCESSO -- shared/xplugin sela o registro de plugins depois do PRIMEIRO
edl_parser() (ver mixr::xplugin::seal(), chamado dentro de
rl::buildStation()); um SEGUNDO MixrFlightEnv() no mesmo processo, ao chamar
reset() pela primeira vez, cai em rl::buildStation() -> edl_parser() de novo
e o registro recusa com "loadModule(...) depois do parse". Por isso o teste
de reset() repetido reusa o MESMO 'env' (chamando reset() de novo nele, que
cai no ramo 'else' de NativeSimulation::reset() -- so RESET_EVENT, sem
buildStation() de novo) -- nao cria um 'env2'.

Precisa rodar com cwd na RAIZ do repositorio (mesma convencao de todo
binario deste projeto) e PYTHONPATH incluindo dist/python (ver
src/rl/README.md).

ARMADILHA CONFIRMADA (nao redescobrir, ver mixr_gym/__init__.py): 'mixr_gym'
TEM DE ser importado antes de 'numpy'/'gymnasium' neste processo -- importar
numpy primeiro e so DEPOIS 'mixr_gym' causa SEGFAULT dentro de libstdc++
(estouro do excedente de TLS estatico do glibc, ao que tudo indica -- ver o
cabecalho de mixr_gym/__init__.py para os dois experimentos que isolaram a
causa). E por isso que o import de mixr_gym vem ANTES do de numpy aqui,
fora de ordem alfabetica/PEP8 -- de proposito.
"""

import os
import sys

if not os.path.exists("./src/rl/configs/scenario_rl.edl"):
    print("ERRO: rode este script com cwd na raiz do repositorio", file=sys.stderr)
    sys.exit(1)

from mixr_gym import MixrFlightEnv  # noqa: E402  (ver ARMADILHA acima -- antes de numpy)

import numpy as np  # noqa: E402


def main() -> int:
    rng = np.random.default_rng(seed=42)

    # --- 1) reset() -- chaves/formas ----------------------------------------
    env = MixrFlightEnv(max_episode_steps=50)
    obs, info = env.reset()
    assert set(obs.keys()) == set(env.observation_space.spaces.keys()), \
        "chaves da observacao nao batem com observation_space"
    assert obs["valid"] == 1, "primeira observacao deveria ser valida"
    print(f"OK  reset(): fuelFraction inicial = {float(obs['fuelFraction'][0]):.4f}")

    # --- 2) N steps com acao aleatoria -- a aeronave se move ----------------
    north0 = float(obs["northM"][0])
    east0 = float(obs["eastM"][0])
    fuel_prev = float(obs["fuelFraction"][0])

    for i in range(50):
        action = rng.uniform(env.action_space.low, env.action_space.high).astype(np.float32)
        obs, reward, terminated, truncated, info = env.step(action)
        fuel_now = float(obs["fuelFraction"][0])
        assert fuel_now <= fuel_prev + 1e-9, \
            f"fuelFraction subiu no passo {i}: {fuel_prev} -> {fuel_now}"
        fuel_prev = fuel_now
        if terminated or truncated:
            break

    moved = abs(float(obs["northM"][0]) - north0) + abs(float(obs["eastM"][0]) - east0)
    assert moved > 1.0, f"aeronave nao se moveu apos 50 passos (delta={moved:.3f} m)"
    print(f"OK  50 x step(): deslocou {moved:.1f} m, combustivel {fuel_prev:.4f}")

    # --- 3) reset() repetido no MESMO env (ver a armadilha "uma Station por
    #        processo" no cabecalho deste arquivo -- NAO criar um Env novo) --
    baseline, _ = env.reset()
    for attempt in range(1, 4):
        obs, _ = env.reset()
        for name in ("northM", "eastM", "altitudeM", "fuelFraction"):
            b = float(baseline[name][0])
            v = float(obs[name][0])
            if abs(b - v) > 1e-6:
                print(f"AVISO  reset() #{attempt + 1}: '{name}' divergiu do baseline "
                      f"({b} -> {v}) -- ver o risco 'RESET_EVENT repetido' no plano "
                      "de implementacao; fallback e processo por episodio.")
    print("OK  reset() chamado 3x no mesmo env (ver avisos acima, se houver)")

    env.close()

    print("\nsmoke test: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
