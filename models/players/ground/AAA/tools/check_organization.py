#!/usr/bin/env python3
# tools/check_organization.py -- linter de boas praticas de organizacao interna
# deste projeto de modelo.
#
# NAO e um teste na acepcao de tests/ (nao roda em CI, nao entra em
# 'make test', nao bloqueia build/install). E um linter OPCIONAL, mas
# fortemente recomendado: confere, em segundos e sem compilar nada, as
# convencoes que docs/CONTRATO.md, docs/ARCHITECTURE.md e o CLAUDE.md raiz
# ja descrevem em prosa -- coisas que hoje so' tem sintoma de falha em
# RUNTIME (ou nenhum sintoma nenhum, como a secao 3 do CONTRATO ja avisa).
#
# Uso:
#   python3 tools/check_organization.py    (ou 'make check-organization', que so chama isto)
#
# Saida: 0 se nao ha FALHA nenhuma (AVISO/EXCECAO/PULADO nao contam), 1 caso
# contrario -- util pra quem quiser encadear isto num hook proprio, mas o
# Makefile deste projeto NUNCA encadeia 'check-organization' em
# 'test'/'build'/'install' de proposito (ver o comentario de models/common.mk).
#
# Cada verificacao e AUTOCONTIDA (le so' os arquivos deste modelo, nunca
# depende de outro projeto do repositorio nem de o SDK estar publicado) e
# AGNOSTICA ao nome do modelo -- o nome do proprio namespace/artefato esperado
# e derivado de meson.build (project()), nunca escrito a mao aqui. Por isso a
# MESMA copia deste arquivo, byte a byte, serve pra qualquer modelo: o unico
# ponto pensado pra ser customizado POR modelo e KNOWN_EXCEPTIONS, logo
# abaixo -- excecoes ja documentadas em CLAUDE.md/CONTRATO.md, nao problemas
# esquecidos (ver o comentario dela).
from __future__ import annotations

import argparse
import os
import re
import sys
import textwrap
import xml.etree.ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODEL_ROOT = HERE.parent


def _find_models_root(start):
    """Sobe a partir de START ate achar o diretorio literalmente chamado
    'models' -- generico a QUALQUER profundidade de MODEL_ROOT sob models/
    (models/<nome>/, models/<categoria>/<nome>/, models/<categoria>/
    <subcategoria>/<nome>/, ...). Usado so' pela excecao documentada de
    'fabrica-tres-listas' que precisa alcancar models/events/ -- nunca por
    nenhuma outra verificacao deste arquivo (as demais sao autocontidas ao
    proprio modelo, ver o docstring do modulo). Devolve None se a busca
    chegar na raiz do sistema de arquivos sem achar 'models' (nao deveria
    acontecer para um script que mora sob models/<algo>/tools/, mas a
    verificacao que usa isto ja trata None como 'nao consegui alcancar
    models/events/', nunca como erro fatal)."""
    cur = start
    while cur.name != "models":
        if cur.parent == cur:
            return None
        cur = cur.parent
    return cur

# ==============================================================================
# Excecoes conhecidas e ACEITAS para ESTE modelo especifico -- documentadas,
# nao escondidas. Cada chave e o check_id de uma verificacao abaixo; o valor
# e o motivo (com referencia a onde a decisao ja foi tomada). O check AINDA
# RODA e AINDA REPORTA (rebaixado de FALHA para EXCECAO no lugar de sumir do
# relatorio) -- e assim que uma excecao conhecida fica visivel em vez de
# virar um "sempre foi assim" tacito.
#
# Vazio aqui, no scaffold/na maioria dos modelos: nao ha excecao nenhuma
# aceita por padrao. Se ESTE modelo tiver uma decisao ja tomada e registrada
# em CLAUDE.md/CONTRATO.md que bate com um check_id daqui, adicione a
# entrada -- nunca "corrija" a verificacao em si para parar de detectar o
# caso (isso apagaria o sinal pra todo modelo NOVO que copiar este arquivo).
# ==============================================================================
KNOWN_EXCEPTIONS: dict[str, str] = {}

MIXR_INCLUDE_RE = re.compile(r'#\s*include\s*[<"](mixr/[^">]+)[">]')
BT_INCLUDE_RE = re.compile(r'#\s*include\s*[<"](behaviortree_cpp_v3/[^">]+)[">]')
NS_OPEN_ONLY_RE = re.compile(r'^namespace\s+([A-Za-z_]\w*)\s*\{\s*$')
NS_OPEN_CLOSE_RE = re.compile(r'^namespace\s+([A-Za-z_]\w*)\s*\{.*\}\s*$')

# Quantos itens de detalhe cada verificacao mostra por padrao -- o resto fica
# atras de 'rode com -v'. Existe porque uma verificacao como
# 'namespace-aninhado' pode ter dezenas de arquivos fora do padrao, e
# despejar todos sempre torna o relatorio ilegivel.
MAX_DETAILS = 6


class Finding:
    __slots__ = ("check_id", "title", "why", "status", "details", "exception_note")

    def __init__(self, check_id, title, why, status, details=(), exception_note=None):
        self.check_id = check_id
        self.title = title
        self.why = why
        self.status = status  # "OK" | "FALHA" | "AVISO" | "PULADO" | "EXCECAO"
        self.details = list(details)
        self.exception_note = exception_note


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def rel(path: Path) -> str:
    return str(path.relative_to(MODEL_ROOT)).replace("\\", "/")


def source_files(*rel_dirs, suffixes=(".hpp", ".cpp", ".h")):
    out = []
    for rel_dir in rel_dirs:
        d = MODEL_ROOT / rel_dir
        if not d.is_dir():
            continue
        for p in sorted(d.rglob("*")):
            if p.is_file() and p.suffix in suffixes:
                out.append(p)
    return out


