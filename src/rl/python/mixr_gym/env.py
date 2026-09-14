"""gymnasium.Env sobre a simulacao MIXR/flight -- ver src/rl/README.md.

O 'state' e o mesmo domain::WorldView que o UBF ja usa para decidir
(models/players/A-4/include/domain/WorldView.hpp); a 'action' e o mesmo
domain::FlightCommand que xnative::FlightAction ja aplica no Autopilot
(heading/altitude/speed -- os tres unicos campos que execute() de fato
atua). Nada disso e reimplementado aqui: o C++ (src/rl/bindings/) so publica os
dois em libs/xrlbridge e este arquivo os traduz para os tipos do
Gymnasium.
"""

from __future__ import annotations

import os
from typing import Any, Callable, Optional, Sequence, Union

import numpy as np
import gymnasium as gym
from gymnasium import spaces

from . import _native

# Relativo a raiz do repositorio -- MESMA convencao de todo binario deste
# projeto (ver CLAUDE.md: "todos os binarios leem configs/ por caminho
# relativo e devem ser executados a partir da raiz do repositorio"). O
# processo Python que importa mixr_gym tem que ter cwd = raiz do repo.
DEFAULT_SCENARIO = "./src/rl/configs/scenario_rl.edl"
DEFAULT_PLAYER = "falcon1"

# OS LIMITES do observation_space -- GENEROSOS, nao fisicos: so para dar ao
# espaco uma faixa finita. O valor de verdade sempre vem do WorldView; nada
# aqui recorta o dado, so declara a forma do espaco.
#
# A ORDEM nao esta aqui. Ela vem do C++ (_native.observation_field_names(),
# que expande a X-macro de libs/xrlbridge/ObservationFields.hpp -- a MESMA
# que o modelo expande contra domain::WorldView na hora de inferir). Este dict
# so associa um limite a cada nome, e _build_field_lists() confere que os dois
# conjuntos batem EXATAMENTE: um campo novo no C++ sem limite aqui, ou um
# limite aqui sem campo la, derruba na importacao em vez de virar uma politica
# que voa errado.
_BOUNDS: dict[str, tuple[float, float]] = {
    "northM": (-50_000.0, 50_000.0),
    "eastM": (-50_000.0, 50_000.0),
    "altitudeM": (-1_000.0, 15_000.0),
    "headingDeg": (0.0, 360.0),
    "speedKts": (0.0, 600.0),
    "rollDeg": (-180.0, 180.0),
    "pitchDeg": (-90.0, 90.0),
    "fuelFraction": (0.0, 1.0),
    "mach": (0.0, 3.0),
    "gLoad": (-10.0, 10.0),
    "alphaDeg": (-90.0, 90.0),
    "terrainElevM": (-1_000.0, 9_000.0),
    "altitudeAglM": (-1_000.0, 15_000.0),
    "contactRangeM": (0.0, 200_000.0),
    "contactRelBearingDeg": (-180.0, 180.0),
    "contactDeltaAltM": (-15_000.0, 15_000.0),
    "contactNorthM": (-200_000.0, 200_000.0),
    "contactEastM": (-200_000.0, 200_000.0),
    "contactAltitudeM": (-1_000.0, 15_000.0),
    "alertNorthM": (-200_000.0, 200_000.0),
    "alertEastM": (-200_000.0, 200_000.0),
    "alertAltitudeM": (-1_000.0, 15_000.0),
    "alertRangeM": (0.0, 200_000.0),
    # RWR + navegacao nativa -- acrescentados nesta mesma passada (existiam em
    # domain::WorldView havia tempo, mas a macro canonica so passou a
    # lista-los agora -- ver libs/xrlbridge/ObservationFields.hpp). Nao entram
    # no schema "classic28" (o default de MixrFlightEnv) -- so ficam visiveis
    # com fields="all" ou uma lista explicita que os inclua.
    "rwrThreatRangeM": (0.0, 200_000.0),
    "rwrThreatRelBearingDeg": (-180.0, 180.0),
    "rwrThreatDeltaAltM": (-15_000.0, 15_000.0),
    "navTrueBrgDeg": (-180.0, 180.0),
    "navCmdAltM": (-1_000.0, 15_000.0),
    "navCmdSpeedKts": (0.0, 600.0),
}


def _build_field_lists() -> tuple[list[tuple[str, float, float]], list[str]]:
    """Deriva as duas listas da ordem canonica do C++, e verifica o contrato."""
    canonicos = list(_native.observation_field_names())
    booleanos = set(_native.observation_bool_fields())

    floats = [n for n in canonicos if n not in booleanos]
    faltando = [n for n in floats if n not in _BOUNDS]
    sobrando = [n for n in _BOUNDS if n not in floats]
    if faltando or sobrando:
        raise RuntimeError(
            "contrato de observacao fora de sincronia entre C++ e Python:\n"
            f"  sem limite em env.py: {faltando}\n"
            f"  limite sem campo no C++: {sobrando}\n"
            "  a ordem canonica vive em libs/xrlbridge/ObservationFields.hpp"
        )

    return ([(n, _BOUNDS[n][0], _BOUNDS[n][1]) for n in floats],
            [n for n in canonicos if n in booleanos])


