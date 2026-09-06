#!/usr/bin/env python3
"""Gera um modelo novo em models/player/<nome>/ a partir de um ponto de
partida copiavel (fixtures/stub, achatado, ou template/, em camadas).

Automatiza a receita MECANICA ja documentada em models/README.md secao 2 e
em models/player/template/docs/PRIMEIROS-PASSOS.md -- nao inventa passo
novo, so elimina os erros manuais mais citados no repositorio: a linha ROOT
do Makefile (calculada aqui pela PROFUNDIDADE REAL do destino, nunca
copiada -- ver models/README.md secao 5, armadilha 7) e o namespace C++
aninhado (models/player/fixtures/stub/docs/CONTRATO.md secao 6) esquecido pela
metade.

O QUE ESTE SCRIPT NAO FAZ, de proposito:
  - nao escreve a logica de dominio (a razao do modelo existir);
  - nao decide se o modelo entra no build orquestrado da raiz
    (models/README.md secao 2, passo final / docs/PRIMEIROS-PASSOS.md
    passo 7);
  - nao registra nada em .github/CODEOWNERS;
  - nao faz commit nenhum.
Tudo isso fica no checklist impresso ao final.

Uso:
    python3 scripts/new_model.py --name meu_modelo --kind stub
    python3 scripts/new_model.py --name meu_modelo --kind template

Pre-requisito (uma vez por maquina, igual a qualquer modelo deste
repositorio): `make configure && make sdk` na raiz.
"""
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from datetime import date
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
ORIGENS = {
    "stub": REPO_ROOT / "models" / "player" / "fixtures" / "stub",
    "template": REPO_ROOT / "models" / "player" / "template",
}
NOME_RE = re.compile(r"^[a-z][a-z0-9_]*$")
IGNORAR = shutil.ignore_patterns(
    "build", "dist", "__pycache__", ".configure-args", "*.o", "*.so"
)

NOVO_CHANGELOG = """# Changelog — `{name}`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em [`../../README.md`](../../README.md); a guarda
[`tests/guard/check_modelo_estrutura.sh`](../../../tests/guard/check_modelo_estrutura.sh) a
trava (ela descobre projetos por `find`, então este diretório já nasce coberto).

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [`meson.build`](meson.build)** — hoje `{version}`. Não existe
outra: não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`../../../shared/xplugin/PluginAbi.hpp`](../../../shared/xplugin/PluginAbi.hpp)).

**As datas saem da data de COMMIT, nunca da mensagem** — ver
[`CONTRIBUTING.md`](../../../CONTRIBUTING.md), na raiz do repositório, para a convenção de
mensagem de commit em uso.

---

## [{version}] — {date}

Gerado a partir de `models/player/{origin_rel}` por `scripts/new_model.py` (kind={kind}).
Substitua esta entrada pela primeira decisão real deste modelo antes do primeiro commit.
"""


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--name", required=True, help="nome do modelo novo (minusculo, snake_case)")
    p.add_argument("--kind", choices=sorted(ORIGENS), default="stub")
    p.add_argument("--dest", default=None, help="default: models/player/<name>")
    p.add_argument(
        "--no-build",
        action="store_true",
        help="pula o 'make build/test' de verificacao ao final (nao recomendado)",
    )
    return p.parse_args()


def substituir(caminho: Path, de: str, para: str, obrigatorio: bool = True) -> None:
    texto = caminho.read_text(encoding="utf-8")
    novo = texto.replace(de, para)
    if novo == texto and obrigatorio:
        print(f"  aviso: '{de}' nao encontrado em {caminho.relative_to(REPO_ROOT)}", file=sys.stderr)
    caminho.write_text(novo, encoding="utf-8")


def arquivos_contendo(raiz: Path, token: str, subpastas: list[str]) -> list[Path]:
    achados = []
    for sub in subpastas:
        base = raiz / sub
        if not base.exists():
            continue
        for f in base.rglob("*"):
            if f.is_file() and token in f.read_text(encoding="utf-8", errors="ignore"):
                achados.append(f)
    return achados


def linha_root(dest: Path) -> str:
    """'ROOT := $(abspath ..N vezes..)' calculado da PROFUNDIDADE REAL do
    destino em relacao a raiz do repo -- elimina a armadilha mais citada de
    models/README.md (copiar o Makefile do stub, 4 niveis por morar em
    fixtures/, para um destino de 3 niveis, e esquecer de tirar um '../')."""
    n = len(dest.resolve().relative_to(REPO_ROOT).parts)
    return "$(abspath " + "/".join([".."] * n) + ")"