def project_name() -> "str | None":
    m = re.search(r"^project\(\s*'([^']+)'", read(MODEL_ROOT / "meson.build"), re.M)
    return m.group(1) if m else None


def expected_namespace(name: str) -> str:
    # Esta formula e a mesma de NS_NOVO em scripts/models.sh (raiz do
    # repositorio) -- duplicada em bash e Python porque as duas rodam em
    # momentos/linguagens diferentes (models.sh cria o scaffold; este
    # script valida o que ja existe). Sem fonte unica possivel entre as
    # duas linguagens: se a formula mudar, atualizar os dois lugares.
    return "x" + name.replace("-", "_")


def strip_meson_comments(text: str) -> str:
    """Remove comentario de linha ('#' ate o fim da linha -- meson usa a
    mesma sintaxe do shell/Python) de cada linha, sem tentar entender string
    literal (nenhum meson.build deste repositorio usa '#' dentro de uma
    string na mesma linha que importe aqui). Existe porque um comentario
    documentando a PROPRIA regra (ex.: '# NUNCA library()') e' texto real do
    arquivo e faria uma busca ingenua por substring achar uma violacao onde
    so ha a explicacao dela."""
    return "\n".join(line.split("#", 1)[0] for line in text.splitlines())


def real_namespace_opens(text: str):
    """As aberturas de namespace 'de verdade' (nao forward-declare de uma
    linha so, tipo 'namespace base { class Foo; }') na ordem em que aparecem
    no arquivo -- o que interessa e so' o INICIO da cadeia, ver check_namespace."""
    opens = []
    for line in text.splitlines():
        s = line.strip()
        if NS_OPEN_CLOSE_RE.match(s):
            continue
        m = NS_OPEN_ONLY_RE.match(s)
        if m:
            opens.append(m.group(1))
    return opens


def resolve(check_id, title, why, status, details, findings):
    exception_note = None
    if status == "FALHA" and check_id in KNOWN_EXCEPTIONS:
        status = "EXCECAO"
        exception_note = KNOWN_EXCEPTIONS[check_id]
    findings.append(Finding(check_id, title, why, status, details, exception_note))


# ------------------------------------------------------------------------------
# 1. domain/ e livre de MIXR e de BehaviorTree.CPP
# ------------------------------------------------------------------------------
def check_domain_pura(findings):
    check_id = "camadas-domain-pura"
    title = "domain/ nao inclui headers do MIXR nem do BehaviorTree.CPP"
    why = (
        "domain/ e a camada de regra pura -- testavel sem levantar Station "
        "nenhuma (docs/ARCHITECTURE.md, secao 'As camadas'). Um #include de "
        "mixr/ ou behaviortree_cpp_v3/ ali reintroduz, em silencio, a "
        "dependencia que a separacao em camadas existe para evitar."
    )
    files = source_files("include/domain", "src/domain")
    if not files:
        resolve(check_id, title, why, "PULADO", ["este modelo nao tem domain/"], findings)
        return
    bad = []
    for f in files:
        for i, line in enumerate(read(f).splitlines(), start=1):
            if MIXR_INCLUDE_RE.search(line) or BT_INCLUDE_RE.search(line):
                bad.append(f"{rel(f)}:{i}: {line.strip()}")
    if bad:
        resolve(check_id, title, why, "FALHA", bad, findings)
    else:
        resolve(check_id, title, why, "OK", [f"{len(files)} arquivo(s) verificado(s)"], findings)


# ------------------------------------------------------------------------------
# 2. bt/ nao inclui MIXR diretamente
# ------------------------------------------------------------------------------
def check_bt_sem_mixr(findings):
    check_id = "camadas-bt-sem-mixr"
    title = "bt/ nao inclui headers do MIXR diretamente"
    why = (
        "bt/ linka BehaviorTree.CPP + domain/, NUNCA o MIXR -- e o que "
        "permite tests/tree/ carregar a arvore de producao contra um "
        "contexto falso, sem Station (docs/ARCHITECTURE.md). Um no que "
        "precise de algo do player deve pedir pela interface "
        "bt/DecisionContext.hpp, nunca incluir mixr/ direto."
    )
    files = source_files("include/bt", "src/bt")
    if not files:
        resolve(check_id, title, why, "PULADO",
                ["este modelo nao tem bt/ (decisao por outro mecanismo)"], findings)
        return
    bad = []
    for f in files:
        for i, line in enumerate(read(f).splitlines(), start=1):
            if MIXR_INCLUDE_RE.search(line):
                bad.append(f"{rel(f)}:{i}: {line.strip()}")
    if bad:
        resolve(check_id, title, why, "FALHA", bad, findings)
    else:
        resolve(check_id, title, why, "OK", [f"{len(files)} arquivo(s) verificado(s)"], findings)


# ------------------------------------------------------------------------------
# 3. xnative/factory.cpp: if/else, NOMES[] e METAS[] sincronizados
# ------------------------------------------------------------------------------
IMPLEMENT_SUBCLASS_RE = re.compile(
    r'IMPLEMENT_(?:PARTIAL_|ABSTRACT_)?SUBCLASS\(\s*([\w:]+)\s*,\s*"([^"]+)"\s*\)'
)


def build_factory_name_map(dirs):
    """{nome-de-classe (ultimo segmento) -> string registrada em
    IMPLEMENT_SUBCLASS(Classe, "string")} -- a fonte de verdade de qual
    string getFactoryName() de fato devolve, que NAO precisa ser igual ao
    nome da classe C++ (ver o comentario de check_factory_sync)."""
    out = {}
    for d in dirs:
        if not d.is_dir():
            continue
        for p in sorted(d.rglob("*.cpp")):
            for m in IMPLEMENT_SUBCLASS_RE.finditer(read(p)):
                out[m.group(1).split("::")[-1]] = m.group(2)
    return out


