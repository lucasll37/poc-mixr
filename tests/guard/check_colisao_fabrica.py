#!/usr/bin/env python3
"""Guarda: nenhum PAR de projetos de modelo sob models/ pode publicar o
MESMO nome de fabrica.

Por que isso e fatal, nao so feio: libs/xplugin/PluginRegistry.cpp,
loadModule(), passo "colisao" -- se dois .so carregados no MESMO processo
tentam registrar o mesmo nome, o segundo chama die() na hora. Ja
aconteceu (ThreadTagProbe, A-4 vs. o extinto modelo missile), corrigido
renomeando o lado que nao e producao. Esta guarda existe para essa classe
de erro nao voltar a ocorrer em silencio.

Descobre os modelos por find sob models/ INTEIRO, a QUALQUER profundidade
(MESMA descoberta de MODELOS_PRODUCAO no Makefile raiz e de
check_modelo_estrutura.sh/check_modelo_fresco.sh: todo diretorio com um
meson.build cujo 'project()' esta na raiz do proprio arquivo) -- nunca por
posicao fixa (models/<categoria>/<nome>/), que ficaria cega assim que uma
categoria ganhasse subpastas proprias (foi exatamente o que aconteceu
quando players/ ganhou air/effect/ground/space/weapon/: a versao anterior
desta guarda, escrita para "duas iteracoes de profundidade", parou de achar
QUALQUER modelo de producao em silencio). Um modelo novo ja entra na
checagem, sem editar este arquivo -- EXCETO models/template/, que existe
justamente para IMITAR o contrato de outro modelo (docs/CONTRATO.md, via o
mirror `src/mirror.cpp`) e nunca e carregado JUNTO com o modelo que imita;
colisao ali e o proposito, nao um bug. Comparar os nomes do mirror (que
reusa de proposito os mesmos 9 nomes de A-4) OU os nomes de exemplo do
scaffold (ExampleState/ExampleBehavior/...) contra A-4 seria ruido, nao
sinal.

Limitacao conhecida, documentada em vez de escondida: so olha o que cada
projeto de modelo (models/.../<nome>/) src/ implementa DIRETAMENTE. Uma
classe compartilhada de fora (ex.: models/events/, hoje so consumida por
A-4 -- ver xnative/factory.cpp, que acrescenta TacticalAlert ao proprio
NOMES[]) nao entra nesta varredura; se um segundo modelo um dia tambem
passar a publicar uma classe de models/events/, confira a mao contra o que
aquele modelo de fato acrescenta ao proprio NOMES[]/plugin.cpp.

Uso: python3 tests/guard/check_colisao_fabrica.py
"""
from __future__ import annotations

import re
import sys
from itertools import combinations
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools"))

import extract_execution_chain as ext  # noqa: E402

MODELS_DIR = REPO_ROOT / "models"

# 'template' nunca e' producao (ver o "porque" da exclusao no docstring do
# modulo). models/events/ nao e' uma pasta de projetos-modelo (e' UM projeto
# so', a lib 'events', consumida por subdir()) -- seu meson.build nao tem
# 'project()' na propria raiz, entao ja fica de fora da descoberta abaixo
# sem precisar de exclusao por nome.
NAO_PRODUCAO = {"template"}

_PROJECT_RE = re.compile(r"^project\b", re.M)


def discover_models():
    """{caminho-relativo: {nomes-de-fabrica}} para cada projeto de modelo sob
    models/ (a QUALQUER profundidade) que NAO seja template -- ver o
    "porque" da exclusao no docstring do modulo. A chave e' o caminho
    relativo completo (nao so' o nome da pasta) porque dois modelos podem,
    em tese, ter o mesmo NOME em categorias diferentes, e as duas entradas
    precisam coexistir no mapa."""
    models = {}
    for meson_build in sorted(MODELS_DIR.glob("**/meson.build")):
        rel_parts = meson_build.relative_to(MODELS_DIR).parts
        # poda build/dist/subprojects/tests/tools -- MESMOS '-not -path' que
        # MODELOS_PRODUCAO (Makefile raiz) usa: um meson.build de
        # subdiretorio (ex.: tests/meson.build) so' tem subdir(), nunca
        # project().
        if any(p in ("build", "dist", "subprojects", "tests", "tools") for p in rel_parts[:-1]):
            continue
        d = meson_build.parent
        if d.name in NAO_PRODUCAO:
            continue
        try:
            texto = meson_build.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if not _PROJECT_RE.search(texto):
            continue
        src = d / "src"
        if not src.exists():
            continue
        factory_map = ext.build_factory_map([src])
        models[str(d.relative_to(REPO_ROOT))] = set(factory_map.keys())
    return models


def main():
    models = discover_models()
    if len(models) < 2:
        print(f"OK -- so {len(models)} modelo(s) sob models/ (fora {'/'.join(sorted(NAO_PRODUCAO))}/); nada para comparar.")
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
