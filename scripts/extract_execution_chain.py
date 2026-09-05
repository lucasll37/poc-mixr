#!/usr/bin/env python3
"""Extrai, do fonte REAL do MIXR e do plugin flight, a cadeia de heranca e quais
classes de fato fazem trabalho de fase.

Nao existe check-edl.py neste repositorio para reaproveitar (docs/TODO.md presumia
que existia) -- este e um extrator novo, pequeno, sem dependencias, que serve de
oraculo para docs/explorador/index.html: qualquer nome de classe/fabrica e qualquer
afirmacao de "isto e ocioso" usada na arvore da UI tem de bater com a saida daqui.

O que ele faz, dois passos:

  1) varre os headers (.hpp) de contexts/src/mixr/include/mixr e models/A4/include
     por DECLARE_SUBCLASS(Tipo, Base) -- monta a cadeia de heranca (Tipo -> Base).
     varre os .cpp de contexts/src/mixr/src e models/A4/src por
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

Uso:
    python3 scripts/extract_execution_chain.py                 # tabela, texto
    python3 scripts/extract_execution_chain.py --json           # tabela, JSON
    python3 scripts/extract_execution_chain.py Aircraft Gimbal  # classes/fabricas especificas
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MIXR_INCLUDE = REPO_ROOT / "contexts/src/mixr/include/mixr"
MIXR_SRC = REPO_ROOT / "contexts/src/mixr/src"
# Só a arvore de mixr::models (nao contexts/src/mixr/src inteiro, que tambem tem
# base/, simulation/, interop/, recorder/, terrain/, linkage/, linearsystem/ --
# essas NAO sao "modelos", sao infraestrutura do framework). --catalog usa isto,
# de proposito, para o universo virar exatamente "as classes que
# mixr::models::factory publica" -- o que o usuario pediu por "built-in no mixr".
MIXR_MODELS_INCLUDE = MIXR_INCLUDE / "models"
MIXR_MODELS_SRC = MIXR_SRC / "models"
# 'models/' -- deliberadamente a pasta INTEIRA, nao 'models/A4/include' como um
# primeiro corte deste script supunha. models/ esta em reorganizacao (confirmado
# rodando: AlertDatalink.hpp saiu de models/A4/include/xnative/ para
# models/player/A4/include/xnative/ ENQUANTO este script era escrito, por uma
# sessao concorrente) -- qualquer subcaminho fixo aqui fica errado no proximo commit.
# rglob() sobre a pasta toda e imune a esse tipo de mudanca de endereco.
MODELS_DIR = REPO_ROOT / "models"
SHARED_DIR = REPO_ROOT / "shared"
DEFAULT_SCENARIO = REPO_ROOT / "src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in"

INCLUDE_ROOTS = [MIXR_INCLUDE, MODELS_DIR, SHARED_DIR]
SRC_ROOTS = [MIXR_SRC, MODELS_DIR, SHARED_DIR]

TARGET_METHODS = [
    "updateTC", "updateData", "dynamics", "transmit", "receive", "process",
    "processComponents",
]

# Estas tres so fornecem o despacho GENERICO (percorrer filhos, dt4 = dt*4,
# switch(phase) chamando os quatro stubs vazios) -- nao contam como "trabalho de
# fase" por si so. Ver System.cpp:135-147 (stubs vazios, medido, nao suposto).
GENERIC_DISPATCH_CLASSES = {"Object", "Component", "System"}

# A base pode vir qualificada por namespace (ex.: DECLARE_SUBCLASS(WorldModel,
# simulation::Simulation), DECLARE_SUBCLASS(SimAgent, base::ubf::Agent)) -- sem
# aceitar ':' aqui a regra nao casava e a classe saia com cadeia vazia, parecendo
# sem fonte por engano.
DECLARE_RE = re.compile(r"\bDECLARE_SUBCLASS\s*\(\s*(\w+)\s*,\s*([\w:]+)\s*\)")
IMPLEMENT_RE = re.compile(
    r'\bIMPLEMENT_(?:PARTIAL_|ABSTRACT_)?SUBCLASS\s*\(\s*(\w+)\s*,\s*"([^"]+)"\s*\)'
)


def iter_files(roots, suffixes):
    for root in roots:
        if not root.exists():
            continue
        for path in sorted(root.rglob("*")):
            if path.suffix in suffixes:
                yield path


def build_inheritance(hpp_roots):
    """Tipo -> BaseType, a partir de DECLARE_SUBCLASS nos headers (texto mascarado,
    para nao casar uma mencao em comentario)."""
    chain = {}
    for f in iter_files(hpp_roots, {".hpp", ".h"}):
        masked = mask_source(f.read_text(encoding="utf-8", errors="replace"))
        for m in DECLARE_RE.finditer(masked):
            # guarda so o ultimo segmento (WorldModel -> Simulation, nao
            # -> 'simulation::Simulation') -- e por esse nome simples que o
            # resto do script (overrides, IMPLEMENT_SUBCLASS) indexa as classes.
            chain[m.group(1)] = m.group(2).rsplit("::", 1)[-1]
    return chain


def build_factory_map(cpp_roots):
    """NomeDeFabrica -> Tipo, a partir de IMPLEMENT_*SUBCLASS nos .cpp (texto
    mascarado, mesmo motivo)."""
    factory_to_class = {}
    for f in iter_files(cpp_roots, {".cpp"}):
        masked = mask_source(f.read_text(encoding="utf-8", errors="replace"))
        for m in IMPLEMENT_RE.finditer(masked):
            cls, name = m.group(1), m.group(2)
            factory_to_class.setdefault(name, cls)
    return factory_to_class


def mask_source(text):
    """Substitui o CONTEUDO de comentarios por espacos, preservando indices e
    quebras de linha (literais de string ficam INTACTOS -- IMPLEMENT_SUBCLASS
    precisa ler o nome de fabrica de dentro das aspas). Sem isso, uma linha de
    PROSA como '// Player::updateData() chama obj->updateData(dt) direto' (o
    proprio codigo deste repo cita a API do framework em comentario, ver
    Component.cpp) e confundida com uma definicao de verdade -- medido
    acontecendo antes deste mascaramento existir. O rastreio de string serve so
    para NAO interpretar um '//' ou '/*' que apareca dentro de uma string como
    inicio de comentario."""
    out = list(text)
    i, n = 0, len(text)
    in_string = None
    while i < n:
        c = text[i]
        if in_string:
            if c == "\\" and i + 1 < n:
                i += 2
                continue
            if c == in_string:
                in_string = None
            i += 1
            continue
        if c in ("\"", "'"):
            in_string = c
            i += 1
            continue
        if text[i : i + 2] == "//":
            j = i
            while j < n and text[j] != "\n":
                out[j] = " "
                j += 1
            i = j
            continue
        if text[i : i + 2] == "/*":
            end = text.find("*/", i + 2)
            end = end + 2 if end != -1 else n
            j = i
            while j < end:
                if text[j] != "\n":
                    out[j] = " "
                j += 1
            i = end
            continue
        i += 1
    return "".join(out)


def find_matching_brace(masked, open_idx):
    """Indice do '}' que fecha o '{' em open_idx, em texto ja mascarado (sem
    comentarios/strings) -- so precisa contar profundidade."""
    depth = 0
    i, n = open_idx, len(masked)
    while i < n:
        c = masked[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


MAX_BODY_LINES = 60


def find_overrides(cpp_roots, methods=None, capture_body=False):
    """Classe -> {metodo: {file, line, nonempty, [startLine, lines, truncated]}}
    a partir de 'Classe::metodo(' nos .cpp.

    Com capture_body=True, tambem recorta o texto REAL (assinatura + corpo, da
    linha da assinatura ate o '}' que fecha, capado em MAX_BODY_LINES linhas
    com 'truncated=True' quando cortar) -- e o que torna o modo --catalog capaz
    de gerar entradas de SRC prontas para ~90 classes sem transcricao manual.
    O corte NUNCA reescreve nada: e sempre um PREFIXO contiguo real, marcado
    como truncado quando e so um prefixo -- a mesma regra de ouro de sempre,
    so automatizada."""
    methods = methods or TARGET_METHODS
    overrides: dict[str, dict[str, dict]] = {}
    pattern = re.compile(r"\b(\w+)::(" + "|".join(methods) + r")\s*\(")
    for f in iter_files(cpp_roots, {".cpp"}):
        text = f.read_text(encoding="utf-8", errors="replace")
        masked = mask_source(text)
        for m in pattern.finditer(masked):
            cls, meth = m.group(1), m.group(2)
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


def resolve_chain(cls, inheritance):
    seen = []
    cur = cls
    guard = 0
    while cur and cur not in seen and guard < 50:
        seen.append(cur)
        cur = inheritance.get(cur)
        guard += 1
    return seen


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


def category_of(file_path):
    """Deriva a categoria de uma classe pelo CAMINHO do .cpp -- os diretorios de
    contexts/src/mixr/src/models/ ja espelham exatamente o agrupamento que
    contexts/src/mixr/src/models/factory.cpp usa nos comentarios (dynamics/,
    environment/, navigation/, player/{air,effect,ground,space,weapon}/,
    sensor/, system/{trackmanager/}), entao usar o caminho e mais robusto que
    reimplementar um parser dos comentarios do factory.cpp."""
    parts = Path(file_path).parts
    try:
        i = parts.index("models")
    except ValueError:
        return "outro"
    rest = parts[i + 1 :]
    if len(rest) >= 2 and rest[0] in ("player", "system") and not rest[1].endswith(".cpp"):
        return f"{rest[0]}/{rest[1]}"
    if len(rest) >= 1:
        return rest[0] if not rest[0].endswith(".cpp") else "misc"
    return "outro"


SLOT_RE = re.compile(
    r'BEGIN_SLOTTABLE\s*\(\s*(\w+)\s*\)(.*?)END_SLOTTABLE\s*\(\s*\1\s*\)', re.S
)
SLOT_ITEM_RE = re.compile(r'"([A-Za-z0-9_]+)"\s*,?\s*(?://\s*(.*))?')


def extract_slots(cpp_roots):
    """Classe -> [{name, comment}], a partir de BEGIN_SLOTTABLE(Classe) ...
    END_SLOTTABLE(Classe) (macros.hpp:291-298) -- e a lista de nomes que um
    .edl pode configurar nessa classe: a 'interface' publica dela, no sentido
    EDL. So le linhas comecando com uma string entre aspas (o formato real,
    confirmado lendo Antenna.cpp:29-37)."""
    slots: dict[str, list[dict]] = {}
    for f in iter_files(cpp_roots, {".cpp"}):
        text = f.read_text(encoding="utf-8", errors="replace")
        masked = mask_source(text)
        for m in SLOT_RE.finditer(masked):
            cls = m.group(1)
            body_masked = m.group(2)
            body_real = text[m.start(2) : m.end(2)]
            out = []
            for line_masked, line_real in zip(body_masked.split("\n"), body_real.split("\n")):
                sm = SLOT_ITEM_RE.match(line_masked.strip())
                if sm:
                    # pega o comentario da linha REAL (o mascarado apaga comentarios)
                    comment = ""
                    if "//" in line_real:
                        comment = line_real.split("//", 1)[1].strip()
                    out.append({"name": sm.group(1), "comment": comment})
            if out:
                slots[cls] = out
    return slots


def find_impl_file(cls, cpp_roots):
    """Arquivo .cpp onde IMPLEMENT_*SUBCLASS(cls, ...) aparece -- usado so para
    as classes sem NENHUM dos 7 metodos-alvo sobrescrito (ex.: classes de dado
    puro, sem trabalho de fase), que 'overrides' nao teria como localizar."""
    pattern = re.compile(r"\bIMPLEMENT_(?:PARTIAL_|ABSTRACT_)?SUBCLASS\s*\(\s*" + re.escape(cls) + r"\s*,")
    for f in iter_files(cpp_roots, {".cpp"}):
        if pattern.search(mask_source(f.read_text(encoding="utf-8", errors="replace"))):
            return str(f.relative_to(REPO_ROOT))
    return None


def build_catalog():
    """Monta o catalogo de TODAS as classes nativas registradas em
    mixr::models::factory (so contexts/src/mixr/src/models/ -- de proposito
    NAO inclui models/ deste repositorio, que sao os plugins PROPRIOS, nao
    'built-in no mixr'). Cada entrada ja carrega corpo de metodo REAL
    (capturado, nao transcrito a mao) e a lista de slots."""
    # inheritance/overrides: SEM restringir a models/ -- a cadeia de qualquer
    # classe de modelo sobe ate Component/Object, que moram em base/ (fora de
    # models/); restringir aqui quebraria resolve_chain() e escondaria o nivel
    # onde o despacho generico mora.
    inheritance = build_inheritance([MIXR_INCLUDE])
    overrides = find_overrides([MIXR_SRC], capture_body=True)
    # factory_map/slots: RESTRITO a models/ -- e o que define o UNIVERSO do
    # catalogo ("classes que mixr::models::factory publica", nao qualquer
    # classe do framework inteiro registrada via IMPLEMENT_SUBCLASS).
    factory_map = build_factory_map([MIXR_MODELS_SRC])
    slots = extract_slots([MIXR_MODELS_SRC])

    # Toda classe que aparece como VALOR de factory_map (i.e., foi registrada
    # via IMPLEMENT_*SUBCLASS) e uma classe 'publicada' pela fabrica -- o
    # universo que o usuario pediu ('todos os modelos built-in no mixr').
    classes_by_factory: dict[str, str] = {}
    for factory_name, cls in factory_map.items():
        classes_by_factory.setdefault(cls, factory_name)

    entries = []
    for cls, factory_name in sorted(classes_by_factory.items()):
        chain = resolve_chain(cls, inheritance)
        # arquivo onde a classe foi de fato IMPLEMENTADA (onde os overrides moram)
        cls_overrides = overrides.get(cls, {})
        any_file = next(iter(cls_overrides.values()))["file"] if cls_overrides else find_impl_file(cls, [MIXR_MODELS_SRC])
        category = category_of(any_file) if any_file else "outro"

        levels = []
        any_work = False
        for level in chain:
            lv_overrides = overrides.get(level, {})
            levels.append({
                "class": level,
                "generic_dispatch": level in GENERIC_DISPATCH_CLASSES,
                "methods": {
                    m: {"nonempty": v["nonempty"], "file": v["file"], "line": v["line"]}
                    for m, v in lv_overrides.items()
                },
            })
            if level not in GENERIC_DISPATCH_CLASSES and any(v["nonempty"] for v in lv_overrides.values()):
                any_work = True

        entries.append({
            "class": cls,
            "factory": factory_name,
            "file": any_file,
            "category": category,
            "chain": chain,
            "idle": not any_work,
            "levels": levels,
            "overrides": {m: v for m, v in cls_overrides.items() if v["nonempty"]},
            "slots": slots.get(cls, []),
        })
    return entries


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "classes",
        nargs="*",
        help="nomes de fabrica ou de classe C++; default: tokens de "
        "src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in",
    )
    ap.add_argument("--json", action="store_true", help="saida em JSON em vez de tabela")
    ap.add_argument(
        "--catalog", action="store_true",
        help="cataloga TODAS as classes nativas de mixr::models::factory (contexts/src/mixr/src/models/ "
        "apenas -- nao inclui os plugins deste repositorio), com corpo de metodo e slots capturados; "
        "ignora os argumentos posicionais; sempre imprime JSON",
    )
    args = ap.parse_args()

    if args.catalog:
        print(json.dumps(build_catalog(), indent=2, ensure_ascii=False))
        return

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