def check_factory_sync(findings):
    check_id = "fabrica-tres-listas"
    title = "xnative/factory.cpp: if/else, NOMES[] e METAS[] descrevem o MESMO conjunto de classes"
    why = (
        "O proprio comentario do factory.cpp documenta isto: as tres listas "
        "'tem de acompanhar' umas as outras. O registro de plugin (PluginRegistry.cpp) "
        "so falha em runtime se um nome declarado em NOMES[] nao construir nada "
        "(factory(nome)==nullptr) -- este check confere a MESMA coisa em segundos, "
        "sem compilar nem levantar cenario nenhum: cada string de NOMES[] precisa "
        "vir de um IMPLEMENT_SUBCLASS(...) de alguma classe que o if/else de fato "
        "constroi, e METAS[] precisa apontar pra essas MESMAS classes, na mesma ordem "
        "do if/else -- NAO precisa ser o nome literal da classe C++ (um modelo pode, "
        "de proposito, registrar uma STRING diferente do nome da classe para evitar "
        "colisao de nome de fabrica com outro modelo que reusa o mesmo nome de classe)."
    )
    f = MODEL_ROOT / "src" / "xnative" / "factory.cpp"
    if not f.exists():
        resolve(check_id, title, why, "PULADO", ["nao ha src/xnative/factory.cpp"], findings)
        return

    text = read(f)
    chain = re.findall(r"name\s*==\s*([\w:]+)::getFactoryName\(\)", text)
    m_nomes = re.search(r"NOMES\[\]\s*=\s*\{(.*?)\}\s*;", text, re.S)
    nomes = re.findall(r'"([^"]+)"', m_nomes.group(1)) if m_nomes else []
    m_metas = re.search(r"METAS\[\]\s*=\s*\{(.*?)\}\s*;", text, re.S)
    metas = re.findall(r"([\w:]+)::getMetaObject\(\)", m_metas.group(1)) if m_metas else []

    if not chain or m_nomes is None or m_metas is None:
        resolve(check_id, title, why, "FALHA", [
            "padrao esperado nao encontrado (if/else + NOMES[] + METAS[]) -- "
            "ver models/template/src/xnative/factory.cpp para o modelo",
        ], findings)
        return

    problems = []
    if len(chain) != len(metas):
        problems.append(f"if/else registra {len(chain)} classe(s), METAS[] tem {len(metas)}")
    for i in range(max(len(chain), len(metas))):
        c = chain[i] if i < len(chain) else None
        me = metas[i] if i < len(metas) else None
        if c != me:
            problems.append(
                f"posicao {i}: if/else usa '{c}', METAS[] usa '{me}' -- "
                "deveriam ser a MESMA classe, na mesma ordem"
            )

    # NOMES[] e conferido como CONJUNTO contra a string de fabrica REAL de
    # cada classe do if/else (IMPLEMENT_SUBCLASS), nao contra o nome da
    # classe C++ -- e o que 'factory(nome)' de fato usa em runtime (a ordem
    # das strings dentro de NOMES[] nao importa pra isso, so' o conjunto).
    # models/events/ entra na varredura porque uma classe do modelo pode vir
    # de la (ex.: TacticalAlert em models/players/air/A-4) -- ver models/events/README.md.
    # models/ e' achado subindo a partir de MODEL_ROOT (_find_models_root),
    # nunca por um numero fixo de '.parent' -- MODEL_ROOT pode estar a
    # QUALQUER profundidade sob models/ (models/<nome>/, models/<categoria>/
    # <nome>/, models/<categoria>/<subcategoria>/<nome>/, ...), e um numero
    # fixo ja quebrou uma vez quando players/ ganhou subpastas proprias.
    scan_dirs = [MODEL_ROOT / "src"]
    models_root = _find_models_root(MODEL_ROOT)
    events_dir = (models_root / "events") if models_root is not None else None
    if events_dir is not None and events_dir.is_dir():
        scan_dirs.append(events_dir)
    impl_map = build_factory_name_map(scan_dirs)

    unresolved = []
    expected_nomes = set()
    for c in chain:
        leaf = c.split("::")[-1]
        if leaf in impl_map:
            expected_nomes.add(impl_map[leaf])
        else:
            unresolved.append(c)

    if unresolved:
        problems.append(
            "nao encontrei IMPLEMENT_SUBCLASS(...) para: " + ", ".join(sorted(set(unresolved))) +
            " -- nao da pra confirmar a string de fabrica real dessa(s) classe(s)"
        )
    else:
        nomes_set = set(nomes)
        so_em_nomes = sorted(nomes_set - expected_nomes)
        so_no_codigo = sorted(expected_nomes - nomes_set)
        if so_em_nomes:
            problems.append(
                "NOMES[] declara string(s) que NENHUMA classe do if/else registra: " +
                ", ".join(so_em_nomes)
            )
        if so_no_codigo:
            problems.append(
                "classe(s) do if/else cuja string de fabrica (IMPLEMENT_SUBCLASS) NAO "
                "esta em NOMES[]: " + ", ".join(so_no_codigo)
            )

    if problems:
        resolve(check_id, title, why, "FALHA", problems, findings)
    else:
        resolve(check_id, title, why, "OK",
                [f"{len(chain)} classe(s) registrada(s), se completam entre if/else, NOMES[] e METAS[]"], findings)


