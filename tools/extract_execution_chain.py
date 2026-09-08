#!/usr/bin/env python3
"""Extrai, do fonte REAL do MIXR e do plugin flight, a cadeia de heranca e quais
classes de fato fazem trabalho de fase.

Nao existe check-edl.py neste repositorio para reaproveitar (docs/TODO.md presumia
que existia) -- este e um extrator novo, pequeno, sem dependencias, que serve de
oraculo para docs/manual/index.html: qualquer nome de classe/fabrica e qualquer
afirmacao de "isto e ocioso" usada na arvore da UI tem de bater com a saida daqui.

O que ele faz, dois passos:

  1) varre os headers (.hpp) de contexts/src/mixr/include/mixr e models/players/A-4/include
     por DECLARE_SUBCLASS(Tipo, Base) -- monta a cadeia de heranca (Tipo -> Base).
     varre os .cpp de contexts/src/mixr/src e models/players/A-4/src por
     IMPLEMENT_SUBCLASS/IMPLEMENT_PARTIAL_SUBCLASS/IMPLEMENT_ABSTRACT_SUBCLASS
     (Tipo, "NomeDeFabrica") -- monta fabrica -> classe C++.

  2) varre os mesmos .cpp por definicoes 'Classe::metodo(' dos sete metodos do ciclo
     de vida do frame (updateTC, updateData, dynamics, transmit, receive, process,
     processComponents), casa as chaves do corpo por profundidade de '{'/'}' (ciente
     de comentarios e literais de string) e verifica se o corpo, sem comentarios, e
     vazio -- exatamente como e o caso medido de System::dynamics/transmit/receive/
     process (System.cpp:135-147), que sao stubs vazios sobrescritos pelas folhas.

Um noh e classificado OCIOSO quando, em toda a cadeia de heranca ACIMA de Object/
Component/System (essas tres so fornecem o despacho generico -- percorrer filhos e o
switch(phase) que chama os stubs vazios acima), nenhum nivel sobrescreve nenhum dos
sete metodos com um corpo nao-vazio. Isto e uma aproximacao estatica (nao le se o
corpo faz algo INTERESSANTE, so se faz ALGUMA coisa) -- suficiente para a "regra de
ouro" do TODO: nao inventar, e dar uma resposta que bate com o fonte.

O catalogo completo para o EDITOR GRAFICO de .edl (antigo modo --edl-catalog deste
arquivo) mora em src/ui/scripts/generate_edl_catalog.py. O catalogo da aba
"Catalogo" de docs/manual/ (antigo modo --catalog deste arquivo, removido --
universo restrito a mixr::models, sem o plugin de producao nem os demais
modulos nativos) mora agora em tools/generate_manual_catalog.py, que importa
find_overrides()/TARGET_METHODS/find_impl_file() daqui como MODULO, sem
reimplementar nada. As primitivas de varredura de C++ que os tres geradores
compartilham (mask_source, build_inheritance, build_factory_map, extract_slots,
find_dispatch_reachable_classes, ...) moram em tools/mixr_source_scan.py,
importado por TODOS -- nao duplicadas.

Uso:
    python3 tools/extract_execution_chain.py                 # tabela, texto
    python3 tools/extract_execution_chain.py --json           # tabela, JSON
    python3 tools/extract_execution_chain.py Aircraft Gimbal  # classes/fabricas especificas
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mixr_source_scan as scan  # noqa: E402

MIXR_INCLUDE = REPO_ROOT / scan.MIXR_INCLUDE_REL
MIXR_SRC = REPO_ROOT / scan.MIXR_SRC_REL
# 'models/' -- deliberadamente a pasta INTEIRA, nao 'models/players/A-4/include' como um
# primeiro corte deste script supunha. models/ esteve em reorganizacao (confirmado
# rodando: AlertDatalink.hpp saiu de models/A4/include/xnative/ para
# models/players/A-4/include/xnative/ ENQUANTO este script era escrito, por uma
# sessao concorrente) -- qualquer subcaminho fixo aqui ficaria errado no proximo commit.
# rglob() sobre a pasta toda e imune a esse tipo de mudanca de endereco.
MODELS_DIR = REPO_ROOT / scan.MODELS_REL
LIBS_DIR = REPO_ROOT / scan.LIBS_REL
DEFAULT_SCENARIO = REPO_ROOT / "src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in"

INCLUDE_ROOTS = [MIXR_INCLUDE, MODELS_DIR, LIBS_DIR]
SRC_ROOTS = [MIXR_SRC, MODELS_DIR, LIBS_DIR]

TARGET_METHODS = [
    "updateTC", "updateData", "dynamics", "transmit", "receive", "process",
    "processComponents",
    # UBF (mixr::base::ubf): metodos do ciclo de decisao, a parte do switch(phase)
    # acima -- ver docs/manual/doc.jsx (trilha "Decisao (UBF)").
    "genAction", "updateState", "execute", "genComplexAction", "controller",
]

# Estas tres so fornecem o despacho GENERICO (percorrer filhos, dt4 = dt*4,
# switch(phase) chamando os quatro stubs vazios) -- nao contam como "trabalho de
# fase" por si so. Ver System.cpp:135-147 (stubs vazios, medido, nao suposto).
GENERIC_DISPATCH_CLASSES = {"Object", "Component", "System"}

mask_source = scan.mask_source
iter_files = scan.iter_files
build_inheritance = scan.build_inheritance
build_factory_map = scan.build_factory_map
resolve_chain = scan.resolve_chain
extract_slots = scan.extract_slots
find_matching_brace = scan.find_matching_brace

MAX_BODY_LINES = 60


def find_overrides(cpp_roots, methods=None, capture_body=False):
    """Classe -> {metodo: {file, line, nonempty, [startLine, lines, truncated]}}
    a partir de 'Classe::metodo(' nos .cpp.

    Com capture_body=True, tambem recorta o texto REAL (assinatura + corpo, da
    linha da assinatura ate o '}' que fecha, capado em MAX_BODY_LINES linhas
    com 'truncated=True' quando cortar) -- e o que torna tools/generate_manual_catalog.py
    capaz de gerar entradas de SNIPPETS prontas para todo o universo dele sem
    transcricao manual. O corte NUNCA reescreve nada: e sempre um PREFIXO
    contiguo real, marcado como truncado quando e so um prefixo -- a mesma
    regra de ouro de sempre, so automatizada.

    PRIMEIRO achado vence por (classe, metodo) -- nao sobrescreve -- pelo
    MESMO motivo de extract_slots() (em mixr_source_scan.py): varrer
    models/ inteiro (incluindo models/players/template/src/mirror.cpp, que
    deliberadamente reimplementa FlightAction::execute/etc. com o MESMO
    nome, so para testar contrato de carga de plugin) faz duas definicoes
    REAIS do mesmo (Classe, metodo) aparecerem em arquivos diferentes.
    Confirmado rodando: sem a guarda, 'FlightAction::execute' saia
    apontando pra mirror.cpp (varrido depois de A-4 na ordem alfabetica),
    nao para a implementacao de producao. A guarda e por (classe, metodo),
    NAO por classe inteira -- uma classe real legitimamente tem metodos
    DIFERENTES definidos em arquivos DIFERENTES (isso nao e colisao
    nenhuma, e so precisa dos DOIS registrados)."""
    methods = methods or TARGET_METHODS
    overrides: dict[str, dict[str, dict]] = {}
    pattern = re.compile(r"\b(\w+)::(" + "|".join(methods) + r")\s*\(")
    for f in iter_files(cpp_roots, {".cpp"}):
        text = f.read_text(encoding="utf-8", errors="replace")
        masked = mask_source(text)
        for m in pattern.finditer(masked):
            cls, meth = m.group(1), m.group(2)
            if meth in overrides.get(cls, {}):
                continue
            paren_end = masked.find(")", m.end())
            if paren_end == -1:
                continue
            brace_start = masked.find("{", paren_end)
            semi = masked.find(";", paren_end)
            # ';' antes de '{' == declaracao/forward decl, nao definicao -- pula.
            if brace_start == -1 or (semi != -1 and semi < brace_start):
                continue
            brace_end = find_matching_brace(masked, brace_start)
            if brace_end == -1:
                continue
            nonempty = bool(masked[brace_start + 1 : brace_end].strip())
            line_no = text.count("\n", 0, m.start()) + 1
            entry = {
                "file": str(f.relative_to(REPO_ROOT)),
                "line": line_no,
                "nonempty": nonempty,
            }
            if capture_body:
                sig_start = text.rfind("\n", 0, m.start()) + 1
                start_line_no = text.count("\n", 0, sig_start) + 1
                raw = text[sig_start : brace_end + 1].split("\n")
                entry["startLine"] = start_line_no
                entry["lines"] = raw[:MAX_BODY_LINES]
                entry["truncated"] = len(raw) > MAX_BODY_LINES
            overrides.setdefault(cls, {})[meth] = entry
    return overrides


def default_target_tokens():
    text = DEFAULT_SCENARIO.read_text(encoding="utf-8", errors="replace")
    return sorted(set(re.findall(r"\(\s*([A-Z]\w+)\b", text)))


def analyze(tokens, inheritance, factory_map, overrides):
    rows = []
    for token in tokens:
        cls = factory_map.get(token, token)
        chain = resolve_chain(cls, inheritance)
        if not chain:
            rows.append(
                {
                    "factory": token,
                    "class": cls,
                    "chain": [],
                    "levels": [],
                    "idle": None,
                    "error": "classe nao encontrada (sem DECLARE_SUBCLASS no fonte)",
                }
            )
            continue
        levels = []
        any_work = False
        for level in chain:
            ov = overrides.get(level, {})
            levels.append(
                {
                    "class": level,
                    "generic_dispatch": level in GENERIC_DISPATCH_CLASSES,
                    "methods": {
                        m: {"nonempty": v["nonempty"], "file": v["file"], "line": v["line"]}
                        for m, v in ov.items()
                    },
                }
            )
            if level in GENERIC_DISPATCH_CLASSES:
                continue
            if any(v["nonempty"] for v in ov.values()):
                any_work = True
        rows.append(
            {
                "factory": token,
                "class": cls,
                "chain": chain,
                "levels": levels,
                "idle": not any_work,
                "error": None,
            }
        )
    return rows


def print_table(rows):
    for r in rows:
        if r["error"]:
            print(f"{r['factory']:22s} -> {r['class']:22s} ERRO: {r['error']}")
            continue
        marker = "OCIOSO" if r["idle"] else "ativo "
        print(f"{r['factory']:22s} -> {r['class']:22s} [{marker}]  cadeia: {' -> '.join(r['chain'])}")
        for lvl in r["levels"]:
            if not lvl["methods"]:
                continue
            tag = " (despacho generico)" if lvl["generic_dispatch"] else ""
            hits = [
                f"{m}{'*' if info['nonempty'] else '(vazio)'}@{info['file']}:{info['line']}"
                for m, info in lvl["methods"].items()
            ]
            print(f"      {lvl['class']:22s}{tag}: {', '.join(hits)}")


def find_impl_file(cls, cpp_roots):
    """Arquivo .cpp onde IMPLEMENT_*SUBCLASS(cls, ...) aparece -- usado so para
    as classes sem NENHUM dos 7 metodos-alvo sobrescrito (ex.: classes de dado
    puro, sem trabalho de fase), que 'overrides' nao teria como localizar."""
    pattern = re.compile(r"\bIMPLEMENT_(?:PARTIAL_|ABSTRACT_)?SUBCLASS\s*\(\s*" + re.escape(cls) + r"\s*,")
    for f in iter_files(cpp_roots, {".cpp"}):
        if pattern.search(mask_source(f.read_text(encoding="utf-8", errors="replace"))):
            return str(f.relative_to(REPO_ROOT))
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "classes",
        nargs="*",
        help="nomes de fabrica ou de classe C++; default: tokens de "
        "src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in",
    )
    ap.add_argument("--json", action="store_true", help="saida em JSON em vez de tabela")
    args = ap.parse_args()

    inheritance = build_inheritance(INCLUDE_ROOTS)
    factory_map = build_factory_map(SRC_ROOTS)
    overrides = find_overrides(SRC_ROOTS)

    tokens = args.classes or default_target_tokens()
    rows = analyze(tokens, inheritance, factory_map, overrides)

    if args.json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
    else:
        print_table(rows)


if __name__ == "__main__":
    main()
