"""Lista unica dos cenarios .edl/.edl.in REAIS deste repositorio (producao +
fixtures), usada por test_edlcheck.py/test_edl_lint.py/test_edl_catalog.py.

Achado por auditoria: os tres arquivos mantinham copias byte-identicas desta
lista, cada uma editada a mao toda vez que um cenario nascia/morria (ex.: a
"Vigesima sexta passada" do CLAUDE.md, que reduziu os cenarios do ./app de
tres para um, teve que tocar os tres arquivos). Extraida para ca.

Deliberadamente NAO virou descoberta por 'find' (o padrao de
tests/guard/check_cenario_plugin.sh::cenarios_de_fonte()): aquele glob pega
TODO .edl/.edl.in do repositorio, inclusive sandbox/ (cenarios grandes de
6-DOF, nao pensados para rodar em CI) e qualquer preset solto em
src/ui/presets/ -- um universo mais amplo e mais instavel do que o que estes
tres testes querem afirmar (a lista curada abaixo e' precisamente "cenarios
de producao mais os dois de fixture que src/ui/scripts/build.js e os testes
de -folder/porta dupla consomem", ver CLAUDE.md secao
"built-in_mixr_1/full-systems-nav"). Trocar para descoberta ampla mudaria o
que estes testes cobrem, nao so removeria duplicacao -- fora do escopo desta
correcao.
"""
from __future__ import annotations

REAL_SCENARIOS = [
    "src/poc/dis/flight/configs/scenario.edl.in",
    "tests/fixtures/built-in_mixr_1/configs/scenario_max_player.edl.in",
    "tests/fixtures/full-systems-nav/configs/scenario_full_nav.edl.in",
    "src/poc/onnx-policy/configs/scenario.edl.in",
    "src/poc/python-flight/configs/scenario.edl.in",
    "src/poc/dis/bandit/configs/scenario.edl",
    "src/poc/rl-training/configs/scenario_rl.edl",
    "src/rl/configs/scenario_rl.edl",
]