# ------------------------------------------------------------------------------
# 4. namespace aninhado sob mixr::models::x<nome>
# ------------------------------------------------------------------------------
def check_namespace(findings, name):
    check_id = "namespace-aninhado"
    title = "domain/bt/ubf/xnative aninham sob mixr::models::x<nome-do-modelo>"
    why = (
        "Um cenario PODE, em tese, carregar mais de um .so de modelo no "
        "mesmo processo. Sob RTLD_LOCAL, a comparacao de type_info degrada "
        "para strcmp do nome mangled -- dois tipos DIFERENTES com o MESMO "
        "nome qualificado, em dois .so distintos, colidiriam em silencio. "
        "docs/CONTRATO.md secao 6."
    )
    if name is None:
        resolve(check_id, title, why, "AVISO", ["nao consegui ler project() em meson.build"], findings)
        return

    expected = expected_namespace(name)
    files = source_files(
        "include/domain", "include/bt", "include/ubf", "include/xnative",
        "src/domain", "src/bt", "src/ubf", "src/xnative",
    )
    bad = []
    checked = 0
    for f in files:
        opens = real_namespace_opens(read(f))
        if not opens:
            continue
        checked += 1
        top3 = opens[:3]
        if top3 != ["mixr", "models", expected]:
            found = " -> ".join(top3) if top3 else "(nenhum)"
            bad.append(f"{rel(f)}: abre {found} (esperado mixr -> models -> {expected})")

    if bad:
        resolve(check_id, title, why, "FALHA", bad, findings)
    else:
        resolve(check_id, title, why, "OK",
                [f"{checked} arquivo(s) verificado(s), todos sob mixr::models::{expected}"], findings)


# ------------------------------------------------------------------------------
# 5. a obrigacao que falha em silencio: publicar no xboard
# ------------------------------------------------------------------------------
def check_xboard(findings):
    check_id = "xboard-obrigatorio"
    title = "a decisao e publicada no xboard (setBehaviorLabel + bumpDecisionCount)"
    why = (
        "Nada no compilador nem no carregador de plugin obriga isto -- um "
        "modelo que nunca chame essas funcoes compila, carrega, satisfaz "
        "provides:, e o core sobe e roda, so que a tela de status mostra "
        "bt=-- e dec=0 para sempre, sem nenhum erro em lugar nenhum. "
        "docs/CONTRATO.md secao 3."
    )
    has_ubf = (MODEL_ROOT / "src" / "ubf").is_dir() or (MODEL_ROOT / "include" / "ubf").is_dir()
    if not has_ubf:
        resolve(check_id, title, why, "PULADO",
                ["este modelo nao tem ubf/ (decisao por outro mecanismo, ex.: guiagem continua)"], findings)
        return

    text = "\n".join(read(f) for f in source_files("src"))
    missing = []
    if "xboard::setBehaviorLabel(" not in text:
        missing.append("xboard::setBehaviorLabel(...) nao encontrado em src/")
    if "xboard::bumpDecisionCount(" not in text:
        missing.append("xboard::bumpDecisionCount(...) nao encontrado em src/")
    if missing:
        resolve(check_id, title, why, "FALHA", missing, findings)
    else:
        resolve(check_id, title, why, "OK", ["as duas chamadas estao presentes em src/"], findings)


# ------------------------------------------------------------------------------
# 6. CHANGELOG.md declara a mesma versao do project() em meson.build
# ------------------------------------------------------------------------------
def check_changelog_version(findings):
    check_id = "changelog-versao"
    title = "CHANGELOG.md declara a MESMA versao do project() em meson.build"
    why = (
        "A versao de um modelo e a do project() -- nao ha tag de git, e o "
        "descritor do plugin nao carrega versao de modelo nenhuma "
        "(PluginDescV1 so tem plugin_name/mixr_pkg_version/build_id). Por "
        "convencao, CHANGELOG.md documenta isso com 'hoje `X.Y.Z`'; se o "
        "meson.build mudar de versao e o CHANGELOG nao acompanhar, o numero "
        "documentado fica errado em silencio."
    )
    changelog = MODEL_ROOT / "CHANGELOG.md"
    meson = MODEL_ROOT / "meson.build"
    if not changelog.is_file() or not meson.is_file():
        resolve(check_id, title, why, "PULADO",
                ["CHANGELOG.md ou meson.build ausente (ver o check 'cinco-pecas')"], findings)
        return

    pm = re.search(r"version:\s*'([0-9]+\.[0-9]+\.[0-9]+)'", read(meson))
    if not pm:
        resolve(check_id, title, why, "AVISO", ["nao encontrei version: 'X.Y.Z' em meson.build"], findings)
        return

    cm = re.search(r"hoje\s*`([0-9]+\.[0-9]+\.[0-9]+)`", read(changelog))
    if not cm:
        resolve(check_id, title, why, "AVISO", [
            "CHANGELOG.md nao declara a versao no formato 'hoje `X.Y.Z`' "
            f"(convencao dos outros modelos) -- meson.build esta em '{pm.group(1)}'",
        ], findings)
        return

    if cm.group(1) != pm.group(1):
        resolve(check_id, title, why, "FALHA", [
            f"CHANGELOG.md diz 'hoje `{cm.group(1)}`', mas meson.build declara version: '{pm.group(1)}'",
        ], findings)
    else:
        resolve(check_id, title, why, "OK", [f"versao '{pm.group(1)}' consistente nos dois arquivos"], findings)


# ------------------------------------------------------------------------------
# 7. as cinco pecas de todo projeto de modelo
# ------------------------------------------------------------------------------
def check_cinco_pecas(findings):
    check_id = "cinco-pecas"
    title = "as cinco pecas de todo projeto de modelo estao presentes"
    why = (
        "tests/guard/check_modelo_estrutura.sh ja cobra isto no NIVEL DO "
        "REPOSITORIO -- este check repete a mesma regra aqui, pra quem "
        "estiver rodando so este projeto, sem o resto do repositorio aberto."
    )
    pieces = (
        ("tests", "dir"),
        ("docs", "dir"),
        ("README.md", "file"),
        ("CHANGELOG.md", "file"),
        ("Makefile", "file"),
    )
    missing = []
    for name, kind in pieces:
        p = MODEL_ROOT / name
        ok = p.is_dir() if kind == "dir" else p.is_file()
        if not ok:
            missing.append(f"{name} ausente")
    if missing:
        resolve(check_id, title, why, "FALHA", missing, findings)
    else:
        resolve(check_id, title, why, "OK", ["as cinco pecas estao presentes"], findings)