def main() -> int:
    args = parse_args()

    if not NOME_RE.match(args.name):
        print(
            f"nome invalido: '{args.name}' -- use minusculas/digitos/underscore, comecando por letra",
            file=sys.stderr,
        )
        return 1

    origem = ORIGENS[args.kind]
    if args.dest:
        dest = Path(args.dest)
        if not dest.is_absolute():
            dest = REPO_ROOT / dest
    else:
        dest = REPO_ROOT / "models" / "player" / args.name

    if dest.exists():
        print(f"'{dest}' ja existe -- escolha outro --name/--dest", file=sys.stderr)
        return 1

    print(f"copiando {origem.relative_to(REPO_ROOT)} -> {dest.relative_to(REPO_ROOT)} ...")
    shutil.copytree(origem, dest, ignore=IGNORAR)

    # 1. meson.build -- mesma receita ja documentada em models/README.md secao 2
    #    (sed sobre a string entre aspas simples, que cobre project() E
    #    shared_module() num passo so).
    meson = dest / "meson.build"
    substituir(meson, f"'{origem.name}'", f"'{args.name}'")

    if args.kind == "stub":
        # 2a. renomeia o arquivo fonte e a referencia files(...) em meson.build
        old_cpp = dest / "src" / f"{origem.name}.cpp"
        new_cpp = dest / "src" / f"{args.name}.cpp"
        old_cpp.rename(new_cpp)
        substituir(meson, f"files('src/{origem.name}.cpp')", f"files('src/{args.name}.cpp')")
        plugin_cpp = new_cpp
        arquivos_ns = [new_cpp]
    else:
        # 2b. template: namespace C++ em TODOS os arquivos de uma vez (a
        #     mesma receita de docs/PRIMEIROS-PASSOS.md passo 2, "grep -rl |
        #     xargs sed").
        plugin_cpp = dest / "src" / "plugin.cpp"
        arquivos_ns = arquivos_contendo(dest, f"x{origem.name}", ["include", "src", "tests"])

    # 3. namespace aninhado -- CONTRATO.md secao 6: 'xstub'/'xtemplate' -> 'x<nome>'
    ns_velho = f"x{origem.name}"
    ns_novo = "x" + re.sub(r"[^a-z0-9]", "", args.name.lower())
    for f in arquivos_ns:
        substituir(f, ns_velho, ns_novo)

    # 4. MIXR_PLUGIN_DEFINE -- o primeiro argumento e a string que o host usa
    #    para identificar o plugin no descritor; tem que bater com o novo nome.
    substituir(plugin_cpp, f'MIXR_PLUGIN_DEFINE("{origem.name}"', f'MIXR_PLUGIN_DEFINE("{args.name}"')

    # 5. ROOT do Makefile -- calculado, nunca copiado (ver linha_root()).
    makefile = dest / "Makefile"
    texto = makefile.read_text(encoding="utf-8")
    novo, n = re.subn(r"^ROOT\s*:=.*$", f"ROOT      := {linha_root(dest)}", texto, count=1, flags=re.MULTILINE)
    if n == 0:
        print(f"  aviso: linha 'ROOT :=' nao encontrada em {makefile.relative_to(REPO_ROOT)}", file=sys.stderr)
    makefile.write_text(novo, encoding="utf-8")

    # 6a. lib<origem>.so -- literal em Makefile/README/docs (comentarios do
    #     alvo `install`, echo de sucesso, ldd de verificacao). Esta ultima
    #     e FUNCIONAL, nao so prosa: o Makefile copiado confere
    #     `ldd .../lib{origem}.so` dentro do proprio alvo `install` -- sem
    #     este passo, 'make install' checaria o arquivo ERRADO.
    for f in dest.rglob("*"):
        if f.is_file() and (f.name == "Makefile" or f.suffix == ".md"):
            substituir(f, f"lib{origem.name}.so", f"lib{args.name}.so", obrigatorio=False)

    # 6b. CHANGELOG.md -- esvaziado e recomecado (docs/PRIMEIROS-PASSOS.md
    #     passo 3), com a versao lida do PROPRIO meson.build copiado.
    m = re.search(r"version:\s*'([^']+)'", meson.read_text(encoding="utf-8"))
    versao = m.group(1) if m else "0.1.0"
    origin_rel = origem.name if args.kind == "template" else f"fixtures/{origem.name}"
    (dest / "CHANGELOG.md").write_text(
        NOVO_CHANGELOG.format(name=args.name, version=versao, date=date.today().isoformat(),
                               origin_rel=origin_rel, kind=args.kind),
        encoding="utf-8",
    )

    # 7. README.md -- so o titulo (H1), a prosa fica para o passo manual
    #    (models/README.md secao 2 / docs/PRIMEIROS-PASSOS.md passo 5).
    readme = dest / "README.md"
    substituir(readme, f"`{origem.name}`", f"`{args.name}`", obrigatorio=False)

    # 8. confere que nada do nome/namespace antigo sobrou (mesmo grep que o
    #    passo 2 do PRIMEIROS-PASSOS.md sugere rodar a mao)
    sobras = arquivos_contendo(dest, ns_velho, ["include", "src", "tests"])
    sobras += [f for f in arquivos_contendo(dest, f"'{origem.name}'", ["."]) if f.name == "meson.build"]
    if sobras:
        print("  aviso: ainda ha ocorrencias do nome/namespace antigo em:")
        for f in sobras:
            print(f"    - {f.relative_to(REPO_ROOT)}")

    if args.no_build:
        print(f"scaffold de models/player/{args.name}/ pronto (build de verificacao PULADO, --no-build).")
        imprimir_checklist(args.name)
        return 0

    # 9. build de fumaca REAL -- prova que o scaffold compila, testa E instala
    #    (== popula ./dist/lib/mixr-plugins/) antes de devolver ao usuario.
    #    'test: build' NAO chama 'meson install' (ver os dois Makefiles) --
    #    por isso os dois alvos, nao so 'test'. Requer 'make configure &&
    #    make sdk' ja rodado na raiz (mesmo pre-requisito de qualquer modelo
    #    deste repositorio).
    print("compilando, testando e instalando o scaffold (make test install) ...")
    rc = subprocess.run(["make", "-C", str(dest), "test", "install"]).returncode
    if rc != 0:
        print(
            "\nFALHOU o build/teste de verificacao -- o scaffold ficou em "
            f"models/player/{args.name}/, incompleto. NAO apague a pasta: compare com "
            f"models/player/{'fixtures/stub' if args.kind == 'stub' else 'template'}/ para achar "
            "o que sobrou, ou confira se 'make configure && make sdk' ja rodou na raiz.",
            file=sys.stderr,
        )
        return 1

    so = dest / "dist" / "lib" / "mixr-plugins" / f"lib{args.name}.so"
    if not so.exists():
        print(f"  aviso: {so.relative_to(REPO_ROOT)} nao foi gerado -- confira o nome do "
              "shared_module() em meson.build", file=sys.stderr)
    else:
        nm = subprocess.run(["nm", "-D", "--defined-only", str(so)], capture_output=True, text=True)
        fortes = [l for l in nm.stdout.splitlines() if " T " in l]
        if len(fortes) != 1:
            print(f"  aviso: esperava exatamente 1 simbolo T exportado, achei {len(fortes)}", file=sys.stderr)
        ldd = subprocess.run(["ldd", str(so)], capture_output=True, text=True)
        if "not found" in ldd.stdout:
            print("  aviso: ldd reporta dependencia nao resolvida:\n" + ldd.stdout, file=sys.stderr)

    print(f"\nscaffold de models/player/{args.name}/ pronto e compilando/testando verde.")
    imprimir_checklist(args.name)
    return 0


