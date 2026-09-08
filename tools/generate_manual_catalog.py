#!/usr/bin/env python3
"""Gera docs/manual/catalog.generated.js: MODEL/FACTORIES/SNIPPETS/STATS que
alimentam docs/manual/doc.jsx -- tanto a aba "Simulação" (Execução, que le
MODEL[cls].wp/.po/.sl/.ch para animar o ciclo de fases) quanto a aba
"Catálogo" (que lista FACTORIES/MODEL/SNIPPETS/STATS inteiros).

Universo: as 7 factories NATIVAS que models/BUILT-IN.md ja usa como escopo --
base, models, simulation, terrain, interop/dis, linkage, recorder -- mais o
plugin de PRODUÇÃO models/players/A-4 (nunca template/, nunca o mirror de
contrato em src/mirror.cpp -- ver .claude/rules/models-plugin.md). Cada
entrada aqui tem despacho REAL num factory.cpp (um branch
'name == X::getFactoryName()' alcançável) -- não "toda classe com
DECLARE_SUBCLASS em algum header", que é o que motivou este gerador (ver
models/BUILT-IN.md e o comentário de cabeçalho de tools/mixr_source_scan.py).
Por construção, TODA entrada aqui é "registrada em fábrica" -- não existe
mais um universo mais amplo de classes declaradas-mas-não-despachadas para
mostrar (essas continuam existindo no fonte, só não entram neste catálogo).

Reaproveita tools/mixr_source_scan.py (mask_source, build_inheritance,
build_factory_map, find_dispatch_reachable_classes, resolve_chain,
extract_slots, iter_files, DECLARE_RE) e tools/extract_execution_chain.py
(find_overrides, TARGET_METHODS) como MÓDULOS -- nada reimplementado.

Uso:
    python3 tools/generate_manual_catalog.py > docs/manual/catalog.generated.js

Chamado automaticamente por 'make docs' (Makefile raiz), antes de
docs/manual/compile.js. O arquivo gerado é COMMITADO (mesmo tratamento que
src/ui/edl_catalog.generated.json já recebe) -- abrir docs/manual/index.html
não exige rodar Python antes.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mixr_source_scan as scan  # noqa: E402
import extract_execution_chain as chain_mod  # noqa: E402

MIXR_INCLUDE = REPO_ROOT / scan.MIXR_INCLUDE_REL
MIXR_SRC = REPO_ROOT / scan.MIXR_SRC_REL
MODELS_DIR = REPO_ROOT / scan.MODELS_REL

# As 7 factories nativas que models/BUILT-IN.md já usa como escopo -- rótulo
# (o que aparece em FACTORIES/MODEL[cls].m e em MOD_ORDER de doc.jsx) ->
# subcaminho sob contexts/src/mixr/{include,src}/mixr/.
NATIVE_MODULES = [
    ("base", "base"),
    ("models", "models"),
    ("simulation", "simulation"),
    ("terrain", "terrain"),
    ("interop/dis", "interop/dis"),
    ("linkage", "linkage"),
    ("recorder", "recorder"),
]

# O plugin de PRODUÇÃO -- varrido contra models/ INTEIRO (não só
# models/players/A-4/), porque uma das 9 classes que o factory.cpp dele
# despacha (TacticalAlert) mora fisicamente em models/events/. A mesma
# disciplina "primeiro achado vence" que extract_slots()/build_inheritance()
# já usam (mixr_source_scan.py, ordem alfabética de arquivo) garante que
# models/players/A-4 (alfabeticamente antes de template/) vence sobre
# qualquer classe de MESMO NOME em models/players/template/.
PLUGIN_LABEL = "plugin:A-4"
PLUGIN_FACTORY_CPP = MODELS_DIR / "players" / "A-4" / "src" / "xnative" / "factory.cpp"

# interop::dis::NetIO/Ntm/Nib redeclaram DECLARE_SUBCLASS com o MESMO nome
# barra da própria classe BASE (DECLARE_SUBCLASS(NetIO, interop::NetIO) --
# build_inheritance() descarta o namespace do 2º argumento, então
# inheritance["NetIO"] vira "NetIO", um self-loop que resolve_chain() não
# atravessa). Mesmo remendo que src/ui/scripts/generate_edl_catalog.py já
# usa: varre interop/common SÓ para completar a cadeia que falta, sob uma
# chave sintética que quebra o self-loop -- as classes de interop/common
# NUNCA entram no catálogo (não é um dos 7 módulos nativos em escopo).
INTEROP_COMMON_INCLUDE = MIXR_INCLUDE / "interop" / "common"

# 4 fases do frame de tempo crítico (mesma ordem/rótulo de PHASES em
# doc.jsx, que continua hand-written lá -- este script só produz ÍNDICES).
PHASE_METHODS = ["dynamics", "transmit", "receive", "process"]


def _clean(name):
    """Descarta o sufixo sintético '@interop_common' do remendo de self-loop
    -- nunca deve vazar pro JSON de saída, só serviu pra achatar
    corretamente por dentro (mesma disciplina de generate_edl_catalog.py)."""
    return name.split("@", 1)[0] if name else name


def _patch_self_loops(inheritance):
    """Acha QUALQUER self-loop em `inheritance` (Tipo -> Tipo) e tenta
    completá-lo lendo a base real em interop/common -- genérico, não uma
    lista hardcoded de 3 nomes (se um dia mais uma classe de dis/ ganhar o
    mesmo padrão, isto já cobre sem editar nada)."""
    self_loop = sorted(c for c, b in inheritance.items() if b == c)
    if not self_loop:
        return
    common_inheritance = scan.build_inheritance([INTEROP_COMMON_INCLUDE])
    for c in self_loop:
        common_base = common_inheritance.get(c)
        if not common_base or common_base == c:
            continue  # não achou base de verdade em interop/common -- deixa o self-loop como está
        alias = f"{c}@interop_common"
        inheritance[alias] = common_base
        for k, v in common_inheritance.items():
            inheritance.setdefault(k, v)  # o resto da cadeia acima (AbstractNetIO -> ... -> Object)
        inheritance[c] = alias  # quebra o self-loop


def build_headers(include_roots):
    """Classe -> caminho relativo do header onde DECLARE_SUBCLASS(Classe, ...)
    aparece -- primeiro achado vence (mesma disciplina/ordem de
    build_inheritance()). Não existe um helper pronto pra isto em
    mixr_source_scan.py (build_inheritance() descarta o caminho do arquivo),
    daí este helper local pequeno."""
    headers = {}
    for f in scan.iter_files(include_roots, {".hpp", ".h"}):
        masked = scan.mask_source(f.read_text(encoding="utf-8", errors="replace"))
        for m in scan.DECLARE_RE.finditer(masked):
            cls = m.group(1)
            headers.setdefault(cls, str(f.relative_to(REPO_ROOT)))
    return headers


def build_factories():
    """{modulo: {"file": "<factory.cpp relativo>", "classes": [...]}} -- um
    por módulo nativo, mais 'plugin:A-4'."""
    factories = {}
    for label, sub in NATIVE_MODULES:
        fcpp = MIXR_SRC / sub / "factory.cpp"
        factory_map = scan.build_factory_map([MIXR_SRC / sub])
        reachable = scan.find_dispatch_reachable_classes([fcpp])
        classes = sorted({cls for cls in factory_map.values() if cls in reachable})
        factories[label] = {"file": str(fcpp.relative_to(REPO_ROOT)), "classes": classes}

    factory_map = scan.build_factory_map([MODELS_DIR])
    reachable = scan.find_dispatch_reachable_classes([PLUGIN_FACTORY_CPP])
    classes = sorted({cls for cls in factory_map.values() if cls in reachable})
    factories[PLUGIN_LABEL] = {
        "file": str(PLUGIN_FACTORY_CPP.relative_to(REPO_ROOT)),
        "classes": classes,
    }
    return factories


def _compute_phases(chain_list, overrides):
    """wp = índices de fase (0..3) onde ALGUMA classe da cadeia (folha ->
    raiz) tem override não-vazio -- o primeiro achado, andando da folha pra
    raiz, é quem de fato executa (semântica real de despacho virtual C++).
    po = {str(indice): classe_dona}."""
    wp, po = [], {}
    for idx, method in enumerate(PHASE_METHODS):
        owner = None
        for level in chain_list:
            entry = overrides.get(level, {}).get(method)
            if entry and entry["nonempty"]:
                owner = level
                break
        if owner:
            wp.append(idx)
            po[str(idx)] = owner
    return wp, po


def _build_impl_files(src_roots):
    """Classe -> arquivo .cpp onde IMPLEMENT_*SUBCLASS(Classe, ...) aparece --
    UM passe só sobre todos os .cpp (ao contrário de
    extract_execution_chain.find_impl_file(), que refaz a varredura inteira
    por classe -- caro demais chamado 234 vezes; mesmo padrão de
    src/ui/scripts/generate_edl_catalog.py::build_class_files())."""
    out = {}
    for f in scan.iter_files(src_roots, {".cpp"}):
        masked = scan.mask_source(f.read_text(encoding="utf-8", errors="replace"))
        for m in scan.IMPLEMENT_RE.finditer(masked):
            out.setdefault(m.group(1), str(f.relative_to(REPO_ROOT)))
    return out


def build_model(factories):
    """MODEL[cls] para toda classe que aparece em algum FACTORIES[*].classes."""
    include_roots = [MIXR_INCLUDE / sub for _, sub in NATIVE_MODULES] + [MODELS_DIR]
    src_roots = [MIXR_SRC / sub for _, sub in NATIVE_MODULES] + [MODELS_DIR]

    inheritance = scan.build_inheritance(include_roots)
    _patch_self_loops(inheritance)

    slot_names = scan.extract_slots(src_roots)  # classe -> [{name, comment}]
    factory_map = scan.build_factory_map(src_roots)  # nome-de-fabrica -> classe
    class_to_factory = {}
    for factory_name, cls in factory_map.items():
        class_to_factory.setdefault(cls, factory_name)

    headers = build_headers(include_roots)
    impl_files = _build_impl_files(src_roots)
    overrides = chain_mod.find_overrides(src_roots, methods=chain_mod.TARGET_METHODS, capture_body=True)

    model = {}
    for module_label, entry in factories.items():
        for cls in entry["classes"]:
            if cls in model:
                # Caso raro, confirmado rodando -- so' um par: base::FileReader
                # (fabrica "FileReader") e recorder::FileReader (fabrica
                # "RecorderFileReader") sao DUAS classes C++ diferentes com o
                # MESMO nome barra. Primeiro modulo processado vence (a ordem
                # de NATIVE_MODULES -- "base" antes de "recorder"), entao a
                # entrada mostrada e' sempre a de base::FileReader; a fabrica
                # "RecorderFileReader" continua listada em
                # FACTORIES["recorder"].classes (o total por modulo nao muda),
                # so' nao ganha uma entrada PROPRIA em MODEL. Mesma ambiguidade
                # ja documentada em src/ui/scripts/generate_edl_catalog.py.
                continue
            raw_chain = scan.resolve_chain(cls, inheritance)
            chain_list = [_clean(lvl) for lvl in raw_chain]
            wp, po = _compute_phases(raw_chain, overrides)
            po = {k: _clean(v) for k, v in po.items()}
            own_overrides = overrides.get(cls, {})
            ov = sorted(m for m, v in own_overrides.items() if v["nonempty"])
            ml = {m: own_overrides[m]["line"] for m in ov}
            sl = [s["name"] for s in slot_names.get(cls, [])]
            factory_name = class_to_factory.get(cls)
            base = _clean(inheritance.get(cls))
            model[cls] = {
                "b": base,
                "f": factory_name if factory_name and factory_name != cls else None,
                "m": module_label,
                "r": True,
                "ch": chain_list,
                "sl": sl,
                "own": len(sl),
                "wp": wp,
                "po": po,
                "d": "System" in raw_chain,
                "ov": ov,
                "hd": headers.get(cls),
                "src": impl_files.get(cls),
                "ml": ml,
            }
    return model, overrides


def build_snippets(model, overrides):
    """SNIPPETS["Classe::metodo"] para TODA classe do universo (não só
    mixr::models) x todo TARGET_METHODS com override não-vazio -- cobertura
    total de detalhe entre classes, decisão desta sessão."""
    snippets = {}
    for cls in model:
        for method, entry in overrides.get(cls, {}).items():
            if not entry.get("nonempty"):
                continue
            snippets[f"{cls}::{method}"] = {
                "file": entry["file"],
                "line": entry["startLine"],
                "lines": entry["lines"],
                "trunc": entry["truncated"],
            }
    return snippets


def build_stats(model):
    classes = len(model)
    registered = sum(1 for e in model.values() if e["r"] is True)
    divergent = sum(1 for e in model.values() if e["f"] is not None)
    with_slots = sum(1 for e in model.values() if len(e["sl"]) > 0)
    slots_total = sum(len(e["sl"]) for e in model.values())
    phase_work = sum(1 for e in model.values() if len(e["wp"]) > 0)
    phase_owners = set()
    for e in model.values():
        phase_owners.update(e["po"].values())
    dispatch = sum(1 for e in model.values() if e["d"] is True)
    cpp = sum(1 for e in model.values() if e["src"] is not None)
    return {
        "classes": classes,
        "registered": registered,
        "divergent": divergent,
        "withSlots": with_slots,
        "slotsTotal": slots_total,
        "phaseWork": phase_work,
        "phaseOwn": len(phase_owners),
        "dispatch": dispatch,
        "cpp": cpp,
    }


def build_all():
    """Monta (MODEL, FACTORIES, SNIPPETS, STATS) -- a única função que
    tests/tools/test_manual_catalog.py precisa importar e chamar."""
    factories = build_factories()
    model, overrides = build_model(factories)
    snippets = build_snippets(model, overrides)
    stats = build_stats(model)
    return model, factories, snippets, stats


def main():
    model, factories, snippets, stats = build_all()
    print("/* GERADO por tools/generate_manual_catalog.py a partir da arvore de fontes do MIXR. Nao editar. */")
    print(f"const MODEL = {json.dumps(model, ensure_ascii=False)};")
    print(f"const FACTORIES = {json.dumps(factories, ensure_ascii=False)};")
    print(f"const SNIPPETS = {json.dumps(snippets, ensure_ascii=False)};")
    print(f"const STATS = {json.dumps(stats, ensure_ascii=False)};")


if __name__ == "__main__":
    main()