# ------------------------------------------------------------------------------
# 8. src/plugin.cpp e so a fronteira C do plugin
# ------------------------------------------------------------------------------
PLUGIN_LINE_WARN = 120


def check_plugin_entrypoint(findings):
    check_id = "plugin-entrypoint"
    title = "src/plugin.cpp e so a fronteira C do plugin"
    why = (
        "docs/CONTRATO.md secao 1: exporte o ponto de entrada SO pela macro "
        "MIXR_PLUGIN_DEFINE, nunca escrevendo a assinatura extern \"C\" a "
        "mao -- num alvo com simbolos escondidos por padrao uma assinatura "
        "manual vira invisivel para quem carrega o .so, com o sintoma longe "
        "do erro. O arquivo tambem nao deveria ganhar logica de dominio -- "
        "'main.cpp nao implementa comportamento' vale igual aqui."
    )
    f = MODEL_ROOT / "src" / "plugin.cpp"
    if not f.is_file():
        resolve(check_id, title, why, "FALHA", ["src/plugin.cpp nao encontrado"], findings)
        return

    text = read(f)
    lines = text.splitlines()
    problems = []
    if "MIXR_PLUGIN_DEFINE(" not in text:
        problems.append("MIXR_PLUGIN_DEFINE(...) nao encontrado")
    if re.search(r'extern\s+"C"', text):
        problems.append('assinatura extern "C" escrita a mao -- deveria vir so da macro MIXR_PLUGIN_DEFINE')
    if problems:
        resolve(check_id, title, why, "FALHA", problems, findings)
        return

    if len(lines) > PLUGIN_LINE_WARN:
        resolve(check_id, title, why, "AVISO", [
            f"src/plugin.cpp tem {len(lines)} linhas (nos modelos deste "
            f"repositorio hoje ficam entre 40 e 55) -- confirme que nao "
            f"ganhou logica alem do registro do plugin",
        ], findings)
        return

    resolve(check_id, title, why, "OK", [
        f'{len(lines)} linha(s), usa MIXR_PLUGIN_DEFINE, sem extern "C" manual',
    ], findings)


# ------------------------------------------------------------------------------
# 9. cada camada com codigo tem uma suite de teste correspondente
# ------------------------------------------------------------------------------
def check_tests_por_camada(findings):
    check_id = "testes-por-camada"
    title = "domain/ e bt/ tem a suite de teste correspondente (tests/domain, tests/tree)"
    why = (
        "domain/ sem tests/domain/ deixa sem teste nenhum a regra mais "
        "barata de testar do modelo; bt/ sem tests/tree/ significa que a "
        "arvore de producao nunca e carregada contra um contexto falso. Ver "
        "a tabela de suites do CLAUDE.md raiz, secao 'Testes automatizados'."
    )
    has_domain = (MODEL_ROOT / "src" / "domain").is_dir()
    has_bt = (MODEL_ROOT / "src" / "bt").is_dir()
    if not has_domain and not has_bt:
        resolve(check_id, title, why, "PULADO", ["nenhuma camada domain/ ou bt/ neste modelo"], findings)
        return

    problems = []
    ok_notes = []
    if has_domain:
        d = MODEL_ROOT / "tests" / "domain"
        if d.is_dir() and any(d.glob("*.cpp")):
            ok_notes.append("tests/domain/ existe e tem teste(s)")
        else:
            problems.append("domain/ existe mas tests/domain/ esta ausente ou vazio")
    if has_bt:
        d = MODEL_ROOT / "tests" / "tree"
        if d.is_dir() and any(d.glob("*.cpp")):
            ok_notes.append("tests/tree/ existe e tem teste(s)")
        else:
            problems.append("bt/ existe mas tests/tree/ esta ausente ou vazio")

    if problems:
        resolve(check_id, title, why, "FALHA", problems, findings)
    else:
        resolve(check_id, title, why, "OK", ok_notes, findings)


# ------------------------------------------------------------------------------
# 10. todo .cpp de src/ esta referenciado em algum meson.build
# ------------------------------------------------------------------------------
def check_arquivos_no_build(findings):
    check_id = "arquivos-registrados-no-build"
    title = "todo .cpp de src/ esta referenciado em algum meson.build deste projeto"
    why = (
        "Um arquivo em src/ que nao aparece em nenhum files(...) de "
        "meson.build nao compila -- nem em producao, nem em teste. E facil "
        "de esquecer ao criar um .cpp novo copiando o padrao de um vizinho "
        "sem lembrar do passo final (docs/ARCHITECTURE.md, 'Quando isto nao "
        "bastar mais')."
    )
    src_dir = MODEL_ROOT / "src"
    if not src_dir.is_dir():
        resolve(check_id, title, why, "PULADO", ["nao ha src/"], findings)
        return

    haystack = read(MODEL_ROOT / "meson.build") + "\n" + read(MODEL_ROOT / "tools" / "meson.build")
    missing = []
    checked = 0
    for f in sorted(src_dir.rglob("*.cpp")):
        checked += 1
        if rel(f) not in haystack:
            missing.append(rel(f))

    if missing:
        resolve(check_id, title, why, "FALHA",
                [f"nao referenciado em nenhum meson.build: {m}" for m in missing], findings)
    else:
        resolve(check_id, title, why, "OK", [f"{checked} arquivo(s) .cpp, todos referenciados"], findings)


