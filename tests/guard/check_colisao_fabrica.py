#!/usr/bin/env python3
"""Guarda: nenhum PAR de modelos "de verdade" sob models/player/ pode
publicar o MESMO nome de fabrica.

Por que isso e fatal, nao so feio: libs/xplugin/PluginRegistry.cpp,
loadModule(), passo "colisao" -- se dois .so carregados no MESMO processo
tentam registrar o mesmo nome, o segundo chama die() (std::exit) na hora.
Ja aconteceu de verdade uma vez (ThreadTagProbe, A4 vs missile -- ver
CLAUDE.md, "vigesima terceira passada") e foi corrigido renomeando o lado
que nao e producao. Esta guarda existe para essa classe de erro nao
precisar ser descoberta rodando de novo da proxima vez.

Descobre os modelos por find sob models/player/ (mesma filosofia de
check_modelo_estrutura.sh/check_falcons_estrutura.sh: um modelo novo ja
entra na checagem, sem editar este arquivo) -- EXCETO models/player/
fixtures/<nome>/, que existem justamente para IMITAR o contrato de outro
modelo (docs/CONTRATO.md) e nunca sao carregados JUNTO com o modelo que
imitam; colisao ali e o proposito, nao um bug (ver CLAUDE.md sobre o
stub reaproveitando os mesmos nomes de A4 de proposito) -- e EXCETO
models/player/template/, pelo mesmo motivo conceitual: nunca e producao,
nenhum cenario aponta pra ele, nunca e carregado ao lado de outro modelo
(models/README.md, secao 2.4). Comparar os nomes de exemplo dele
(ExampleState/ExampleBehavior/...) contra A4/missile seria ruido, nao
sinal.

Limitacao conhecida, documentada em vez de escondida: so olha o que cada
models/player/<nome>/src/ implementa DIRETAMENTE. Uma classe compartilhada
de fora (ex.: models/events/, hoje so consumida por A4 -- ver
xnative/factory.cpp, que acrescenta TacticalAlert ao proprio NOMES[]) nao
entra nesta varredura; se um segundo modelo um dia tambem passar a
publicar uma classe de models/events/, confira a mao contra o que aquele
modelo de fato acrescenta ao proprio NOMES[]/plugin.cpp.

Uso: python3 tests/guard/check_colisao_fabrica.py
"""
from __future__ import annotations

import sys
from itertools import combinations
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools"))

import extract_execution_chain as ext  # noqa: E402

MODELS_PLAYER = REPO_ROOT / "models" / "player"


NAO_PRODUCAO = {"fixtures", "template"}


def discover_models():
    """{nome: {nomes-de-fabrica}} para cada models/player/<nome>/ que NAO
    seja fixture/template -- ver o "porque" da exclusao no docstring do
    modulo."""
    models = {}
    for d in sorted(MODELS_PLAYER.iterdir()):
        if not d.is_dir() or d.name in NAO_PRODUCAO:
            continue
        src = d / "src"
        if not src.exists():
            continue
        factory_map = ext.build_factory_map([src])
        models[d.name] = set(factory_map.keys())
    return models


def main():
    models = discover_models()
    if len(models) < 2:
        print(f"OK -- so {len(models)} modelo(s) sob models/player/ (fora {'/'.join(sorted(NAO_PRODUCAO))}/); nada para comparar.")
        return 0

    failures = []
    for (name_a, names_a), (name_b, names_b) in combinations(sorted(models.items()), 2):
        overlap = names_a & names_b
        if overlap:
            failures.append(
                f"{name_a} e {name_b} publicam o(s) MESMO(S) nome(s) de fabrica {sorted(overlap)} -- "
                f"isso derruba o processo (die()) na hora se algum .edl algum dia carregar os dois "
                f".so juntos (libs/xplugin/PluginRegistry.cpp, passo 'colisao'). Renomeie a classe "
                f"do lado que NAO e producao (ver CLAUDE.md, precedente 'ThreadTagProbe' -> "
                f"'MissileThreadTagProbe')."
            )

    if failures:
        print(f"FALHOU ({len(failures)}):")
        for f in failures:
            print(" -", f)
        return 1

    pairs = len(models) * (len(models) - 1) // 2
    print(f"OK -- {len(models)} modelo(s) ({', '.join(sorted(models))}), {pairs} par(es) comparado(s), "
          f"nenhuma colisao de nome de fabrica.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