def imprimir_checklist(nome: str) -> None:
    print(f"""
Falta, MANUALMENTE (nada disto e automatizavel):

  [ ] a regra de negocio de verdade (docs/PRIMEIROS-PASSOS.md, passo 5, se veio do
      template -- domain -> ubf -> xnative)
  [ ] preservar as chamadas ao xboard em ubf/*Action::execute() (a UNICA obrigacao que
      falha em silencio -- ver models/player/fixtures/stub/docs/CONTRATO.md secao 3)
  [ ] atualizar xnative/factory.cpp (NOMES[]/METAS[]) se classes forem renomeadas/removidas
  [ ] revisar a prosa de README.md/docs/*.md -- so o titulo foi trocado, o resto ainda
      descreve a origem ({nome} copiou de fixtures/stub ou template/)
  [ ] o bloco `provides:` do .edl do SEU cenario (tem que bater EXATAMENTE com o que o
      .so exporta)
  [ ] git add models/player/{nome}/ (este script nao commita nada)
  [ ] decidir se este modelo entra em 'make models'/'make test' da raiz:
      -> se o Makefile raiz ja descobre modelos por 'find' (confira 'models:' nele),
         nao precisa fazer nada -- este diretorio ja entra sozinho.
      -> senao, acrescente uma linha ao alvo `models:` do Makefile raiz, no molde de
         'missile' (ver models/README.md, secao 2).
  [ ] adicionar uma linha em .github/CODEOWNERS para models/player/{nome}/
""")


if __name__ == "__main__":
    sys.exit(main())
