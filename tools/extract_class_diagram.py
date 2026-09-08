#!/usr/bin/env python3
"""Extrai, dos headers REAIS do MIXR vendorizado (`contexts/src/mixr/include/`),
um diagrama de classe UML curado: 18 classes "Tier 1" com corpo completo
(atributos/componentes/metodos planificados) e um conjunto de classes "Tier 2"
(stub minimo -- so nome + base imediata), para alimentar a aba experimental
"Diagrama de Classe Estrutural" de `docs/manual/doc.jsx`.

Nao existia, em lugar nenhum deste repositorio, um parser de CORPO de classe
C++ (atributos e assinaturas de metodo) -- `tools/mixr_source_scan.py` e os
consumidores dele so leem 3 fatos guiados por macro (heranca via
DECLARE_SUBCLASS, fabrica via IMPLEMENT_*SUBCLASS, slots via BEGIN_SLOTTABLE).
Este e' o parser novo: um tokenizador de corpo de classe escrito a mao,
validado lendo os 18 headers-alvo por completo (nao por amostragem) antes de
qualquer regex ser escrita.

Uso:
    python3 tools/extract_class_diagram.py > /tmp/class_diagram.json

Sai com codigo != 0 (e imprime o motivo em stderr) se a auto-verificacao
embutida falhar -- ver `self_check()` mais abaixo.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))
import mixr_source_scan as scan  # noqa: E402

MIXR_INCLUDE = REPO_ROOT / scan.MIXR_INCLUDE_REL
MIXR_SRC = REPO_ROOT / scan.MIXR_SRC_REL

mask_source = scan.mask_source
find_matching_brace = scan.find_matching_brace
classify_slot_type = scan.classify_slot_type
build_descendants = scan.build_descendants
extract_primary_components = scan.extract_primary_components
DECLARE_RE = scan.DECLARE_RE

# ---------------------------------------------------------------------------
# Escopo dos dados (ver o plano -- secao "Escopo dos dados: 18 classes 'Tier 1'
# + ~23 'Tier 2'"): as 18 classes fundacionais com extracao completa, uma por
# arquivo EXATO (nao ha ambiguidade de qual arquivo ler -- diferente do
# catalogo do editor de EDL, que varre pastas inteiras).
# ---------------------------------------------------------------------------
TIER1_HEADERS = {
    "Referenced":           ("mixr::base",           "contexts/src/mixr/include/mixr/base/Referenced.hpp"),
    "Object":                ("mixr::base",           "contexts/src/mixr/include/mixr/base/Object.hpp"),
    "Component":             ("mixr::base",           "contexts/src/mixr/include/mixr/base/Component.hpp"),
    "Pair":                  ("mixr::base",           "contexts/src/mixr/include/mixr/base/Pair.hpp"),
    "PairStream":            ("mixr::base",           "contexts/src/mixr/include/mixr/base/PairStream.hpp"),
    "System":                ("mixr::models",         "contexts/src/mixr/include/mixr/models/system/System.hpp"),
    "AbstractPlayer":        ("mixr::simulation",     "contexts/src/mixr/include/mixr/simulation/AbstractPlayer.hpp"),
    "Player":                ("mixr::models",         "contexts/src/mixr/include/mixr/models/player/Player.hpp"),
    "Station":               ("mixr::simulation",     "contexts/src/mixr/include/mixr/simulation/Station.hpp"),
    "Simulation":            ("mixr::simulation",     "contexts/src/mixr/include/mixr/simulation/Simulation.hpp"),
    "WorldModel":            ("mixr::models",         "contexts/src/mixr/include/mixr/models/WorldModel.hpp"),
    "Agent":                 ("mixr::base::ubf",      "contexts/src/mixr/include/mixr/base/ubf/Agent.hpp"),
    "AbstractBehavior":      ("mixr::base::ubf",      "contexts/src/mixr/include/mixr/base/ubf/AbstractBehavior.hpp"),
    "AbstractState":         ("mixr::base::ubf",      "contexts/src/mixr/include/mixr/base/ubf/AbstractState.hpp"),
    "NetIO":                 ("mixr::interop::common", "contexts/src/mixr/include/mixr/interop/common/NetIO.hpp"),
    "Ntm":                   ("mixr::interop::common", "contexts/src/mixr/include/mixr/interop/common/Ntm.hpp"),
    "AbstractDataRecorder":  ("mixr::simulation",     "contexts/src/mixr/include/mixr/simulation/AbstractDataRecorder.hpp"),
    "DataRecorder":          ("mixr::recorder",       "contexts/src/mixr/include/mixr/recorder/DataRecorder.hpp"),
    "OutputHandler":         ("mixr::recorder",       "contexts/src/mixr/include/mixr/recorder/OutputHandler.hpp"),
}

# Os 10 papeis de Player (ver PRIMARY_COMPONENT_RE em mixr_source_scan.py) +
# os demais alvos citados no plano. Semente FIXA -- qualquer alvo "outro"
# real, nao antecipado aqui (ex.: SynchronizedState), e' descoberto Ã  parte
# (ver discover_outro()) e entra em `tier2` do mesmo jeito, marcado
# source="discovered" em vez de "seed"/"role".
TIER2_ROLE_NAMES = {
    "DynamicsModel", "Pilot", "Navigation", "Datalink", "Radio", "Gimbal",
    "RfSensor", "IrSystem", "OnboardComputer", "StoresMgr",
}
TIER2_SEED_NAMES = TIER2_ROLE_NAMES | {
    "AbstractNib", "Terrain", "AbstractAtmosphere", "RfSignature",
    "IrSignature", "Statistic", "List",
}


def curated_names_for_test():
    """Exposto so' para o teste de regressao
    (tests/tools/test_extract_class_diagram.py): o conjunto Tier-1 uniao
    TIER2_SEED_NAMES -- a MESMA curadoria usada internamente
    (`extract_tier1_class`) para decidir se um alvo de composicao e'
    'antecipado' (curado de antemao) ou 'descoberto' (achado so' rodando a
    varredura ampla de DECLARE_SUBCLASS)."""
    return set(TIER1_HEADERS) | TIER2_SEED_NAMES

# interop/hla e interop/rprfom tem classes com o MESMO NOME BARRA de classes
# DIFERENTES e de fato usadas (Aircraft, NetIO, Nib, Ntm, ...) -- mesma
# exclusao ja usada por src/ui/scripts/generate_edl_catalog.py
# (EDL_CATALOG_MIXR_MODULES) e pelo proprio plano deste extrator (Tarefa 2,
# passo 3). So entra na varredura "ampla" usada para achar alvos "outros" --
# a base de CADA um dos 18 Tier-1 e' resolvida POR ARQUIVO (ver
# resolve_base_in_file), nunca por este dicionario.
EXCLUDED_INTEROP_DIRS = {"hla", "rprfom"}


def iter_include_headers_excluding(exclude_interop_dirs):
    for path in sorted(MIXR_INCLUDE.rglob("*")):
        if path.suffix not in (".hpp", ".h"):
            continue
        parts = path.relative_to(MIXR_INCLUDE).parts
        if len(parts) >= 2 and parts[0] == "interop" and parts[1] in exclude_interop_dirs:
            continue
        yield path


def build_broad_inheritance():
    """Tipo -> BaseType (nome barra), sobre TODO `contexts/src/mixr/include/`
    (exceto hla/rprfom) -- usado so' para a pergunta booleana "este nome bare
    e' mesmo uma classe MIXR de verdade em algum lugar" (descoberta de alvo
    'outro' nao antecipado) e para o `base` do stub Tier-2 resultante. NUNCA
    usado para resolver a base de um dos 18 Tier-1 (isso e' por-arquivo, ver
    resolve_base_in_file) -- e' exatamente o mapa que colidiria em
    NetIO/Ntm/Nib (`interop::dis::Nib` redeclara `Nib` derivando de
    `interop::Nib`, que apos descartar namespace vira o MESMO nome barra:
    self-loop). 'primeiro achado vence' (setdefault, nao atribuicao) para que
    a entrada de `interop/common` (sem self-loop) normalmente vença sobre a
    de `interop/dis` (com); e qualquer self-loop que ainda sobreviva (a
    ordem de varredura por si so' nao garante isso para TODO nome, so' para
    os que testamos) e' filtrado no ponto de uso (ver resolve_outro_base)."""
    chain = {}
    for f in iter_include_headers_excluding(EXCLUDED_INTEROP_DIRS):
        masked = mask_source(f.read_text(encoding="utf-8", errors="replace"))
        for m in DECLARE_RE.finditer(masked):
            chain.setdefault(m.group(1), m.group(2).rsplit("::", 1)[-1])
    return chain