# ------------------------------------------------------------------------------
# 11. o mesmo nome em project(), shared_module() e MIXR_PLUGIN_DEFINE
# ------------------------------------------------------------------------------
def check_nome_plugin(findings):
    check_id = "nome-plugin-consistente"
    title = "o mesmo nome em project(), shared_module() e MIXR_PLUGIN_DEFINE"
    why = (
        "src/plugin.cpp ja documenta isto no proprio comentario: o primeiro "
        "argumento de MIXR_PLUGIN_DEFINE vai para o descritor binario e "
        "aparece em mensagens de erro do core quando algo da errado no "
        "carregamento -- 'combine com o nome do project() em meson.build e "
        "do shared_module()' (docs/PRIMEIROS-PASSOS.md, passo 1)."
    )
    meson_text = read(MODEL_ROOT / "meson.build")
    plugin_text = read(MODEL_ROOT / "src" / "plugin.cpp")
    pm = re.search(r"^project\(\s*'([^']+)'", meson_text, re.M)
    sm = re.search(r"shared_module\(\s*'([^']+)'", meson_text)
    dm = re.search(r'MIXR_PLUGIN_DEFINE\(\s*"([^"]+)"', plugin_text)

    if not (pm and sm and dm):
        resolve(check_id, title, why, "AVISO", [
            f"nao consegui achar os tres nomes (project()={'sim' if pm else 'nao'}, "
            f"shared_module()={'sim' if sm else 'nao'}, MIXR_PLUGIN_DEFINE={'sim' if dm else 'nao'})",
        ], findings)
        return

    names = {"project()": pm.group(1), "shared_module()": sm.group(1), "MIXR_PLUGIN_DEFINE": dm.group(1)}
    if len(set(names.values())) > 1:
        resolve(check_id, title, why, "FALHA", [f"{k} = '{v}'" for k, v in names.items()], findings)
    else:
        resolve(check_id, title, why, "OK", [f"os tres usam '{pm.group(1)}'"], findings)


# ------------------------------------------------------------------------------
# 12. todo header tem guarda de inclusao
# ------------------------------------------------------------------------------
PRAGMA_ONCE_RE = re.compile(r"^\s*#\s*pragma\s+once\b", re.M)
IFNDEF_RE = re.compile(r"^\s*#\s*ifndef\s+(\w+)", re.M)
DEFINE_RE = re.compile(r"^\s*#\s*define\s+(\w+)", re.M)


def check_include_guards(findings):
    check_id = "guarda-de-inclusao"
    title = "todo header tem guarda de inclusao (#pragma once OU #ifndef/#define)"
    why = (
        "Um header incluido duas vezes na mesma unidade de traducao (direto "
        "ou por uma cadeia de #include) sem guarda nenhuma e erro de "
        "redefinicao -- ou pior, comportamento indefinido silencioso se as "
        "duas inclusoes acontecerem em contextos diferentes. As DUAS formas "
        "convivem hoje neste repositorio (A-4 usa #ifndef em ubf/xnative, "
        "#pragma once no resto); qualquer uma resolve o risco -- o que "
        "importa e nunca faltar."
    )
    files = source_files("include/domain", "include/bt", "include/ubf", "include/xnative")
    if not files:
        resolve(check_id, title, why, "PULADO", ["nao ha header nenhum sob include/"], findings)
        return

    missing = []
    estilos = {"pragma once": [], "ifndef/define": []}
    for f in files:
        head = "\n".join(read(f).splitlines()[:15])
        if PRAGMA_ONCE_RE.search(head):
            estilos["pragma once"].append(rel(f))
        elif IFNDEF_RE.search(head) and DEFINE_RE.search(head):
            estilos["ifndef/define"].append(rel(f))
        else:
            missing.append(rel(f))

    if missing:
        resolve(check_id, title, why, "FALHA",
                [f"sem guarda de inclusao nas primeiras 15 linhas: {m}" for m in missing], findings)
        return

    if estilos["pragma once"] and estilos["ifndef/define"]:
        resolve(check_id, title, why, "AVISO", [
            f"estilo misturado: {len(estilos['pragma once'])} arquivo(s) usam 'pragma once', "
            f"{len(estilos['ifndef/define'])} usam '#ifndef/#define' -- nao e erro, so inconsistencia",
        ] + estilos["ifndef/define"][:3], findings)
        return

    estilo_usado = "pragma once" if estilos["pragma once"] else "#ifndef/#define"
    resolve(check_id, title, why, "OK",
            [f"{len(files)} arquivo(s), todos com guarda ({estilo_usado})"], findings)


# ------------------------------------------------------------------------------
# 13. toda arvore de comportamento em configs/ e XML bem formado
# ------------------------------------------------------------------------------
def check_xml_trees(findings):
    check_id = "arvores-xml-validas"
    title = "todo .xml de configs/ e XML bem formado"
    why = (
        "tinyxml2 (o parser que o BehaviorTree.CPP usa em runtime) tolera "
        "violacoes que quebram um parser XML estrito -- em especial, hifen "
        "duplo '--' dentro de um comentario, que a propria especificacao XML "
        "proibe. O Groot usa QDomDocument (estrito) e recusa o ARQUIVO "
        "INTEIRO com um erro generico de sintaxe, sem apontar a causa -- "
        "CLAUDE.md raiz, secao 'Groot', armadilha no 1. Este check roda o "
        "MESMO parser estrito (xml.etree, da biblioteca padrao do Python) "
        "antes de alguem descobrir isso tentando abrir a arvore no Groot."
    )
    configs = MODEL_ROOT / "configs"
    xmls = sorted(configs.glob("*.xml")) if configs.is_dir() else []
    if not xmls:
        resolve(check_id, title, why, "PULADO", ["nenhum .xml em configs/"], findings)
        return

    bad = []
    for f in xmls:
        try:
            ET.parse(f)
        except ET.ParseError as e:
            bad.append(f"{rel(f)}: {e}")
    if bad:
        resolve(check_id, title, why, "FALHA", bad, findings)
    else:
        resolve(check_id, title, why, "OK", [f"{len(xmls)} arquivo(s) .xml, todos bem formados"], findings)