# Campos de TEXTO do WorldView (contactName/alertSender/alertContactName)
# ficam de fora do observation_space de proposito -- nao sao RL-friendly.
# Continuam disponiveis em info["raw_state"] para debug/log.
#
# Este e' o CATALOGO COMPLETO (todos os 38 campos numericos/booleanos que o
# C++ publica) -- nao o observation_space de nenhum MixrFlightEnv em
# particular. Qual SUBCONJUNTO entra no observation_space de UMA instancia e'
# decidido pelo parametro 'fields' do construtor (ver _resolve_fields()
# abaixo) -- por default, so os 28 historicos ("classic28"), para nao mudar o
# formato de ninguem que ja usa MixrFlightEnv sem pedir os campos novos.
_FLOAT_FIELDS, _BOOL_FIELDS = _build_field_lists()
_BOUNDS_BY_NAME: dict[str, tuple[float, float]] = {n: (lo, hi) for n, lo, hi in _FLOAT_FIELDS}
_BOOL_FIELD_SET: set[str] = set(_BOOL_FIELDS)


def _resolve_fields(fields: Union[str, Sequence[str]]) -> list[str]:
    """Traduz o parametro 'fields' do construtor numa lista de nomes.

    "classic28" (default) e' o schema historico, hardcoded do lado C++
    (xrlbridge::classicSchema28()) -- nunca deriva da ordem atual de
    observation_field_names(), pelo mesmo motivo que o C++ documenta: um
    .onnx/checkpoint ja treinado nao pode ter a forma da entrada mudada por
    baixo dele so' porque a lista canonica cresceu.
    """
    if fields == "classic28":
        return list(_native.classic_schema_28())
    if fields == "all":
        return list(_native.observation_field_names())

    nomes = list(fields)
    desconhecidos = [n for n in nomes if n not in _BOUNDS_BY_NAME and n not in _BOOL_FIELD_SET]
    if desconhecidos:
        raise ValueError(
            f"fields contem nome(s) desconhecido(s): {desconhecidos}\n"
            f"  campos validos: {sorted(_BOUNDS_BY_NAME) + sorted(_BOOL_FIELD_SET)}"
        )
    return nomes


def _build_observation_space(field_names: list[str]) -> spaces.Dict:
    fields: dict[str, spaces.Space] = {}
    for name in field_names:
        if name in _BOOL_FIELD_SET:
            fields[name] = spaces.Discrete(2)
        else:
            low, high = _BOUNDS_BY_NAME[name]
            fields[name] = spaces.Box(low=low, high=high, shape=(1,), dtype=np.float32)
    return spaces.Dict(fields)


def _to_obs(raw: dict[str, Any], field_names: list[str]) -> dict[str, Any]:
    obs: dict[str, Any] = {}
    for name in field_names:
        if name in _BOOL_FIELD_SET:
            obs[name] = int(bool(raw[name]))
        else:
            obs[name] = np.array([raw[name]], dtype=np.float32)
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
    src/rl/configs/scenario_rl.edl) -- API padrao gymnasium.Env, single-agent.
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
        fields: Union[str, Sequence[str]] = "classic28",
    ) -> None:
        """'fields' escolhe quais campos entram no observation_space, e em que
        ordem (dentro do Dict -- ordem so' importa de verdade para quem
        depois achata o dict, ver src/poc/rl-training/flatten_obs.py):
        "classic28" (default -- os 28 historicos, o observation_space de
        sempre), "all" (os 38 completos, incluindo RWR/navegacao) ou uma
        lista explicita de nomes (ver mixr_gym._native.observation_field_names()
        para o catalogo completo).
        """
        super().__init__()
        self._field_names = _resolve_fields(fields)

        # 'isfile', nao 'exists': um DIRETORIO passado por engano tambem
        # satisfaz 'exists', e o unico ponto de checagem do lado Python era
        # esse -- sem ele, o caminho segue ate o parser flex vendorizado do
        # MIXR (contexts/src/mixr/src/base/edl_parser/EdlScanner.cpp), que
        # chama exit(2) DIRETO em 'yyFlexLexer::LexerError()' ao falhar de
        # ler um diretorio como arquivo -- matando o interprete Python sem
        # excecao nenhuma, fora do alcance de qualquer try/except daqui.
        # Confirmado reproduzindo antes desta correcao.
        if not os.path.isfile(scenario_path):
            raise FileNotFoundError(
                f"cenario '{scenario_path}' nao encontrado (ou nao e um arquivo) -- "
                "rode com cwd na raiz do repositorio (mesma convencao dos binarios "
                "deste projeto)"
            )

        self._sim = _native.NativeSimulation(scenario_path, player_name)
        self._max_episode_steps = max_episode_steps
        self._reward_fn = reward_fn or default_reward
        self._step_count = 0
        self._prev_raw: Optional[dict[str, Any]] = None

        self.observation_space = _build_observation_space(self._field_names)
        low = np.array([heading_range[0], altitude_range_m[0], speed_range_kts[0]], dtype=np.float32)
        high = np.array([heading_range[1], altitude_range_m[1], speed_range_kts[1]], dtype=np.float32)
        self.action_space = spaces.Box(low=low, high=high, dtype=np.float32)

    def reset(self, *, seed: Optional[int] = None, options: Optional[dict] = None):
        super().reset(seed=seed)
        raw = self._sim.reset()
        self._step_count = 0
        self._prev_raw = raw
        return _to_obs(raw, self._field_names), {"raw_state": raw}

    def step(self, action: np.ndarray):
        heading_deg, altitude_m, speed_kts = (float(a) for a in action)
        raw, terminated = self._sim.step(heading_deg, altitude_m, speed_kts)

        self._step_count += 1
        truncated = self._step_count >= self._max_episode_steps

        reward = self._reward_fn(raw, self._prev_raw, terminated)
        self._prev_raw = raw

        obs = _to_obs(raw, self._field_names)
        info = {"raw_state": raw}
        return obs, reward, terminated, truncated, info

    def close(self) -> None:
        self._sim.close()
