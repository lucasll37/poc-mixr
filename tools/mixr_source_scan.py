"""Primitivas de varredura do fonte C++ do MIXR, compartilhadas por DOIS
consumidores que variam so' o UNIVERSO de arquivos varrido, nunca a mecanica
de extracao:

  - tools/generate_manual_catalog.py -- para a aba Catalogo de
    docs/manual/doc.jsx, universo = as 7 factories nativas que
    models/BUILT-IN.md ja usa como escopo, mais o plugin de producao
    models/players/A-4.
  - src/ui/scripts/generate_edl_catalog.py -- para o editor grafico de .edl
    (src/ui/edl_builder.jsx), universo = todas as factories que
    app/src/mixr_factory.cpp de fato encadeia.

Nao duplicar estas funcoes entre os dois -- e o motivo deste arquivo existir.
"""
from __future__ import annotations

import re
from pathlib import Path

# Caminhos relativos a REPO_ROOT, comuns aos dois consumidores -- cada um
# resolve REPO_ROOT do proprio jeito (profundidade diferente sob a raiz do
# repositorio) e monta os Path() finais com esta base.
MIXR_INCLUDE_REL = "contexts/src/mixr/include/mixr"
MIXR_SRC_REL = "contexts/src/mixr/src"
MODELS_REL = "models"
LIBS_REL = "libs"

# A base pode vir qualificada por namespace (ex.: DECLARE_SUBCLASS(WorldModel,
# simulation::Simulation), DECLARE_SUBCLASS(SimAgent, base::ubf::Agent)) -- sem
# aceitar ':' aqui a regra nao casava e a classe saia com cadeia vazia, parecendo
# sem fonte por engano.
DECLARE_RE = re.compile(r"\bDECLARE_SUBCLASS\s*\(\s*(\w+)\s*,\s*([\w:]+)\s*\)")
IMPLEMENT_RE = re.compile(
    r'\bIMPLEMENT_(?:PARTIAL_|ABSTRACT_)?SUBCLASS\s*\(\s*(\w+)\s*,\s*"([^"]+)"\s*\)'
)

# Idioma de despacho -- identico nos 7 factory.cpp nativos e nos factory.cpp
# proprios deste repositorio (models/players/*/src/xnative/, libs/x*/) --
# sempre 'name == X::getFactoryName()', por REFERENCIA de classe, nunca uma
# string literal do lado da comparacao. IMPLEMENT_RE sozinho superconta: uma
# classe pode ter IMPLEMENT_SUBCLASS no proprio .cpp sem NUNCA ganhar um
# branch de despacho no factory.cpp do modulo (ex.: mixr::base::Component,
# mixr::models::DynamicsModel/IrSystem, os 6 orfaos de mixr::models
# documentados em models/BUILT-IN.md) -- essas classes compilam, mas
# mixrFactory() nunca as constroi a partir de um .edl. So o texto do
# factory.cpp responde isso; nao ha como derivar de IMPLEMENT_RE sozinho.
DISPATCH_RE = re.compile(r"\bname\b\s*==\s*((?:\w+::)*\w+)::getFactoryName\s*\(\s*\)")

SLOT_RE = re.compile(
    r'BEGIN_SLOTTABLE\s*\(\s*(\w+)\s*\)(.*?)END_SLOTTABLE\s*\(\s*\1\s*\)', re.S
)
SLOT_NAME_RE = re.compile(r'"([A-Za-z0-9_]+)"')


def iter_files(roots, suffixes):
    for root in roots:
        if not root.exists():
            continue
        for path in sorted(root.rglob("*")):
            if path.suffix in suffixes:
                yield path


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