def resolve_outro_base(name, broad_inheritance):
    base = broad_inheritance.get(name)
    if base == name:
        return None  # self-loop residual -- ver o comentario de build_broad_inheritance()
    return base


# ---------------------------------------------------------------------------
# Isolar o corpo da classe (algoritmo, passo 1 do plano) -- por ARQUIVO.
# ---------------------------------------------------------------------------
def resolve_base_in_file(class_name, masked_text):
    """DECLARE_SUBCLASS(<class_name>, Base) no MESMO arquivo -- nunca um mapa
    global (ver o comentario de build_broad_inheritance())."""
    pat = re.compile(r"\bDECLARE_SUBCLASS\s*\(\s*" + re.escape(class_name) + r"\s*,\s*([\w:]+)\s*\)")
    m = pat.search(masked_text)
    if m:
        return m.group(1).rsplit("::", 1)[-1]
    return None


def base_from_inheritance_clause(clause):
    """Fallback para as duas unicas classes deste conjunto sem
    DECLARE_SUBCLASS: `class Object : public Referenced` (sem macro nenhuma)
    e `class Referenced` (sem base nenhuma). `clause` e' o texto capturado
    pelo grupo `(:\\s*[^{;]*)?` do casamento de `class NOME ... {` -- inclui
    o `:` inicial quando presente."""
    if not clause:
        return None
    clause = clause.lstrip(":").strip()
    if not clause:
        return None
    first = clause.split(",")[0]
    first = re.sub(r"\b(public|protected|private|virtual)\b", " ", first).strip()
    if not first:
        return None
    return first.rsplit("::", 1)[-1]