# ------------------------------------------------------------------------------
# 14. empacotado como plugin, nao como biblioteca comum (CONTRATO.md secao 1)
# ------------------------------------------------------------------------------
def check_plugin_packaging(findings):
    check_id = "empacotamento-plugin"
    title = "empacotado como plugin (shared_module, sem simbolo vazando)"
    why = (
        "docs/CONTRATO.md secao 1: shared_module() (nunca library() -- um "
        "artefato linkavel poderia acabar num link_with: de outra coisa, "
        "duplicando estado interno do MIXR no processo), "
        "gnu_symbol_visibility:'hidden' (esconde tudo por padrao) e "
        "-Wl,--no-undefined (transforma 'chamei algo que o core nao exporta' "
        "em erro de LINK, nao de dlopen em runtime). Se o modelo linkar "
        "alguma biblioteca ESTATICA (aqui, so BehaviorTree.CPP, quando ha "
        "bt/), tambem -Wl,--exclude-libs,ALL -- senao os simbolos dela vazam "
        "para fora do .so."
    )
    meson = MODEL_ROOT / "meson.build"
    if not meson.is_file():
        resolve(check_id, title, why, "PULADO", ["nao ha meson.build"], findings)
        return

    text = strip_meson_comments(read(meson))
    problems = []

    if not re.search(r"\bshared_module\s*\(", text):
        problems.append("nenhum shared_module() encontrado -- o artefato de producao precisa ser um plugin, nunca library()/shared_library() sozinho")

    bare_library = re.search(r"(?<![a-zA-Z_])library\s*\(", text)
    if bare_library:
        problems.append("chamada a library() encontrada -- deveria ser shared_module() (ver o comentario padrao: 'NUNCA library()')")

    if not re.search(r"gnu_symbol_visibility\s*:\s*'hidden'", text):
        problems.append("gnu_symbol_visibility:'hidden' nao encontrado")

    if "-Wl,--no-undefined" not in text:
        problems.append("-Wl,--no-undefined nao encontrado nos link_args")

    linka_estatica = "behavior_tree_dep" in text
    if linka_estatica and "-Wl,--exclude-libs,ALL" not in text:
        problems.append(
            "modelo linka behavior_tree_dep (BehaviorTree.CPP, biblioteca ESTATICA) mas "
            "-Wl,--exclude-libs,ALL nao aparece nos link_args -- os simbolos dela vazariam do .so"
        )

    if problems:
        resolve(check_id, title, why, "FALHA", problems, findings)
    else:
        nota = "com -Wl,--exclude-libs,ALL (linka biblioteca estatica)" if linka_estatica else "sem biblioteca estatica linkada"
        resolve(check_id, title, why, "OK", [f"shared_module() + hidden + no-undefined, {nota}"], findings)


# ------------------------------------------------------------------------------
# 15. dado proprio do modelo (arvore, aeronave) e publicado junto do .so
# ------------------------------------------------------------------------------
def check_data_published(findings):
    check_id = "dados-publicados"
    title = "dado proprio (configs/, data/) e publicado via install_data/install_subdir"
    why = (
        "docs/CONTRATO.md secao 4: 'o .so sozinho pode nao ser a entrega "
        "completa' -- um arquivo que o modelo referencia por conta propria "
        "(uma arvore XML, uma aeronave JSBSim) e dado DO MODELO, nao do "
        "cenario que o carrega, e precisa ser instalado junto (install_data()/ "
        "install_subdir() no meson.build) para o cenario achar o caminho que "
        "o slot (ex.: treeFile:) aponta depois de instalado."
    )
    meson = MODEL_ROOT / "meson.build"
    dirs_com_dado = [d for d in ("configs", "data") if (MODEL_ROOT / d).is_dir() and any((MODEL_ROOT / d).iterdir())]
    if not dirs_com_dado:
        resolve(check_id, title, why, "PULADO", ["nao ha configs/ nem data/ com conteudo"], findings)
        return
    if not meson.is_file():
        resolve(check_id, title, why, "FALHA", ["ha dado em " + "/".join(dirs_com_dado) + " mas nao ha meson.build"], findings)
        return

    text = strip_meson_comments(read(meson))
    faltando = []
    for d in dirs_com_dado:
        if not re.search(rf"install_(data|subdir)\(\s*'{d}/", text):
            faltando.append(f"{d}/ tem conteudo mas nenhum install_data()/install_subdir() referencia '{d}/...' em meson.build")

    if faltando:
        resolve(check_id, title, why, "FALHA", faltando, findings)
    else:
        resolve(check_id, title, why, "OK", [f"{'/'.join(dirs_com_dado)} publicado(s) via install_data/install_subdir"], findings)


ALL_CHECKS = (
    check_domain_pura,
    check_bt_sem_mixr,
    check_factory_sync,
    None,  # check_namespace precisa do nome do projeto -- ligado a parte, ver run_checks()
    check_xboard,
    check_changelog_version,
    check_cinco_pecas,
    check_plugin_entrypoint,
    check_tests_por_camada,
    check_arquivos_no_build,
    check_nome_plugin,
    check_include_guards,
    check_xml_trees,
    check_plugin_packaging,
    check_data_published,
)

CHECK_ID_WIDTH = 30  # a mais comprida hoje e' 'arquivos-registrados-no-build' (30)