def find_dispatch_reachable_classes(factory_cpp_paths):
    """Nomes de classe (sem namespace) com despacho REAL num dado conjunto de
    factory.cpp -- regex sobre o texto mask_source()'d de cada arquivo, sem
    presumir uma unica cadeia if/else-if contigua (models/factory.cpp tem
    DUAS cadeias 'if' separadas, uma apos a outra -- uma varredura de texto
    puro cobre isso sem precisar entender controle de fluxo). Refs
    qualificadas por namespace (ubf::Agent::getFactoryName(),
    events::TacticalAlert::getFactoryName()) sao normalizadas para o ultimo
    segmento, igual build_inheritance() ja faz para a base de DECLARE_SUBCLASS.

    Caminhos que nao existem sao ignorados em silencio (mesma tolerancia de
    iter_files) -- convivel com um chamador que monta a lista via glob sem
    saber de antemao quais plugins/libs existem.

    Uso tipico, combinando com build_factory_map() (NomeDeFabrica -> Tipo):
        factory_map = build_factory_map([modulo_src_root])
        reachable = find_dispatch_reachable_classes([modulo_src_root / "factory.cpp"])
        concretas = {cls for cls in factory_map.values() if cls in reachable}

    Nao serve para 'models/template/src/mirror.cpp' (o mirror de
    contrato) -- esse arquivo usa um idioma totalmente diferente (funcao
    'fabrica', comparacao por std::strcmp com string literal, sem
    'getFactoryName()' nenhum) e e deliberadamente fora do escopo de
    producao (ver .claude/rules/models-plugin.md); use
    'models/template/src/xnative/factory.cpp' (o scaffold real) para
    as classes construiveis do template."""
    reachable = set()
    for path in factory_cpp_paths:
        if not path.exists():
            continue
        masked = mask_source(path.read_text(encoding="utf-8", errors="replace"))
        for m in DISPATCH_RE.finditer(masked):
            reachable.add(m.group(1).rsplit("::", 1)[-1])
    return reachable


def resolve_chain(cls, inheritance):
    seen = []
    cur = cls
    guard = 0
    while cur and cur not in seen and guard < 50:
        seen.append(cur)
        cur = inheritance.get(cur)
        guard += 1
    return seen


def extract_slots(cpp_roots):
    """Classe -> [{name, comment}], a partir de BEGIN_SLOTTABLE(Classe) ...
    END_SLOTTABLE(Classe) (macros.hpp:291-298) -- e a lista de nomes que um
    .edl pode configurar nessa classe: a 'interface' publica dela, no sentido
    EDL. So le linhas comecando com uma string entre aspas (o formato real,
    confirmado lendo Antenna.cpp:29-37).

    PRIMEIRO achado vence (nunca sobrescreve) quando o mesmo nome de classe
    aparece em mais de um arquivo -- relevante para o catalogo do editor
    grafico, que varre 'models/' (deste repositorio) inteiro:
    'models/players/A-4' (a implementacao de producao) e
    'models/template' (o mirror de contrato em src/mirror.cpp, um
    modelo minimo so pra testar o CONTRATO de carga de plugin) declaram as
    MESMAS classes (BtBehavior, AlertDatalink, ...) com o MESMO nome barra,
    e 'A-4' < 'template' alfabeticamente -- sem 'first wins', o mirror
    (varrido depois, deliberadamente mais simples) sobrescrevia os slots
    REAIS de producao, confirmado rodando: BtBehavior saia com 5 slots em
    vez dos ~19 de verdade. O modo tabela de tools/extract_execution_chain.py
    (restrito a contexts/src/mixr/src/models/) nunca tinha essa colisao para
    comecar -- esse universo nem inclui models/players/."""
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
                # (estilo compacto, ex. libs/xmsg/MsgFeed.cpp:26-28:
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


# ---------------------------------------------------------------------------
# Classificacao de tipo de slot ('kind') e os 10 'papeis primarios' de Player
# -- promovidos de src/ui/scripts/generate_edl_catalog.py (o segundo
# consumidor a precisar disto) para ca, agora que tools/extract_class_diagram.py
# se torna o TERCEIRO: a normalizacao de tipo usada para separar 'atributo
# escalar' de 'composicao com outra classe MIXR' no diagrama de classe e' a
# MESMA regra que ja separa slot escalar de slot-objeto no editor grafico de
# .edl -- nao reimplementar (o motivo deste arquivo existir, ver o topo).
# 'src/ui/scripts/generate_edl_catalog.py' volta a importar estes nomes
# daqui, mesmo padrao de alias que ja usa para mask_source/build_inheritance/
# build_factory_map/extract_slots/find_matching_brace.