def find_class_body(class_name, masked_text):
    """(brace_start, brace_end) do corpo de `class <class_name> ... { ... }`
    neste arquivo (indices absolutos no buffer mascarado/original -- os dois
    tem o MESMO tamanho, mask_source() preserva indices). `\\b` nos dois lados
    do nome evita casar `AgentTC` ao procurar `Agent` (nao ha limite de
    palavra entre 't' de 'Agent' e 'T' de 'TC', entao `\\bAgent\\b` nao entra
    ali -- confirmado lendo base/ubf/Agent.hpp, que declara as duas classes
    no MESMO arquivo). Forward-declaration (`class Foo;`, sem chave) nunca
    casa, porque o `\\{` final e' obrigatorio."""
    pat = re.compile(r"\bclass\s+" + re.escape(class_name) + r"\b\s*(:\s*[^{;]*)?\{")
    m = pat.search(masked_text)
    if not m:
        return None
    brace_start = m.end() - 1
    brace_end = find_matching_brace(masked_text, brace_start)
    if brace_end == -1:
        return None
    return brace_start, brace_end, m.group(1)


# ---------------------------------------------------------------------------
# Dividir o corpo em declaracoes (algoritmo, passo 2) -- tokenizador escrito
# a mao sobre o texto MASCARADO (indices absolutos), rastreando profundidade
# de parenteses/chaves.
# ---------------------------------------------------------------------------
VISIBILITY_RE = re.compile(r"\b(public|protected|private)\s*:")
NESTED_CLASS_RE = re.compile(r"\b(class|struct)\s+(\w+)?\s*(:\s*[^{;]*)?\{")
NESTED_ENUM_RE = re.compile(r"\benum\b(?:\s+(?:class|struct))?\s*(\w+)?\s*(:\s*[\w:]+)?\s*\{")
TYPEDEF_USING_FRIEND_RE = re.compile(r"\b(typedef|using|friend)\b")
DECLARE_MACRO_START_RE = re.compile(r"\bDECLARE_SUBCLASS\s*\(")


