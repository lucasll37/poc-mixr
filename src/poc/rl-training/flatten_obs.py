"""FlattenedObservation -- achata a observacao de MixrFlightEnv para o
contrato .onnx de producao. Ver train.py e notebooks/train.ipynb, os dois
consumidores desta classe.
"""

from __future__ import annotations

import mixr_gym
import numpy as np
from gymnasium import ObservationWrapper, spaces


class FlattenedObservation(ObservationWrapper):
    """Achata o Dict/Discrete de MixrFlightEnv num Box(28,) plano, na ORDEM
    CANONICA do C++ (mixr_gym._native.observation_field_names()).

    Existe porque o contrato do .onnx que OnnxPolicyAction consome e
    'float32[1,28] -> float32[1,3]' (ver tools/export_onnx.py) -- sem Dict,
    sem Discrete. E o formato que torch.onnx.export produz de graca para uma
    MlpPolicy comum. MEDIDO QUEBRANDO: uma 'MultiInputPolicy' treinada
    direto sobre o Dict/Discrete de MixrFlightEnv NAO exporta para esse
    contrato -- CombinedExtractor exige um dict na entrada
    ('AssertionError: Expected dict, got Tensor' dentro de
    stable_baselines3/common/preprocessing.py::preprocess_obs()), e mesmo
    contornando isso o vetor de features que ela produz nao tem 28
    posicoes (os 5 campos Discrete(2) viram one-hot, 10 posicoes, nao 5).

    Por que aqui e nao dentro de MixrFlightEnv: o Dict/Discrete e ergonomia
    do lado Python da BIBLIOTECA (src/rl) -- dict com nome de campo,
    Discrete(2) para os booleanos -- documentada como contrato publico no
    README dela. Mudar isso mudaria o contrato pra quem ja consome o Dict
    diretamente. Achatar aqui, no CONSUMIDOR, resolve o problema sem tocar
    a biblioteca.
    """

    def __init__(self, env):
        super().__init__(env)
        self._names = list(mixr_gym._native.observation_field_names())

        low, high = [], []
        for name in self._names:
            sub = env.observation_space[name]
            if isinstance(sub, spaces.Box):
                low.append(float(sub.low[0]))
                high.append(float(sub.high[0]))
            else:  # Discrete(2) -- os campos booleanos
                low.append(0.0)
                high.append(1.0)
        self.observation_space = spaces.Box(
            low=np.array(low, dtype=np.float32),
            high=np.array(high, dtype=np.float32),
            dtype=np.float32,
        )

    def observation(self, obs: dict) -> np.ndarray:
        return np.array(
            [float(np.asarray(obs[name]).reshape(-1)[0]) for name in self._names],
            dtype=np.float32,
        )
