#!/usr/bin/env python3
"""Extrai, do fonte REAL do MIXR e do plugin flight, a cadeia de heranca e quais
classes de fato fazem trabalho de fase.

Nao existe check-edl.py neste repositorio para reaproveitar (docs/TODO.md presumia
que existia) -- este e um extrator novo, pequeno, sem dependencias, que serve de
oraculo para docs/manual/index.html: qualquer nome de classe/fabrica e qualquer
afirmacao de "isto e ocioso" usada na arvore da UI tem de bater com a saida daqui.

O que ele faz, dois passos:

  1) varre os headers (.hpp) de contexts/src/mixr/include/mixr e models/player/A4/include
     por DECLARE_SUBCLASS(Tipo, Base) -- monta a cadeia de heranca (Tipo -> Base).
     varre os .cpp de contexts/src/mixr/src e models/player/A4/src por
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
# 'models/' -- deliberadamente a pasta INTEIRA, nao 'models/player/A4/include' como um
# primeiro corte deste script supunha. models/ esteve em reorganizacao (confirmado
# rodando: AlertDatalink.hpp saiu de models/A4/include/xnative/ para
# models/player/A4/include/xnative/ ENQUANTO este script era escrito, por uma
# sessao concorrente) -- qualquer subcaminho fixo aqui ficaria errado no proximo commit.
# rglob() sobre a pasta toda e imune a esse tipo de mudanca de endereco.
MODELS_DIR = REPO_ROOT / "models"
SHARED_DIR = REPO_ROOT / "shared"
DEFAULT_SCENARIO = REPO_ROOT / "src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in"

INCLUDE_ROOTS = [MIXR_INCLUDE, MODELS_DIR, SHARED_DIR]
SRC_ROOTS = [MIXR_SRC, MODELS_DIR, SHARED_DIR]

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
    so automatizada.

    PRIMEIRO achado vence por (classe, metodo) -- nao sobrescreve -- pelo
    MESMO motivo de extract_slots()/extract_slot_types() acima: varrer
    models/ inteiro (incluindo models/player/fixtures/stub, que
    deliberadamente reimplementa FlightAction::execute/etc. com o MESMO
    nome, so para testar contrato de carga de plugin) faz duas definicoes
    REAIS do mesmo (Classe, metodo) aparecerem em arquivos diferentes.
    Confirmado rodando: sem a guarda, 'FlightAction::execute' saia
    apontando pra stub.cpp (varrido depois de A4 na ordem alfabetica),
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
SLOT_NAME_RE = re.compile(r'"([A-Za-z0-9_]+)"')


def extract_slots(cpp_roots):
    """Classe -> [{name, comment}], a partir de BEGIN_SLOTTABLE(Classe) ...
    END_SLOTTABLE(Classe) (macros.hpp:291-298) -- e a lista de nomes que um
    .edl pode configurar nessa classe: a 'interface' publica dela, no sentido
    EDL. So le linhas comecando com uma string entre aspas (o formato real,
    confirmado lendo Antenna.cpp:29-37).

    PRIMEIRO achado vence (nunca sobrescreve) quando o mesmo nome de classe
    aparece em mais de um arquivo -- relevante para --edl-catalog, que varre
    'models/' (deste repositorio) inteiro: 'models/player/A4' (a implementacao
    de producao) e 'models/player/fixtures/stub' (um modelo minimo, so pra
    testar o CONTRATO de carga de plugin) declaram as MESMAS classes
    (BtBehavior, AlertDatalink, ...) com o MESMO nome barra, e 'A4' < 'fixtures'
    alfabeticamente -- sem 'first wins', o stub (varrido depois, deliberadamente
    mais simples) sobrescrevia os slots REAIS de producao, confirmado rodando:
    BtBehavior saia com 5 slots em vez dos ~19 de verdade. --catalog (o modo
    pre-existente, restrito a contexts/src/mixr/src/models/) nunca tinha essa
    colisao para comecar -- esse universo nem inclui models/player/."""
    slots: dict[str, list[dict]] = {}
    for f in iter_files(cpp_roots, {".cpp"}):
        text = f.read_text(encoding="utf-8", errors="replace")
        masked = mask_source(text)
        for m in SLOT_RE.finditer(masked):
            cls = m.group(1)
            if cls in slots:
                continue
            body_masked = m.group(2)
            body_real = text[m.start(2) : m.end(2)]
            out = []
            for line_masked, line_real in zip(body_masked.split("\n"), body_real.split("\n")):
                # findall, nao match(): uma linha pode ter MAIS DE UM slot
                # (estilo compacto, ex. shared/xmsg/MsgFeed.cpp:26-28:
                # '"trackManager", "maxPlayers", "healthEvery", ...,' numa
                # linha so -- confirmado rodando, .match() por linha perdia
                # todos menos o primeiro). Comentario de fim de linha, quando
                # existe, so faz sentido atribuido a um unico nome -- com
                # mais de um nome na linha, fica vazio (nao ha como saber de
                # qual item o comentario fala).
                names = SLOT_NAME_RE.findall(line_masked)
                if not names:
                    continue
                comment = ""
                if len(names) == 1 and "//" in line_real:
                    comment = line_real.split("//", 1)[1].strip()
                out.extend({"name": name, "comment": comment} for name in names)
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


# ---------------------------------------------------------------------------
# --edl-catalog: catalogo COMPLETO (todas as factories nativas -- base/models/
# terrain/recorder/interop/simulation/linkage -- mais shared/x* e os plugins
# deste repositorio) com tipo/unidade/cardinalidade por slot, para dirigir a
# paleta e os widgets do editor grafico de .edl (src/ui/edl_builder.jsx).
#
# Aditivo: nao toca build_catalog()/--catalog (usado por docs/manual/doc.jsx, universo
# deliberadamente restrito a mixr::models -- ver o comentario de
# MIXR_MODELS_SRC acima). Aqui o universo e SRC_ROOTS/INCLUDE_ROOTS, que ja
# cobrem tudo (linha 64-65) -- generalizar a CHAMADA das funcoes que ja
# existem, nao a funcao.
# ---------------------------------------------------------------------------

# --edl-catalog restringe o universo aos MESMOS modulos que
# app/src/mixr_factory.cpp de fato encadeia em mixrFactoryBuiltin() -- nao
# "tudo sob contexts/src/mixr/{src,include}", que tambem tem interop/common,
# interop/hla, interop/rprfom e linearsystem/, NENHUM encadeado ali (o dis so
# entra via mixr::dis::factory(), que e mixr::interop::dis por baixo -- ver o
# comentario da secao "src/poc/dis/bandit" no CLAUDE.md). Isso nao e so escopo:
# interop/hla e interop/rprfom tem classes com o MESMO NOME BARRA (sem
# namespace) de classes DIFERENTES e de fato usadas -- Aircraft, GroundVehicle,
# NetIO, Nib, Ntm -- confirmado rodando: build_inheritance()/extract_slots()
# indexam por nome de classe SEM namespace (build_inheritance() so guarda "o
# ultimo segmento", linha ~109), entao "Aircraft" (mixr::models, 45 slots
# reais) e "Aircraft" (mixr::rprfom::RprFom.cpp, um NIB sem relacao nenhuma)
# colidem na mesma chave, e quem "ganha" depende so da ordem alfabetica de
# arquivo -- um risco real de corromper a entrada MAIS importante do catalogo
# (Aircraft) dependendo de qual arquivo e varrido por ultimo. Restringir aos
# modulos de fato encadeados elimina a colisao (nenhuma classe realmente usada
# neste repositorio reaparece com o mesmo nome barra em dois desses sete
# modulos) e tambem tira do catalogo fabricas que o parser deste repositorio
# nunca alcancaria de qualquer forma (HlaNetIO, RprFomNetIO, ...).
#
# Limitacao conhecida, restante mesmo com este recorte -- nao redescobrir:
# base::FileReader (fabrica "FileReader") e recorder::FileReader (fabrica
# "RecorderFileReader") sao DUAS classes diferentes com o MESMO nome barra
# "FileReader", uma em base/ e outra em recorder/ -- as duas ficam no
# catalogo (nenhuma fabrica e perdida, confirmado rodando), mas ambas mostram
# a cadeia/slots de qualquer uma que a varredura processar por ultimo
# (build_inheritance()/extract_slots() indexam por classe, sem namespace).
# Caso raro (so este par, dos 16 nomes colidentes originais, sobrevive a
# exclusao de interop/hla e interop/rprfom) e de baixo impacto (nenhum
# cenario real deste repositorio usa FileReader/RecorderFileReader) --
# resolver de verdade exigiria chave qualificada por namespace em
# build_inheritance()/extract_slots(), fora do escopo desta extensao.
EDL_CATALOG_MIXR_MODULES = [
    "base", "models", "terrain", "interop/dis", "linkage", "recorder", "simulation",
]
EDL_CATALOG_INCLUDE_ROOTS = [MIXR_INCLUDE / m for m in EDL_CATALOG_MIXR_MODULES] + [
    MODELS_DIR, SHARED_DIR,
]
EDL_CATALOG_SRC_ROOTS = [MIXR_SRC / m for m in EDL_CATALOG_MIXR_MODULES] + [
    MODELS_DIR, SHARED_DIR,
]

SLOT_MAP_RE = re.compile(
    r"BEGIN_SLOT_MAP\s*\(\s*(\w+)\s*\)(.*?)END_SLOT_MAP\s*\(\s*\)", re.S
)
# END_SLOT_MAP() nao repete o nome da classe (macros.hpp:313-315, ao contrario
# de END_SLOTTABLE(ThisType)) -- por isso o casamento e so pelo par mais
# proximo, nao por backreference.
ON_SLOT_RE = re.compile(r"\bON_SLOT\s*\(\s*(\d+)\s*,\s*(\w+)\s*,\s*([\w:]+)\s*\)")

NUMBER_TYPES = {"Number", "Integer"}
BOOLEAN_TYPES = {"Boolean"}
TEXT_TYPES = {"String", "Identifier"}
VECTOR_TYPES = {"List"}   # [ n n n ] -- numlist, edl_parser.y:189-191
LIST_TYPES = {"PairStream"}  # { ... } -- sempre um PairStream, edl_parser.y


def build_descendants(inheritance):
    """Inverte Tipo->Base (ja extraido por build_inheritance) em Base->[Tipos].
    E o que da, para qualquer 'familia de unidade' (Distance, Angle, ...), a
    lista de unidades CONCRETAS que a UI pode oferecer num dropdown -- sem
    tabela fixa nenhuma, so grafo de heranca ja extraido do fonte real."""
    out: dict[str, list[str]] = {}
    for child, parent in inheritance.items():
        out.setdefault(parent, []).append(child)
    return out


def collect_descendants(root, descendants):
    """Todos os descendentes TRANSITIVOS de root (BFS sobre 'descendants')."""
    out, seen, stack = [], set(), list(descendants.get(root, []))
    while stack:
        c = stack.pop()
        if c in seen:
            continue
        seen.add(c)
        out.append(c)
        stack.extend(descendants.get(c, []))
    return sorted(out)


def classify_slot_type(name, inheritance, descendants):
    """Um tipo aceito por ON_SLOT vira um destes 'kinds': number/boolean/text/
    vector/list/unit/object.

    'unit' exige que o tipo (a) derive de Number E (b) tenha pelo menos um
    descendente concreto -- e o que separa 'Distance' (familia de unidade de
    verdade, Meters/Feet/NauticalMiles/... por baixo) de 'LatLon' (tambem
    deriva de Number -- DECLARE_SUBCLASS(LatLon, Number), confirmado no fonte
    -- mas e um composto sem filho nenhum: direction/degrees/minutes/seconds
    sao slots PROPRIOS dela, nao uma unidade). Sem a condicao (b), LatLon
    virava 'unit' por engano e ganhava um dropdown de unidade vazio."""
    if name in NUMBER_TYPES:
        return "number", None
    if name in BOOLEAN_TYPES:
        return "boolean", None
    if name in TEXT_TYPES:
        return "text", None
    if name in VECTOR_TYPES:
        return "vector", None
    if name in LIST_TYPES:
        return "list", None
    chain = resolve_chain(name, inheritance)
    if len(chain) > 1 and "Number" in chain[1:] and descendants.get(name):
        return "unit", name
    return "object", name


def extract_slot_types(cpp_roots):
    """Classe -> {indice: [tipos C++ aceitos]}, a partir de ON_SLOT dentro de
    BEGIN_SLOT_MAP(Classe)/END_SLOT_MAP() (macros.hpp:302-324). Mais de um
    ON_SLOT no mesmo indice = tipo uniao (ex.: Antenna.beamWidth aceita Angle
    OU Number; RfSensor.modes aceita PairStream OU RfSensor).

    SEMPRE roda sobre texto MASCARADO -- sem isso, ON_SLOT comentado (ex.:
    IrSensor.cpp:54-56, dois slots do indice 7 desativados no fonte real)
    vira falso positivo e o indice 7 pareceria aceitar 3 tipos em vez de 1.
    Confirmado rodando sem o mascaramento antes de escrever esta versao.

    PRIMEIRO achado vence, por classe inteira (mesmo motivo/mesma ordem de
    varredura de extract_slots() acima -- 'models/player/A4' antes de
    'models/player/fixtures/stub'): sem isso, os INDICES de ON_SLOT de dois
    arquivos diferentes para a mesma classe se MISTURARIAM num so
    dicionario, o que faz ainda menos sentido que so perder um dos dois --
    os indices de um arquivo nao tem relacao nenhuma com os do outro."""
    out: dict[str, dict[int, list[str]]] = {}
    seen_classes = set()
    for f in iter_files(cpp_roots, {".cpp"}):
        masked = mask_source(f.read_text(encoding="utf-8", errors="replace"))
        for m in SLOT_MAP_RE.finditer(masked):
            cls, body = m.group(1), m.group(2)
            if cls in seen_classes:
                continue
            seen_classes.add(cls)
            per_idx = out.setdefault(cls, {})
            for sm in ON_SLOT_RE.finditer(body):
                idx = int(sm.group(1))
                bare = sm.group(3).rsplit("::", 1)[-1]
                lst = per_idx.setdefault(idx, [])
                if bare not in lst:
                    lst.append(bare)
    return out


def build_class_files(cpp_roots):
    """Classe -> arquivo .cpp onde IMPLEMENT_*SUBCLASS(Classe, ...) aparece --
    um passe so sobre todos os arquivos (ao contrario de find_impl_file(), que
    refaz a varredura inteira por classe -- caro demais para rodar uma vez por
    classe do catalogo inteiro)."""
    out: dict[str, str] = {}
    for f in iter_files(cpp_roots, {".cpp"}):
        masked = mask_source(f.read_text(encoding="utf-8", errors="replace"))
        for m in IMPLEMENT_RE.finditer(masked):
            out.setdefault(m.group(1), str(f.relative_to(REPO_ROOT)))
    return out


def origin_of(impl_file):
    """De onde uma classe vem, pelo caminho do .cpp que a implementa: uma
    factory nativa do fork (agrupada pelo primeiro nivel sob
    contexts/src/mixr/src/, com interop/dis e interop/rprfom mantidos
    distintos), uma lib propria deste repo (shared/x*), ou algo do USUARIO
    sob ./models/ -- QUALQUER coisa la (nao so models/player/<nome>/): um
    plugin de player, um fixture de teste (models/player/fixtures/<nome>/),
    ou um payload de evento (models/events/<nome>/...). Generico de
    proposito -- um modelo novo em models/<qualquer-coisa>/ ja aparece
    rotulado na hora, sem precisar editar este arquivo.

    Confirmado rodando: antes desta generalizacao, TacticalAlert (o
    payload real do evento de alerta tatico, implementado em
    models/events/payloads/EID_ALERT/TacticalAlert.cpp -- nao um
    'models/player/<nome>/') caia no 'return "builtin"' por engano, como
    se fosse uma classe nativa do MIXR."""
    if impl_file is None:
        return "unknown"
    if impl_file.startswith("models/player/fixtures/"):
        # pula os segmentos 'player'/'fixtures' -- o nome que importa e o
        # do modelo (models/player/fixtures/stub/... -> "stub", nao
        # "fixtures").
        rest = impl_file[len("models/player/fixtures/"):].split("/")
        return f"plugin:{rest[0]}"
    if impl_file.startswith("models/player/"):
        rest = impl_file[len("models/player/"):].split("/")
        return f"plugin:{rest[0]}"
    if impl_file.startswith("models/"):
        # qualquer outra coisa sob models/ que nao seja um player -- ex.:
        # models/events/payloads/EID_ALERT/TacticalAlert.cpp -> "events".
        rest = impl_file[len("models/"):].split("/")
        return f"plugin:{rest[0]}"
    if impl_file.startswith("shared/"):
        return "shared"
    prefix = "contexts/src/mixr/src/"
    if impl_file.startswith(prefix):
        parts = impl_file[len(prefix):].split("/")
        if parts[0] == "interop" and len(parts) > 1:
            return f"interop/{parts[1]}"
        return parts[0]
    return "builtin"


def load_edl_catalog_overrides():
    path = REPO_ROOT / "src" / "ui" / "edl_catalog_overrides.json"
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def build_edl_catalog():
    """Catalogo completo para o editor grafico de .edl: por classe (de TODAS
    as factories, nao so mixr::models), slots ACHATADOS por heranca, com
    tipo(s)/unidade/cardinalidade por slot -- derivados mecanicamente do
    fonte. Nao ha lista fixa de 'familias de unidade': uma familia e qualquer
    tipo que deriva de Number e tem descendente concreto (classify_slot_type),
    e os proprios descendentes (collect_descendants) viram as opcoes do
    dropdown. A UNICA curadoria manual e src/ui/edl_catalog_overrides.json,
    para os poucos slots 'referencia por nome' que o TIPO sozinho (String,
    igual a qualquer rotulo comum) nao distingue."""
    # Inclui tambem as raizes de SRC (nao so INCLUDE) na varredura de
    # heranca: dez headers deste fork moram junto do .cpp em vez de em
    # include/ (ex.: linkage/platform/UsbJoystick_linux.hpp) -- sem isso,
    # UsbJoystick saia sem base nenhuma (chain=['UsbJoystick'], sem herdar
    # 'adapters' de IoDevice), confirmado rodando contra o cenario real do
    # 'bandit' (que declara justamente 'adapters:' num UsbJoystick).
    # iter_files() so pega .hpp/.h aqui, entao os .cpp destas mesmas
    # pastas nao sao lidos de novo por engano.
    inheritance = build_inheritance(EDL_CATALOG_INCLUDE_ROOTS + EDL_CATALOG_SRC_ROOTS)
    factory_map = build_factory_map(EDL_CATALOG_SRC_ROOTS)   # nome-de-fabrica -> classe
    slot_names = extract_slots(EDL_CATALOG_SRC_ROOTS)        # classe -> [{name, comment}]
    slot_types = extract_slot_types(EDL_CATALOG_SRC_ROOTS)   # classe -> {indice: [tipos]}
    class_files = build_class_files(EDL_CATALOG_SRC_ROOTS)   # classe -> arquivo .cpp

    # `dis::NetIO`/`dis::Ntm`/`dis::Nib` tem o MESMO nome barra que a propria
    # classe BASE delas (`DECLARE_SUBCLASS(NetIO, interop::NetIO)` -- o
    # segundo argumento vira bare "NetIO" apos build_inheritance() descartar
    # o namespace, IGUAL ao nome da propria classe) -- um self-loop que
    # resolve_chain() nao consegue atravessar (confirmado rodando:
    # inheritance["NetIO"] == "NetIO"). Sintoma medido, antes deste
    # remendo: o catalogo perdia 'inputEntityTypes'/'outputEntityTypes'
    # (declarados em interop/common/NetIO.cpp, nunca alcancados) -- e o
    # lint (scripts/edl_lint.py) acusava "slot desconhecido" em cenarios
    # de PRODUCAO reais que usam exatamente esses dois slots. Corrigido
    # varrendo interop/common (que NAO entra no restante do catalogo -- ver
    # o comentario de EDL_CATALOG_MIXR_MODULES: nada de la e encadeado pelo
    # `app/src/mixr_factory.cpp` deste repositorio, entao nao deveria
    # aparecer na PALETA) so para EMENDAR o nivel-base que falta, sob uma
    # chave sintetica que quebra o self-loop sem contaminar o restante do
    # catalogo. Generico (detecta QUALQUER self-loop, nao so estes tres) --
    # se um dia mais uma classe de dis/ ganhar o mesmo padrao, o remendo ja
    # cobre sem precisar de outra entrada hardcoded.
    self_loop = sorted(c for c, b in inheritance.items() if b == c)
    if self_loop:
        common_roots_inc = [MIXR_INCLUDE / "interop/common"]
        common_roots_src = [MIXR_SRC / "interop/common"]
        common_inheritance = build_inheritance(common_roots_inc)
        common_slot_names = extract_slots(common_roots_src)
        common_slot_types = extract_slot_types(common_roots_src)
        for c in self_loop:
            common_base = common_inheritance.get(c)
            if not common_base or common_base == c:
                continue  # nao achou base de verdade em interop/common -- deixa o self-loop como esta
            alias = f"{c}@interop_common"
            slot_names[alias] = common_slot_names.get(c, [])
            slot_types[alias] = common_slot_types.get(c, {})
            inheritance[alias] = common_base
            for k, v in common_inheritance.items():
                inheritance.setdefault(k, v)  # o resto da cadeia acima (AbstractNetIO -> ... -> Object)
            inheritance[c] = alias  # quebra o self-loop

    descendants = build_descendants(inheritance)
    reference_slots = set(load_edl_catalog_overrides().get("referenceSlots", {}))

    # Classe -> UM nome de fabrica (o primeiro encontrado), usado so para
    # rotular as OPCOES de um dropdown de unidade (ex.: a classe Decibel vira
    # a opcao "dB" -- fatoria::factory_name diverge do nome da classe C++,
    # confirmado no fonte). NAO e a fonte das entradas do catalogo: uma classe
    # pode ter MAIS DE UM nome de fabrica de verdade (ex.: base::FileReader
    # registra tanto "FileReader" quanto -- via recorder::FileReader, MESMO
    # nome de classe, classe DIFERENTE -- "RecorderFileReader"; colapsar por
    # classe aqui perderia esse segundo, real, nome de fabrica).
    class_to_factory: dict[str, str] = {}
    for factory_name, cls in factory_map.items():
        class_to_factory.setdefault(cls, factory_name)

    def clean(name):
        # Descarta o sufixo sintetico '@interop_common' do remendo de
        # self-loop acima -- nunca deve vazar pro JSON de saida, so serviu
        # pra achatar corretamente por dentro.
        return name.split("@", 1)[0] if name else name

    entries = []
    for factory_name, cls in sorted(factory_map.items()):
        chain = resolve_chain(cls, inheritance)

        flat_slots_raw = []
        # Da RAIZ (perto de Object) para a FOLHA (cls): slots de classe-base
        # primeiro -- mesma ordem de numeracao encadeada do SlotTable real
        # (SlotTable::baseTable, ver shared/xplugin/README.md).
        for lvl in reversed(chain):
            lvl_names = slot_names.get(lvl, [])
            lvl_types = slot_types.get(lvl, {})
            for i, item in enumerate(lvl_names, start=1):
                name = item["name"]
                types = lvl_types.get(i, [])
                kinds = [classify_slot_type(t, inheritance, descendants) for t in types]
                unit_families = sorted({k[1] for k in kinds if k[0] == "unit"})
                object_types = sorted({k[1] for k in kinds if k[0] == "object"})
                flat_slots_raw.append({
                    "name": name,
                    "declaredIn": clean(lvl),
                    "comment": item["comment"],
                    "acceptsNumber": any(k[0] == "number" for k in kinds),
                    "acceptsBoolean": any(k[0] == "boolean" for k in kinds),
                    "acceptsText": any(k[0] == "text" for k in kinds),
                    "acceptsVector": any(k[0] == "vector" for k in kinds),
                    "acceptsChildList": any(k[0] == "list" for k in kinds),
                    "unitFamilies": [
                        {
                            "family": fam,
                            "options": [
                                class_to_factory.get(d, d)
                                for d in collect_descendants(fam, descendants)
                                if d in class_to_factory
                            ],
                        }
                        for fam in unit_families
                    ],
                    "objectTypes": object_types,
                    "isReference": name in reference_slots,
                })

        # Uma subclasse pode REDECLARAR um slot com o mesmo nome que a base
        # ja tinha (ex.: dis::NetIO redeclara 'maxTimeDR' etc., que ja vem
        # de interop::common::NetIO, com o mesmo par de tipos) -- mantem a
        # posicao da PRIMEIRA aparicao (ordem base->folha) mas os DADOS da
        # ULTIMA (a redeclaracao mais derivada vence, mesma semantica de
        # SlotTable/setSlotByIndex real).
        by_name, order = {}, []
        for s in flat_slots_raw:
            if s["name"] not in by_name:
                order.append(s["name"])
            by_name[s["name"]] = s
        flat_slots = [by_name[n] for n in order]

        entries.append({
            "class": cls,
            "factory": factory_name,
            "baseClass": clean(inheritance.get(cls)),
            "chain": [clean(lvl) for lvl in chain],
            "origin": origin_of(class_files.get(cls)),
            "slots": flat_slots,
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
    ap.add_argument(
        "--edl-catalog", action="store_true",
        help="cataloga TODAS as factories (base/models/terrain/recorder/interop/simulation/linkage, "
        "shared/x* e os plugins deste repositorio), com tipo/unidade/cardinalidade por slot, para o "
        "editor grafico de .edl (src/ui/edl_builder.jsx); ignora os argumentos posicionais; sempre "
        "imprime JSON",
    )
    args = ap.parse_args()

    if args.catalog:
        print(json.dumps(build_catalog(), indent=2, ensure_ascii=False))
        return

    if args.edl_catalog:
        print(json.dumps(build_edl_catalog(), indent=2, ensure_ascii=False))
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