NUMBER_TYPES = {"Number", "Integer"}
BOOLEAN_TYPES = {"Boolean"}
TEXT_TYPES = {"String", "Identifier"}
VECTOR_TYPES = {"List"}   # [ n n n ] -- numlist, edl_parser.y:189-191
LIST_TYPES = {"PairStream"}  # { ... } -- sempre um PairStream, edl_parser.y


def build_descendants(inheritance):
    """Inverte Tipo->Base (ja extraido por build_inheritance) em Base->[Tipos].
    E o que da, para qualquer 'familia de unidade' (Distance, Angle, ...), a
    lista de unidades CONCRETAS que um consumidor pode oferecer (ex.: num
    dropdown) -- sem tabela fixa nenhuma, so grafo de heranca ja extraido do
    fonte real."""
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
    """Um tipo aceito por ON_SLOT (ou o tipo normalizado de um atributo C++,
    ver tools/extract_class_diagram.py) vira um destes 'kinds': number/
    boolean/text/vector/list/unit/object.

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


# 'papeis primarios' de Player -- dynamicsModel/pilot/navigation/datalink/
# radio/gimbal/rfSensor/irSystem/onboardComputer/storesMgr NAO sao slots (o
# catalogo de slots nao tem entrada nenhuma para eles): Player.hpp documenta
# explicitamente que esses nomes de chave em EDL sao COSMETICOS -- quem
# resolve de verdade e Player::updateSystemPointers() (Player.cpp), varrendo
# a lista generica 'components:' por TIPO C++ (findByType()), nunca por
# nome.
#
# Mecanico, sem tabela de curadoria: o PAPEL exibido vem do ARGUMENTO de
# typeid() (a classe-base esperada), nao do nome do setter C++ -- 'setSensor'
# (o setter real de RfSensor) viraria o papel 'sensor', que nao bate com a
# convencao de EDL de producao ('sensors:'/'rfSensor'). 'RfSensor' ->
# 'rfSensor' (so a primeira letra minuscula) ja bate com as 10 chaves
# convencionais usadas em producao, sem tabela nenhuma escrita a mao.
PRIMARY_COMPONENT_RE = re.compile(
    r"set\w+\(\s*findByType\(\s*typeid\(\s*(\w+)\s*\)\s*\)\s*\)"
)


def role_name_from_base_class(base_class):
    return base_class[:1].lower() + base_class[1:]


def extract_primary_components(mixr_src_root):
    """[{role, baseClass}], extraido do corpo de Player::updateSystemPointers()
    (<mixr_src_root>/models/player/Player.cpp) -- ver o comentario acima para
    o porque disso nao estar no catalogo de slots. 'mixr_src_root' e' o
    caminho ate contexts/src/mixr/src (cada consumidor resolve REPO_ROOT do
    proprio jeito -- ver o docstring deste arquivo -- por isso a raiz entra
    por parametro, nunca hard-coded aqui). Se o framework algum dia deixar de
    ter essa funcao nesse formato, isto devolve lista vazia (silencioso) --
    e' o teste de integracao de quem chama, nao este modulo, quem trava
    contra essa regressao."""
    player_cpp = Path(mixr_src_root) / "models/player/Player.cpp"
    if not player_cpp.exists():
        return []
    text = player_cpp.read_text(encoding="utf-8", errors="replace")
    masked = mask_source(text)
    sig = re.search(r"void\s+Player::updateSystemPointers\s*\([^)]*\)\s*\{", masked)
    if not sig:
        return []
    brace_start = masked.index("{", sig.start())
    brace_end = find_matching_brace(masked, brace_start)
    if brace_end == -1:
        return []
    body = masked[brace_start:brace_end]
    roles = []
    seen = set()
    for m in PRIMARY_COMPONENT_RE.finditer(body):
        base_class = m.group(1)
        if base_class in seen:
            continue
        seen.add(base_class)
        roles.append({"role": role_name_from_base_class(base_class), "baseClass": base_class})
    return roles