# --------------------------------------------------------------------------
# Saida estilo gtest: colorida, compacta por padrao (uma linha por
# verificacao + o bloco de detalhe so' para quem nao passou), com recapitulacao
# no fim -- ver o cabecalho do arquivo para o "porque" do MAX_DETAILS.
# --------------------------------------------------------------------------
class Ansi:
    RESET = "\x1b[0m"
    BOLD = "\x1b[1m"
    RED = "\x1b[31m"
    GREEN = "\x1b[32m"
    YELLOW = "\x1b[33m"
    CYAN = "\x1b[36m"
    GRAY = "\x1b[90m"


STATUS_STYLE = {
    "OK": (Ansi.GREEN, "OK"),
    "FALHA": (Ansi.RED, "FALHA"),
    "AVISO": (Ansi.YELLOW, "AVISO"),
    "EXCECAO": (Ansi.YELLOW, "EXCECAO"),
    "PULADO": (Ansi.GRAY, "PULADO"),
}


def want_color(args) -> bool:
    if args.no_color:
        return False
    if args.color:
        return True
    if os.environ.get("NO_COLOR") is not None:
        return False
    return sys.stdout.isatty()


def make_colorizer(enabled):
    if not enabled:
        return lambda text, *codes: text
    return lambda text, *codes: "".join(codes) + text + Ansi.RESET


def compact_note(f: Finding) -> str:
    if f.status in ("OK", "PULADO"):
        return "; ".join(f.details)
    n = len(f.details)
    if n == 0:
        return "excecao conhecida" if f.status == "EXCECAO" else ""
    if n == 1 and len(f.details[0]) <= 70:
        return f.details[0]
    sufixo = " -- excecao conhecida" if f.status == "EXCECAO" else ""
    return f"{n} item(ns){sufixo}, ver detalhe abaixo"


def render_compact(f: Finding, colorize) -> str:
    color, label = STATUS_STYLE[f.status]
    bracket = f"[{colorize(label.rjust(7), Ansi.BOLD, color)}]"
    note = compact_note(f)
    line = f"{bracket} {f.check_id:<{CHECK_ID_WIDTH}}"
    if note:
        line += f"  {note}"
    return line


def render_detail(f: Finding, colorize, verbose: bool) -> str:
    out = []
    out.append(textwrap.fill(f.why, width=96, initial_indent=" " * 11, subsequent_indent=" " * 11))
    if f.exception_note:
        rotulo = colorize("excecao conhecida:", Ansi.BOLD, Ansi.YELLOW)
        texto = textwrap.fill(
            f.exception_note, width=96,
            initial_indent=" " * 11, subsequent_indent=" " * 11,
        ).lstrip()
        out.append(f"{' ' * 11}{rotulo} {texto}")
    shown = f.details if verbose else f.details[:MAX_DETAILS]
    escondidos = len(f.details) - len(shown)
    for d in shown:
        out.append(f"{' ' * 13}- {d}")
    if escondidos > 0:
        out.append(f"{' ' * 13}{colorize(f'... e mais {escondidos} (rode com -v/--verbose para ver todos)', Ansi.GRAY)}")
    return "\n".join(out)


def run_checks(name) -> list:
    findings: list = []
    for chk in ALL_CHECKS:
        if chk is None:
            check_namespace(findings, name)
        else:
            chk(findings)
    return findings


def parse_args(argv):
    p = argparse.ArgumentParser(
        prog="check_organization.py",
        description="Linter OPCIONAL de boas praticas de organizacao interna do modelo.",
    )
    p.add_argument("-v", "--verbose", action="store_true",
                    help="mostra todos os itens de cada verificacao, sem cortar em %d." % MAX_DETAILS)
    g = p.add_mutually_exclusive_group()
    g.add_argument("--color", action="store_true", help="forca cores mesmo fora de um terminal.")
    g.add_argument("--no-color", action="store_true", help="desliga cores mesmo dentro de um terminal.")
    return p.parse_args(argv)


def main(argv=None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    colorize = make_colorizer(want_color(args))

    name = project_name()
    findings = run_checks(name)

    titulo = name or MODEL_ROOT.name
    excecoes = sorted(KNOWN_EXCEPTIONS.keys())

    print(colorize(f"== check_organization.py -- {titulo} ==", Ansi.BOLD))
    print(colorize("   linter opcional -- nao roda em CI, nao bloqueia build/test/install", Ansi.GRAY))
    nota_excecoes = ", ".join(excecoes) if excecoes else "nenhuma"
    print(colorize(f"   excecoes conhecidas configuradas: {nota_excecoes}", Ansi.GRAY))
    print()

    for f in findings:
        print(render_compact(f, colorize))
        if f.status in ("FALHA", "AVISO", "EXCECAO"):
            print(render_detail(f, colorize, args.verbose))
            print()

    counts = {}
    for f in findings:
        counts[f.status] = counts.get(f.status, 0) + 1
    ordem = ("OK", "FALHA", "AVISO", "EXCECAO", "PULADO")
    partes = [colorize(f"{counts[s]} {s.lower()}", STATUS_STYLE[s][0], Ansi.BOLD) for s in ordem if counts.get(s)]
    print(colorize(f"[{'=' * 10}]", Ansi.CYAN) + f" {len(findings)} verificacao(oes) -- " + ", ".join(partes))

    n_fail = counts.get("FALHA", 0)
    if n_fail:
        falhas = [f.check_id for f in findings if f.status == "FALHA"]
        print()
        print(colorize("[  FALHOU  ]", Ansi.BOLD, Ansi.RED) +
              f" {n_fail} verificacao(oes) precisa(m) de atencao:")
        for check_id in falhas:
            print(f"             - {check_id}")
        print()
        print("Isto e um linter, nao um teste de comportamento -- 'make test' continua sendo a autoridade sobre correcao.")
        return 1

    print()
    print(colorize("Nenhuma falha.", Ansi.GREEN, Ansi.BOLD) +
          " Isto NAO substitui 'make test' (que verifica comportamento, nao organizacao).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
