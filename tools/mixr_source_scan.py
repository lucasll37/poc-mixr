"""Primitivas de varredura do fonte C++ do MIXR, compartilhadas por DOIS
consumidores que variam so' o UNIVERSO de arquivos varrido, nunca a mecanica
de extracao:

  - tools/extract_execution_chain.py (tabela/--catalog) -- para
    docs/manual/doc.jsx, universo restrito a mixr::models.
  - src/ui/scripts/generate_edl_catalog.py (--edl-catalog, ex-'--edl-catalog'
    deste proprio arquivo antes do split) -- para o editor grafico de .edl
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
    'models/player/A4' (a implementacao de producao) e
    'models/player/fixtures/stub' (um modelo minimo, so pra testar o
    CONTRATO de carga de plugin) declaram as MESMAS classes (BtBehavior,
    AlertDatalink, ...) com o MESMO nome barra, e 'A4' < 'fixtures'
    alfabeticamente -- sem 'first wins', o stub (varrido depois,
    deliberadamente mais simples) sobrescrevia os slots REAIS de producao,
    confirmado rodando: BtBehavior saia com 5 slots em vez dos ~19 de
    verdade. O modo tabela/--catalog (restrito a
    contexts/src/mixr/src/models/) nunca tinha essa colisao para comecar --
    esse universo nem inclui models/player/."""
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