def find_matching_paren(masked, open_idx):
    depth = 0
    i, n = open_idx, len(masked)
    while i < n:
        c = masked[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def skip_optional_semicolon(masked, pos):
    n = len(masked)
    k = pos
    while k < n and masked[k].isspace():
        k += 1
    if k < n and masked[k] == ";":
        return k + 1
    return pos


def scan_statement(masked, start):
    """Varre uma declaracao (atributo OU metodo) a partir de `start`,
    rastreando profundidade de parenteses. Termina no `;` de topo (sem corpo
    inline) ou no `{` de topo (corpo inline de metodo OU inicializador de
    atributo por chave -- os dois casos se resolvem igual: acha o `}` que
    fecha, consome um `;` opcional logo depois). Devolve
    (term_idx, end_idx, is_method, first_paren_idx) -- `is_method` e'
    "havia um '(' de topo antes do terminador" (aceita construtor/destrutor/
    operator= como metodo, ja que todos tem parenteses); `first_paren_idx` e'
    a posicao do primeiro '(' de topo (None se nao houver), usado so' para
    achar o NOME de um metodo (o identificador logo antes dele)."""
    n = len(masked)
    i = start
    depth = 0
    has_top_paren = False
    first_paren_idx = None
    while i < n:
        c = masked[i]
        if c == "(":
            if depth == 0:
                has_top_paren = True
                if first_paren_idx is None:
                    first_paren_idx = i
            depth += 1
            i += 1
            continue
        if c == ")":
            depth = max(0, depth - 1)
            i += 1
            continue
        if depth == 0 and c == ";":
            return i, i + 1, has_top_paren, first_paren_idx
        if depth == 0 and c == "{":
            end_brace = find_matching_brace(masked, i)
            if end_brace == -1:
                return i, n, has_top_paren, first_paren_idx
            end = skip_optional_semicolon(masked, end_brace + 1)
            return i, end, has_top_paren, first_paren_idx
        i += 1
    return n, n, has_top_paren, first_paren_idx


def split_declarations(masked, body_start, body_end):
    """[(kind, visibility, start, end, extra)] cobrindo TODO o corpo
    [body_start, body_end) -- kind em {"macro","preprocessor","nested",
    "typedef","attribute","method"}. Rotulo de visibilidade (public:/
    protected:/private:) so' atualiza um estado mutavel, nunca vira item
    proprio na saida (mas o texto dele e' considerado 'consumido' para o
    self-check via um item kind="visibility")."""
    items = []
    pos = body_start
    current_vis = "private"  # default de `class` (todos os 18 sao `class`, nunca `struct`)
    while pos < body_end:
        while pos < body_end and masked[pos].isspace():
            pos += 1
        if pos >= body_end:
            break

        m = VISIBILITY_RE.match(masked, pos)
        if m and m.end() <= body_end:
            current_vis = m.group(1)
            items.append(("visibility", current_vis, pos, m.end(), None))
            pos = m.end()
            continue

        m = DECLARE_MACRO_START_RE.match(masked, pos)
        if m:
            paren_end = find_matching_paren(masked, m.end() - 1)
            end = body_end if paren_end == -1 else skip_optional_semicolon(masked, paren_end + 1)
            items.append(("macro", current_vis, pos, end, None))
            pos = end
            continue

        if masked[pos] == "#":
            eol = masked.find("\n", pos, body_end)
            end = body_end if eol == -1 else eol
            items.append(("preprocessor", current_vis, pos, end, None))
            pos = end
            continue

        m = NESTED_CLASS_RE.match(masked, pos) or NESTED_ENUM_RE.match(masked, pos)
        if m:
            brace_idx = m.end() - 1
            end_brace = find_matching_brace(masked, brace_idx)
            end = body_end if end_brace == -1 else skip_optional_semicolon(masked, end_brace + 1)
            items.append(("nested", current_vis, pos, end, None))
            pos = end
            continue

        m = TYPEDEF_USING_FRIEND_RE.match(masked, pos)
        if m:
            term_idx, end, _, _ = scan_statement(masked, pos)
            items.append(("typedef", current_vis, pos, end, None))
            pos = end
            continue

        term_idx, end, is_method, first_paren_idx = scan_statement(masked, pos)
        kind = "method" if is_method else "attribute"
        items.append((kind, current_vis, pos, end, (term_idx, first_paren_idx)))
        pos = end
    return items


# ---------------------------------------------------------------------------
# Normalizacao de tipo (algoritmo, passo 3) e nome de metodo.
# ---------------------------------------------------------------------------
WRAPPER_RE = re.compile(r"^(?:base::)?(safe_ptr|safe_queue)<(.+)>$", re.S)
STD_ARRAY_RE = re.compile(r"^std::array<(.+)>$", re.S)
QUALIFIER_PREFIX_RE = re.compile(r"^\s*(?:const|mutable|static)\s+")


def split_top_level_comma(text):
    """Primeiro segmento de `text` ate a PRIMEIRA virgula de topo (fora de
    '<...>' aninhado) -- usado so' para o primeiro argumento de
    std::array<T, N>."""
    depth = 0
    for i, c in enumerate(text):
        if c in "<(":
            depth += 1
        elif c in ">)":
            depth = max(0, depth - 1)
        elif c == "," and depth == 0:
            return text[:i]
    return text


def strip_qualifiers_and_pointer(t):
    t = t.strip()
    while True:
        m = QUALIFIER_PREFIX_RE.match(t)
        if not m:
            break
        t = t[m.end():]
    stars = t.count("*")
    t = t.replace("*", "").replace("&", "").strip()
    # reaplica o strip de qualificador (ex.: 'const models::Player' dentro
    # de um safe_ptr<const models::Player> so' fica exposto DEPOIS de tirar
    # o '*'/'&' externo, que aqui nao existe -- mas o prefixo pode reaparecer
    # se o wrapper devolveu algo como 'const Foo*')
    while True:
        m = QUALIFIER_PREFIX_RE.match(t)
        if not m:
            break
        t = t[m.end():]
    return t.strip(), stars


def normalize_attribute_type(raw_type_text):
    """(bare_name, extra_stars, is_container_wrapper) -- desembrulha UM nivel
    de base::safe_ptr<T>/base::safe_queue<T>/std::array<T,N>, tira
    const/mutable/static e ponteiro/referencia, e devolve o ULTIMO segmento
    '::' do que sobrar. `is_container_wrapper` e' True para safe_queue<T> e
    std::array<T,N> (ambos representam MUITOS T, nao um so' -- ao contrario
    de safe_ptr<T>, que e' so' um ponteiro thread-safe para UM T)."""
    outer, outer_stars = strip_qualifiers_and_pointer(raw_type_text)
    m = WRAPPER_RE.match(outer)
    if m:
        wrapper, inner = m.group(1), m.group(2)
        inner_bare, inner_stars = strip_qualifiers_and_pointer(inner)
        bare = inner_bare.rsplit("::", 1)[-1] if inner_bare else inner_bare
        return bare, outer_stars + inner_stars, wrapper == "safe_queue"
    m = STD_ARRAY_RE.match(outer)
    if m:
        first_arg = split_top_level_comma(m.group(1))
        inner_bare, inner_stars = strip_qualifiers_and_pointer(first_arg)
        bare = inner_bare.rsplit("::", 1)[-1] if inner_bare else inner_bare
        return bare, outer_stars + inner_stars, True
    bare = outer.rsplit("::", 1)[-1] if outer else outer
    return bare, outer_stars, False


NAME_BEFORE_PAREN_RE = re.compile(
    r"(operator\s*(?:==|!=|<=|>=|<<|>>|\+\+|--|=)|~?\w+)\s*$"
)


def parse_method_name(text_before_paren):
    m = NAME_BEFORE_PAREN_RE.search(text_before_paren.strip())
    if m:
        return re.sub(r"\s+", "", m.group(1))
    return text_before_paren.strip().split()[-1] if text_before_paren.strip() else "?"


ASSIGN_SPLIT_RE = re.compile(r"(?<![=!<>])=(?!=)")


def parse_attribute_declarator(declarator_masked):
    d = declarator_masked.strip()
    is_static = bool(re.search(r"\bstatic\b", d))
    d = ASSIGN_SPLIT_RE.split(d, maxsplit=1)[0].strip()
    m = re.match(r"^(.*[\s*&])(\w+)\s*(?:\[[^\]]*\])?$", d)
    if m:
        type_text = re.sub(r"\b(static|mutable)\b", " ", m.group(1)).strip()
        type_text = re.sub(r"\s+", " ", type_text)
        name = m.group(2)
    else:
        type_text, name = "", (d.split()[-1] if d.split() else d)
    return type_text, name, is_static


# ---------------------------------------------------------------------------
# Papel especial: os 10 ponteiros base::Pair* de Player (ver o algoritmo,
# passo 4) -- generico o bastante para nao se aplicar a nenhuma outra classe
# deste conjunto (confirmado por grep: sao os UNICOS atributos "Pair*"
# encontrados nos 18 headers-alvo).
# ---------------------------------------------------------------------------
PLAYER_ROLE_FIELD_NAMES = {
    "dynamicsModel", "datalink", "gimbal", "nav", "obc",
    "pilot", "radio", "sensor", "irSystem", "sms",
}

# Caso especial unico e restrito (algoritmo, passo 5): PairStream nao tem
# atributo proprio (herda armazenamento de List) -- o alvo de composicao e'
# inferido pelo tipo mais citado entre os PROPRIOS metodos, limiar >= 3.
PAIRSTREAM_INFERENCE_THRESHOLD = 3


class ClassExtraction:
    def __init__(self, name, namespace, header_rel):
        self.name = name
        self.namespace = namespace
        self.header_rel = header_rel
        self.base = None
        self.attributes = []
        self.components = []
        self.methods = []
        self.body_len = 0
        self.leftover = ""


def extract_tier1_class(name, namespace, header_rel, curated_names, tier1_names,
                         broad_inheritance, descendants_by_broad, outros, warnings):
    path = REPO_ROOT / header_rel
    text = path.read_text(encoding="utf-8", errors="replace")
    masked = mask_source(text)

    found = find_class_body(name, masked)
    if not found:
        warnings.append(f"{name}: corpo da classe nao encontrado em {header_rel}")
        return None
    brace_start, brace_end, clause = found

    base = resolve_base_in_file(name, masked)
    if base is None:
        base = base_from_inheritance_clause(clause)

    result = ClassExtraction(name, namespace, header_rel)
    result.base = base
    body_start, body_end = brace_start + 1, brace_end
    result.body_len = body_end - body_start

    items = split_declarations(masked, body_start, body_end)

    # self-check de cobertura: cada item tem de comecar exatamente onde o
    # anterior terminou (a unica coisa entre os dois, feita pelo proprio
    # split_declarations, e' espaco em branco puro -- ver o loop de skip).
    prev_end = body_start
    leftover_chunks = []
    for kind, vis, start, end, extra in items:
        gap = masked[prev_end:start]
        if gap.strip():
            leftover_chunks.append(gap)
        prev_end = end
    tail = masked[prev_end:body_end]
    if tail.strip():
        leftover_chunks.append(tail)
    result.leftover = "".join(leftover_chunks)

    method_name_counts_for_pairstream = {}

    for kind, vis, start, end, extra in items:
        if kind in ("visibility", "macro", "preprocessor", "nested", "typedef"):
            continue

        term_idx, first_paren_idx = extra
        if kind == "attribute":
            declarator = masked[start:term_idx]
            type_text, attr_name, is_static = parse_attribute_declarator(declarator)
            if not type_text:
                continue

            if name == "Player" and attr_name in PLAYER_ROLE_FIELD_NAMES:
                bare_check, _, _ = normalize_attribute_type(type_text)
                if bare_check == "Pair":
                    # Substituido pelas 10 entradas typeid()-in-setter-body,
                    # acrescentadas depois do loop -- ver o comentario de
                    # PLAYER_ROLE_FIELD_NAMES acima.
                    continue

            bare, extra_stars, is_container = normalize_attribute_type(type_text)
            if not bare:
                result.attributes.append({
                    "name": attr_name, "type": type_text.strip(),
                    "visibility": vis, "static": is_static,
                })
                continue

            is_curated = bare in curated_names
            kind_slot, _ = (None, None)
            if not is_curated:
                kind_slot, _unit = classify_slot_type(bare, broad_inheritance, descendants_by_broad)

            if is_curated or (kind_slot == "object" and bare in broad_inheritance):
                if not is_curated:
                    outro_base = resolve_outro_base(bare, broad_inheritance)
                    outros.setdefault(bare, {"base": outro_base, "source": "discovered"})
                target_tier = 1 if bare in tier1_names else 2
                many = is_container or extra_stars >= 2
                if not many:
                    kind_for_mult, _ = classify_slot_type(bare, broad_inheritance, descendants_by_broad)
                    many = kind_for_mult in ("vector", "list")
                result.components.append({
                    "name": attr_name, "target": bare, "targetTier": target_tier,
                    "multiplicity": "many" if many else "one",
                    "resolvedVia": "direct-member", "visibility": vis,
                })
            else:
                result.attributes.append({
                    "name": attr_name, "type": type_text.strip(),
                    "visibility": vis, "static": is_static,
                })
            continue

        # kind == "method"
        declarator = masked[start:term_idx]
        text_before_paren = declarator[: (first_paren_idx - start)] if first_paren_idx is not None else declarator
        method_name = parse_method_name(text_before_paren)
        signature = re.sub(r"\s+", " ", declarator).strip()
        is_virtual = bool(re.search(r"\bvirtual\b", text_before_paren))
        is_static_m = bool(re.search(r"\bstatic\b", text_before_paren))
        is_override = bool(re.search(r"\boverride\b", declarator))
        is_const = bool(re.search(r"\)\s*(?:const)\b", declarator))
        is_pure = bool(re.search(r"=\s*0\s*$", declarator))
        result.methods.append({
            "name": method_name, "visibility": vis, "static": is_static_m,
            "virtual": is_virtual, "override": is_override,
            "pureVirtual": is_pure, "const": is_const, "signature": signature,
        })
        if name == "PairStream" and method_name not in ("PairStream",):
            for token in re.findall(r"\b(?:const\s+)?(\w+)\s*[*&]", declarator):
                if token in ("const", "unsigned", "int", "void", "bool"):
                    continue
                method_name_counts_for_pairstream[token] = method_name_counts_for_pairstream.get(token, 0) + 1

    if name == "Player":
        for role in extract_primary_components(MIXR_SRC):
            base_class = role["baseClass"]
            outros.setdefault(base_class, {"base": None, "source": "role"})
            result.components.append({
                "name": role["role"], "target": base_class, "targetTier": 2,
                "multiplicity": "one", "resolvedVia": "typeid()-in-setter-body",
                "visibility": "private",
            })

    if name == "PairStream" and method_name_counts_for_pairstream:
        best_target, best_count = max(method_name_counts_for_pairstream.items(), key=lambda kv: kv[1])
        if best_count >= PAIRSTREAM_INFERENCE_THRESHOLD:
            if best_target not in curated_names:
                outros.setdefault(best_target, {"base": resolve_outro_base(best_target, broad_inheritance), "source": "discovered"})
            result.components.append({
                "name": "(contents)", "target": best_target,
                "targetTier": 1 if best_target in tier1_names else 2,
                "multiplicity": "many", "resolvedVia": "inferred-from-methods",
                "visibility": "public",
            })

    return result


def build_diagram():
    tier1_names = set(TIER1_HEADERS)
    curated_names = tier1_names | TIER2_SEED_NAMES

    broad_inheritance = build_broad_inheritance()
    descendants_by_broad = build_descendants(broad_inheritance)

    outros = {}
    warnings = []
    classes = {}
    for name, (namespace, header_rel) in TIER1_HEADERS.items():
        result = extract_tier1_class(
            name, namespace, header_rel, curated_names, tier1_names,
            broad_inheritance, descendants_by_broad, outros, warnings,
        )
        if result is None:
            continue
        classes[name] = {
            "file": header_rel,
            "namespace": namespace,
            "base": result.base,
            "attributes": result.attributes,
            "components": result.components,
            "methods": result.methods,
            "_bodyLength": result.body_len,
            "_leftover": result.leftover,
        }

    # tier2 seed (papeis + demais alvos citados no plano)
    tier2 = {}
    for seed_name in TIER2_SEED_NAMES:
        source = "role" if seed_name in TIER2_ROLE_NAMES else "seed"
        base = resolve_outro_base(seed_name, broad_inheritance)
        tier2[seed_name] = {"base": base, "source": source}
    for outro_name, info in outros.items():
        if outro_name in tier2 or outro_name in tier1_names:
            continue
        tier2[outro_name] = info

    # Fecha o grafo por BASE: toda classe Tier-1 (ou Tier-2 ja descoberta)
    # cuja base caia FORA do conjunto curado tambem vira um stub Tier-2 --
    # e' assim que 'AbstractNetIO' (base de NetIO) e
    # 'AbstractRecorderComponent' (base de OutputHandler/AbstractDataRecorder)
    # entram, exatamente os dois exemplos citados no plano. Ponto fixo, capado
    # (a cadeia de heranca real e' curta -- termina em Object/Referenced, que
    # ja sao Tier-1).
    known = set(classes) | set(tier2)
    for _ in range(10):
        pending_bases = set()
        for cname, cdata in classes.items():
            if cdata["base"] and cdata["base"] not in known:
                pending_bases.add(cdata["base"])
        for tname, tdata in tier2.items():
            if tdata["base"] and tdata["base"] not in known:
                pending_bases.add(tdata["base"])
        if not pending_bases:
            break
        for b in pending_bases:
            tier2[b] = {"base": resolve_outro_base(b, broad_inheritance), "source": "discovered"}
            known.add(b)

    return classes, tier2, warnings


def self_check(classes, warnings):
    problems = list(warnings)

    for name in TIER1_HEADERS:
        if name not in classes:
            problems.append(f"{name}: ausente do resultado (corpo nao encontrado)")

    for name, data in classes.items():
        body_len = data["_bodyLength"]
        leftover = data["_leftover"]
        if body_len <= 0:
            problems.append(f"{name}: corpo vazio (body_len={body_len})")
        if len(leftover.strip()) > 8:
            problems.append(
                f"{name}: sobra de texto nao classificado ({len(leftover.strip())} chars): "
                f"{leftover.strip()[:120]!r}"
            )

    # Sentinela dura, conferida a mao lendo Referenced.hpp linha a linha
    # (nao um numero repetido de outro lugar): 2 atributos (semaphore,
    # refCount), 7 metodos (ctor default, ctor de copia deletado,
    # operator= deletado, destrutor puro, getRefCount, ref, unref), 0
    # componentes (Exception/ExpInvalidRefCount/ExpInvalidRefCountDelete sao
    # tipos aninhados, nao atributos).
    ref = classes.get("Referenced")
    if ref:
        if len(ref["attributes"]) != 2:
            problems.append(f"Referenced: esperava 2 atributos, achou {len(ref['attributes'])}")
        if len(ref["methods"]) != 7:
            problems.append(f"Referenced: esperava 7 metodos, achou {len(ref['methods'])}")
        if len(ref["components"]) != 0:
            problems.append(f"Referenced: esperava 0 componentes, achou {len(ref['components'])}")
        if ref["base"] is not None:
            problems.append(f"Referenced: esperava base=None, achou {ref['base']!r}")

    return problems


def main():
    classes, tier2, warnings = build_diagram()
    problems = self_check(classes, warnings)

    # _bodyLength/_leftover sao so' para a auto-verificacao -- nao vazam
    # para o JSON de saida.
    public_classes = {}
    for name, data in classes.items():
        public_classes[name] = {k: v for k, v in data.items() if not k.startswith("_")}

    output = {
        "generatedBy": "tools/extract_class_diagram.py",
        "classes": public_classes,
        "tier2": tier2,
    }
    print(json.dumps(output, indent=2, ensure_ascii=False))

    if problems:
        for p in problems:
            print(f"AUTO-VERIFICACAO FALHOU: {p}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
