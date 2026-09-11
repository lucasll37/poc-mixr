#!/usr/bin/env python3
"""Lint LEVE de um arquivo .edl/.edl.in, contra o catalogo de
generate_edl_catalog.py (src/ui/edl_catalog.generated.json).

Não é um parser fiel à gramática bison/flex real (edl_parser.y) -- é um
varredor estrutural simples (conta parênteses/chaves, ignora comentário/
string) que sabe o suficiente para checar: fábrica desconhecida, slot
desconhecido para a classe, ASCII-only no arquivo inteiro, `plugins:` fora
de ordem, e referência-por-nome pendurada (best-effort, aviso -- não
bloqueia, porque a própria gramática não tem noção de escopo/referência
para isso ser 100% decidível estaticamente).

A verificação PROFUNDA e definitiva é o parser C++ de verdade, via o
binário `edlcheck` (app/src/edlcheck_main.cpp, ex.: `dist/bin/edlcheck
<arquivo>` ou `$(BUILD_DIR)/app/src/edlcheck <arquivo>`) -- este script
existe para pegar o erro mais comum ANTES de precisar compilar/instalar
nada, e para poder rodar como regressão contra os cenários reais do
repositório (nenhum deveria acusar erro aqui).

Também é usado internamente por src/ui/scripts/build.js, como self-check do
round-trip do cenário padrão do editor gráfico de .edl.

Uso:
    python3 src/ui/scripts/edl_lint.py <arquivo.edl>
    python3 src/ui/scripts/edl_lint.py --json <arquivo.edl>
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))

import mixr_source_scan as scan  # noqa: E402

CATALOG_PATH = REPO_ROOT / "src" / "ui" / "edl_catalog.generated.json"

# Mesma mecanica de app/src/app/ScenarioTemplate.cpp (expandIncludes() +
# substituicao literal de token), so que aqui e so para o LINT nao se
# confundir com sintaxe de template ('@include:@'/'@NUM_TC_THREADS@') que
# nao existe na gramatica EDL de verdade -- o '@' esta no charset de
# identificador do scanner (edl_scanner.l:48), entao '@include:nome@' sem
# expandir seria lido como um slot chamado '@include' seguido de valor
# '@nome'. So um nivel (igual ao original -- "deliberadamente RASO"), e o
# valor de qualquer '@TOKEN@' remanescente vira um numero neutro: o lint
# nao precisa saber o valor de verdade, so nao pode tropecar na sintaxe.
# Efeito colateral aceito: numero de linha nos erros pode nao bater mais
# com o arquivo ORIGINAL depois da expansao de um include.
INCLUDE_RE = re.compile(r"@include:([^@]+)@")
TOKEN_RE = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)@")
FRAGMENTS_DIR = REPO_ROOT / "app" / "configs" / "fragments"


def expand_templates(text):
    def repl_include(m):
        frag = FRAGMENTS_DIR / m.group(1)
        return frag.read_text(encoding="utf-8", errors="replace") if frag.exists() else ""

    text = INCLUDE_RE.sub(repl_include, text)
    text = TOKEN_RE.sub("1", text)
    return text

FORM_RE = re.compile(r"\(\s*([A-Za-z_]\w*)")
# Mesmo charset de identificador do scanner real (edl_scanner.l:48), seguido
# de ':' -- a mesma regra que SLOT_ID usa para reconhecer um nome de slot.
SLOT_KEY_RE = re.compile(r"([a-zA-Z0-9~!@#$%^&*\-_+=<>?/]+)\s*:")

# Slots "referência por nome" conhecidos (mesma curadoria do catálogo,
# src/ui/edl_catalog_overrides.json) -- usado aqui só para o aviso de
# referência pendurada, best-effort.
def load_reference_slots():
    path = REPO_ROOT / "src" / "ui" / "edl_catalog_overrides.json"
    if not path.exists():
        return set()
    return set(json.loads(path.read_text(encoding="utf-8")).get("referenceSlots", {}))


REFERENCE_SLOTS = load_reference_slots()
REFERENCE_VALUE_RE = re.compile(r"([a-zA-Z0-9~!@#$%^&*\-_+=<>?/]+)")


def load_catalog():
    if not CATALOG_PATH.exists():
        raise SystemExit(
            f"catalogo nao encontrado em {CATALOG_PATH} -- rode 'make open-edl' "
            f"(ou 'node src/ui/scripts/build.js') primeiro."
        )
    entries = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    return {e["factory"]: e for e in entries}


def find_matching(text, open_idx, open_ch, close_ch):
    depth = 0
    i, n = open_idx, len(text)
    while i < n:
        c = text[i]
        if c == open_ch:
            depth += 1
        elif c == close_ch:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


class LintResult:
    def __init__(self):
        self.errors = []
        self.warnings = []


def parse_form(text, start, catalog, result, path):
    m = FORM_RE.match(text, start)
    if not m:
        result.errors.append(f"{path}: '(' sem nome de classe logo depois")
        return start + 1
    class_name = m.group(1)
    end = find_matching(text, start, "(", ")")
    if end == -1:
        result.errors.append(f"{path}/{class_name}: parêntese '(' sem fechamento correspondente")
        return len(text)

    entry = catalog.get(class_name)
    if entry is None:
        result.errors.append(f"{path}: fábrica desconhecida '{class_name}'")
        slot_names = None
    else:
        slot_names = {s["name"] for s in entry["slots"]}

    here = f"{path}/{class_name}"
    i = m.end()
    while i < end:
        c = text[i]
        if c == "(":
            sub_end = find_matching(text, i, "(", ")")
            parse_form(text, i, catalog, result, here)
            i = sub_end + 1 if sub_end != -1 else end
            continue
        if c == "{":
            sub_end = find_matching(text, i, "{", "}")
            if sub_end == -1:
                result.errors.append(f"{here}: chave '{{' sem fechamento correspondente")
                i = end
                continue
            parse_list(text, i, sub_end, catalog, result, here)
            i = sub_end + 1
            continue
        km = SLOT_KEY_RE.match(text, i)
        if km:
            slot_name = km.group(1)
            if slot_names is not None and slot_name not in slot_names:
                result.errors.append(f"{here}: slot desconhecido '{slot_name}' (classe '{class_name}' não declara esse slot, nem herda)")
            i = km.end()
            continue
        i += 1
    return end + 1


def parse_list(text, start, end, catalog, result, path):
    i = start + 1
    while i < end:
        c = text[i]
        if c == "(":
            sub_end = find_matching(text, i, "(", ")")
            parse_form(text, i, catalog, result, path)
            i = sub_end + 1 if sub_end != -1 else end
            continue
        if c == "{":
            sub_end = find_matching(text, i, "{", "}")
            if sub_end == -1:
                result.errors.append(f"{path}: chave '{{' sem fechamento correspondente (dentro de lista)")
                i = end
                continue
            parse_list(text, i, sub_end, catalog, result, path)
            i = sub_end + 1
            continue
        i += 1
    return end + 1


def check_ascii(text, result):
    for line_no, line in enumerate(text.split("\n"), start=1):
        for ch in line:
            if ord(ch) > 126:
                result.errors.append(
                    f"linha {line_no}: caractere fora de ASCII ({ch!r}) -- um único assim, em "
                    f"QUALQUER lugar do arquivo (mesmo dentro de comentário), faz o parser real "
                    f"rejeitar o arquivo INTEIRO com 'syntax error', sem dizer o motivo"
                )
                break


def check_plugins_first(text, result):
    """`plugins:` (que faz dlopen e registra novas factories) tem que ser o
    PRIMEIRO item de components: -- o parser resolve em ordem textual
    (recursao a esquerda), entao um nome de fabrica de plugin usado depois
    no mesmo arquivo so existe se o PluginLoader ja rodou. Best-effort:
    so avisa se achar 'components:' com 'plugins:' presente mas nao
    primeiro -- nao entende aninhamento profundo o suficiente para travar
    isso com certeza."""
    masked = scan.mask_source(text)
    m = re.search(r"\bcomponents\s*:\s*\{", masked)
    if not m:
        return
    end = find_matching(masked, m.end() - 1, "{", "}")
    if end == -1:
        return
    body = masked[m.end():end]
    if "plugins" not in body:
        return
    first_key = re.search(r"([a-zA-Z0-9~!@#$%^&*\-_+=<>?/]+)\s*:", body)
    if first_key and first_key.group(1) != "plugins":
        result.warnings.append(
            "components: tem 'plugins:' mas ele não é o primeiro item -- o PluginLoader "
            "precisa carregar antes de qualquer nome de fábrica de plugin ser usado no "
            "mesmo arquivo (o parser resolve em ordem textual)."
        )


def check_dangling_references(masked, result):
    """Aviso BEST-EFFORT (não bloqueia): o valor de um slot 'referência por
    nome' (antennaName, trackManagerName, ...) tem que bater com o nome de
    ALGUM objeto declarado em algum lugar do arquivo -- 'nome: ( Classe ... )'.
    Isto NÃO é resolução de escopo de verdade (um nome de outro player
    passaria despercebido igual, e a própria gramática não tem noção de
    referência para isso ser decidível estaticamente); é só para pegar o
    erro de digitação óbvio antes de rodar a simulação."""
    if not REFERENCE_SLOTS:
        return
    declared = set(re.findall(r"([a-zA-Z0-9~!@#$%^&*\-_+=<>?/]+)\s*:\s*\(", masked))
    ref_pattern = re.compile(
        r"\b(" + "|".join(re.escape(s) for s in REFERENCE_SLOTS) + r")\s*:\s*"
        r"([a-zA-Z0-9~!@#$%^&*\-_+=<>?/]+)"
    )
    for m in ref_pattern.finditer(masked):
        slot_name, value = m.group(1), m.group(2)
        if value not in declared:
            result.warnings.append(
                f"{slot_name}: {value} -- nenhum objeto chamado '{value}' foi encontrado "
                f"declarado neste arquivo (referência por nome pendurada?)"
            )


def mask_string_contents(masked):
    """Apaga tambem o CONTEUDO de literais "entre aspas" (preservando
    tamanho/quebra de linha, mesma tecnica de mask_source), sem apagar as
    aspas em si. mask_source() por si so preserva string INTACTA de
    proposito (IMPLEMENT_RE precisa ler nome de fabrica de dentro dela) --
    mas isso faz um valor como '"...(RCS)..."' (uma descricao de texto
    livre, sem relacao nenhuma com sintaxe) confundir o varredor estrutural
    com um '(' de verdade. Confirmado rodando: scenario_max_player.edl.in
    tem exatamente essa string, e sem esta mascara o lint acusava
    'fabrica desconhecida RCS'."""
    out = list(masked)
    in_string = False
    for i, c in enumerate(masked):
        if in_string:
            if c == '"':
                in_string = False
            elif c != "\n":
                out[i] = " "
            continue
        if c == '"':
            in_string = True
    return "".join(out)


def lint_text(text, catalog):
    result = LintResult()
    check_ascii(text, result)
    check_plugins_first(text, result)
    masked = scan.mask_source(text)
    check_dangling_references(masked, result)
    structural = mask_string_contents(masked)
    i, n = 0, len(structural)
    while i < n:
        if structural[i] == "(":
            end = find_matching(structural, i, "(", ")")
            parse_form(structural, i, catalog, result, "")
            i = end + 1 if end != -1 else n
            continue
        i += 1
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("arquivo", type=Path)
    ap.add_argument("--json", action="store_true", help="saida em JSON em vez de texto")
    args = ap.parse_args()

    if not args.arquivo.exists():
        raise SystemExit(f"arquivo nao encontrado: {args.arquivo}")

    catalog = load_catalog()
    text = args.arquivo.read_text(encoding="utf-8", errors="replace")
    if "@" in text:
        text = expand_templates(text)
    result = lint_text(text, catalog)

    if args.json:
        print(json.dumps({"errors": result.errors, "warnings": result.warnings}, indent=2, ensure_ascii=False))
    else:
        for e in result.errors:
            print(f"ERRO: {e}")
        for w in result.warnings:
            print(f"AVISO: {w}")
        if not result.errors and not result.warnings:
            print(f"OK -- {args.arquivo}: nenhum problema encontrado (lint leve; rode o binario "
                  f"'edlcheck' para o parser real).")

    return 1 if result.errors else 0


if __name__ == "__main__":
    sys.exit(main())
