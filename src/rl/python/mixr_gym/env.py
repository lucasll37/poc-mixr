"""gymnasium.Env sobre a simulacao MIXR/flight -- ver src/rl/README.md.

O 'state' e o mesmo domain::WorldView que o UBF ja usa para decidir
(models/flight/include/domain/WorldView.hpp); a 'action' e o mesmo
domain::FlightCommand que xnative::FlightAction ja aplica no Autopilot
(heading/altitude/speed -- os tres unicos campos que execute() de fato
atua). Nada disso e reimplementado aqui: o C++ (src/rl/bindings/) so publica os
dois em shared/xrlbridge e este arquivo os traduz para os tipos do
Gymnasium.
"""

from __future__ import annotations

import os
from typing import Any, Callable, Optional

import numpy as np
import gymnasium as gym
from gymnasium import spaces

from . import _native

# Relativo a raiz do repositorio -- MESMA convencao de todo binario deste
# projeto (ver CLAUDE.md: "todos os binarios leem configs/ por caminho
# relativo e devem ser executados a partir da raiz do repositorio"). O
# processo Python que importa mixr_gym tem que ter cwd = raiz do repo.
DEFAULT_SCENARIO = "./src/rl/configs/scenario_rl.epp"
DEFAULT_PLAYER = "falcon1"

# (nome, low, high) -- limites GENEROSOS, nao fisicos: so para dar ao
# observation_space uma faixa finita. O valor de verdade sempre vem do
# WorldView; nada aqui recorta o dado, so declara a forma do espaco.
_FLOAT_FIELDS: list[tuple[str, float, float]] = [
    ("northM", -50_000.0, 50_000.0),
    ("eastM", -50_000.0, 50_000.0),
    ("altitudeM", -1_000.0, 15_000.0),
    ("headingDeg", 0.0, 360.0),
    ("speedKts", 0.0, 600.0),
    ("rollDeg", -180.0, 180.0),
    ("pitchDeg", -90.0, 90.0),
    ("fuelFraction", 0.0, 1.0),
    ("mach", 0.0, 3.0),
    ("gLoad", -10.0, 10.0),
    ("alphaDeg", -90.0, 90.0),
    ("terrainElevM", -1_000.0, 9_000.0),
    ("altitudeAglM", -1_000.0, 15_000.0),
    ("contactRangeM", 0.0, 200_000.0),
    ("contactRelBearingDeg", -180.0, 180.0),
    ("contactDeltaAltM", -15_000.0, 15_000.0),
    ("contactNorthM", -200_000.0, 200_000.0),
    ("contactEastM", -200_000.0, 200_000.0),
    ("contactAltitudeM", -1_000.0, 15_000.0),
    ("alertNorthM", -200_000.0, 200_000.0),
    ("alertEastM", -200_000.0, 200_000.0),
    ("alertAltitudeM", -1_000.0, 15_000.0),
    ("alertRangeM", 0.0, 200_000.0),
]

# Campos de TEXTO do WorldView (contactName/alertSender/alertContactName)
# ficam de fora do observation_space de proposito -- nao sao RL-friendly.
# Continuam disponiveis em info["raw_state"] para debug/log.
_BOOL_FIELDS: list[str] = ["valid", "terrainValid", "hasContact", "hasAlert", "weaponReady"]


def _build_observation_space() -> spaces.Dict:
    fields: dict[str, spaces.Space] = {}
    for name, low, high in _FLOAT_FIELDS:
        fields[name] = spaces.Box(low=low, high=high, shape=(1,), dtype=np.float32)
    for name in _BOOL_FIELDS:
        fields[name] = spaces.Discrete(2)
    return spaces.Dict(fields)


def _to_obs(raw: dict[str, Any]) -> dict[str, Any]:
    obs: dict[str, Any] = {}
    for name, _low, _high in _FLOAT_FIELDS:
        obs[name] = np.array([raw[name]], dtype=np.float32)
    for name in _BOOL_FIELDS:
        obs[name] = int(bool(raw[name]))
    return obs


def default_reward(obs_raw: dict[str, Any], prev_raw: Optional[dict[str, Any]],
                    terminated: bool) -> float:
    """Ponto de partida MINIMO -- nao e a resposta de nenhuma pesquisa.

    Custo pequeno por passo (favorece eficiencia) mais uma penalidade grande
    se o episodio terminou em colisao (terminated == True e o avio nao
    chegou la por 'reset()'). Substitua via o parametro 'reward_fn' do
    construtor de MixrFlightEnv -- a forma certa de recompensa depende do
    que se esta pesquisando, e nao e uma decisao de infraestrutura.
    """
    if terminated:
        return -100.0
    return -0.01


class MixrFlightEnv(gym.Env):
    """Um agente RL controla UMA aeronave (default: falcon1); as demais do
    cenario continuam decidindo pela arvore de comportamento nativa (ver
    src/rl/configs/scenario_rl.epp) -- API padrao gymnasium.Env, single-agent.
    """

    metadata = {"render_modes": []}

    def __init__(
        self,
        scenario_path: str = DEFAULT_SCENARIO,
        player_name: str = DEFAULT_PLAYER,
        max_episode_steps: int = 2000,
        heading_range: tuple[float, float] = (0.0, 360.0),
        altitude_range_m: tuple[float, float] = (0.0, 8_000.0),
        speed_range_kts: tuple[float, float] = (0.0, 400.0),
        reward_fn: Optional[Callable[[dict, Optional[dict], bool], float]] = None,
    ) -> None:
        super().__init__()

        if not os.path.exists(scenario_path):
            raise FileNotFoundError(
                f"cenario '{scenario_path}' nao encontrado -- rode com cwd na raiz "
                "do repositorio (mesma convencao dos binarios deste projeto)"
            )

        self._sim = _native.NativeSimulation(scenario_path, player_name)
        self._max_episode_steps = max_episode_steps
        self._reward_fn = reward_fn or default_reward
        self._step_count = 0
        self._prev_raw: Optional[dict[str, Any]] = None

        self.observation_space = _build_observation_space()
        low = np.array([heading_range[0], altitude_range_m[0], speed_range_kts[0]], dtype=np.float32)
        high = np.array([heading_range[1], altitude_range_m[1], speed_range_kts[1]], dtype=np.float32)
        self.action_space = spaces.Box(low=low, high=high, dtype=np.float32)

    def reset(self, *, seed: Optional[int] = None, options: Optional[dict] = None):
        super().reset(seed=seed)
        raw = self._sim.reset()
        self._step_count = 0
        self._prev_raw = raw
        return _to_obs(raw), {"raw_state": raw}

    def step(self, action: np.ndarray):
        heading_deg, altitude_m, speed_kts = (float(a) for a in action)
        raw, terminated = self._sim.step(heading_deg, altitude_m, speed_kts)

        self._step_count += 1
        truncated = self._step_count >= self._max_episode_steps

        reward = self._reward_fn(raw, self._prev_raw, terminated)
        self._prev_raw = raw

        obs = _to_obs(raw)
        info = {"raw_state": raw}
        return obs, reward, terminated, truncated, info

    def close(self) -> None:
        self._sim.close()
