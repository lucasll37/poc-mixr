#!/usr/bin/env python3
#
# Atualiza o <TreeNodesModel> de toda arvore .xml em configs/ com a saida de
# verdade do binario dump-tree-model (BT::writeTreeNodesModelXML() nativo do
# BT.CPP, sobre a MESMA factory que o modelo registra em
# bt_factory.cpp -- ver CLAUDE.md raiz, "Groot -- editor e
# monitor ao vivo"). E' o que o alvo 'make update-bt' roda.
#
# O bloco e' SEMPRE regenerado por inteiro a partir do registro atual --
# nunca um diff no-a-no -- porque isso ja cobre os dois lados sozinho: um no
# que saiu do bt_factory.cpp desaparece do bloco novo (removido), um no que
# entrou aparece (adicionado). Se a arvore ainda nao tem bloco nenhum (por
# exemplo, um .xml escrito a mao, sem passar por 'make create-bt'), o bloco e'
# INSERIDO antes do </root> de fechamento em vez de reportado como erro.
#
# Agnostico ao MODELO: nao ha lista de nomes fixa -- toda arvore descoberta
# em configs/ (todo .xml com <BehaviorTree> dentro) e' considerada; um no
# registrado em C++ mas ainda nao usado em XML nenhum simplesmente nao
# aparece em lugar nenhum ate a proxima chamada.
#
# Dois modos:
#   update_bt_models.py             -- corrige as arvores encontradas in-place
#   update_bt_models.py --check     -- so' verifica, nao escreve; exit 1 se
#                                       alguma arvore estiver desatualizada
#                                       (e' o que o teste 'tree-model-sync' do
#                                       meson roda, ver tests/meson.build)
#
# --binary aponta pro executavel dump-tree-model; default e' o caminho de
# build convencional deste projeto (build/tools/, relativo a este arquivo),
# mas o teste do meson passa o caminho de verdade explicitamente (nao pode
# presumir que o build dir se chama "build").
#
# uso: tools/update_bt_models.py [--check] [--binary PATH]
import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODEL_ROOT = HERE.parent
CONFIGS = MODEL_ROOT / "configs"
DEFAULT_BINARY = MODEL_ROOT / "build" / "tools" / "dump-tree-model"

# ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): a regex exigia
# EXATAMENTE dois espacos literais antes de '<TreeNodesModel>' -- uma
# reindentacao (ex.: o Groot resalvando o arquivo com QDomDocument/
# QXmlStreamWriter, que tem estilo de indentacao proprio, ou alguem
# reformatando o XML a mao com TAB) fazia o bloco existente NAO ser
# detectado. apply_tree_nodes_model() cai entao no caminho de "inserir antes
# de </root>" -- SEM erro, reportado como "inserido" -- produzindo um
# arquivo com DOIS blocos <TreeNodesModel>. So' na chamada SEGUINTE (make
# update-bt/tree-model-sync) e' que os 2 blocos sao detectados e o script
# aborta com ValueError, sem se autorreparar. '[ \t]*' tolera qualquer
# indentacao (inclusive nenhuma) antes da tag de abertura.
BLOCK_RE = re.compile(r"[ \t]*<TreeNodesModel>.*?</TreeNodesModel>\n", re.DOTALL)

# SEGUNDO ACHADO, ESTE RODANDO E DESTRUTIVO (nao redescobrir): a regex acima
# nao sabe o que e' COMENTARIO. Uma arvore cujo cabecalho MENCIONA a tag --
# ex.: "o bloco <TreeNodesModel> abaixo e' gerado, nao edite a mao", que e'
# exatamente o comentario que se quer escrever num arquivo gerado -- casa a
# partir da MENCAO e, com re.DOTALL, engole tudo ate' o '</TreeNodesModel>'
# do bloco de verdade: o resto do comentario, o '-->', o '<root>' e a
# <BehaviorTree> INTEIRA. O sintoma nao e' um erro: o script diz
# "substituido" e o arquivo fica sem arvore nenhuma (reproduzido, com perda
# real do arquivo -- recuperado de uma copia). Por isso as mencoes dentro de
# comentario sao mascaradas ANTES de procurar o bloco.
COMMENT_RE = re.compile(r"<!--.*?-->", re.DOTALL)


def _mascarar_comentarios(content: str) -> str:
    """Devolve o texto com todo comentario XML trocado por '.' do MESMO
    tamanho -- os offsets continuam validos no texto ORIGINAL, e e' isso que
    permite procurar no mascarado e recortar no original.

    Mascarar, e nao so' descartar matches que COMECAM dentro de um
    comentario: um match que comeca na mencao e termina no bloco de verdade
    CONSOME o bloco, e o finditer seguinte nunca mais o enxerga (medido -- a
    primeira tentativa de correcao filtrava por posicao de inicio e deixava
    o arquivo com DOIS blocos, com o --check reportando tudo em ordem).

    Mesma tecnica de tools/mixr_source_scan.py::mask_source() na raiz deste
    repositorio, pelo mesmo motivo."""
    return COMMENT_RE.sub(lambda m: "." * (m.end() - m.start()), content)


def discover_tree_files() -> list[Path]:
    """Acha as arvores do BT.CPP em configs/ pelo CONTEUDO (tem <BehaviorTree>
    em algum lugar), nunca por nome de arquivo -- e' o que permite este
    script valer para o conjunto de arvores de QUALQUER projeto de modelo,
    nao so' os nomes que este projeto tem hoje."""
    return sorted(
        path for path in CONFIGS.glob("*.xml")
        if "<BehaviorTree" in path.read_text()
    )


def apply_tree_nodes_model(content: str, generated: str) -> tuple[str, str]:
    """Devolve (novo_conteudo, acao), acao em
    {"inalterado", "substituido", "inserido"}. Levanta ValueError se o
    arquivo tiver mais de um bloco <TreeNodesModel> -- isso e' XML corrompido
    a mao, nao e' para adivinhar qual bloco vale."""
    block = "  " + generated + "\n"

    # So' um bloco REAL conta -- uma mencao a tag dentro de um comentario XML
    # nao e' o bloco (ver COMMENT_RE acima para o estrago que isso causava).
    matches = list(BLOCK_RE.finditer(_mascarar_comentarios(content)))
    if len(matches) > 1:
        raise ValueError(f"achei {len(matches)} blocos <TreeNodesModel>, esperava no maximo 1")
    if len(matches) == 1:
        m = matches[0]
        new_content = content[:m.start()] + block + content[m.end():]
        return new_content, ("inalterado" if new_content == content else "substituido")

    idx = content.rfind("</root>")
    if idx == -1:
        raise ValueError("sem bloco <TreeNodesModel> e sem </root> de fechamento -- nao e uma arvore valida")
    return content[:idx] + block + content[idx:], "inserido"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                         help="so' verifica (exit 1 se desatualizado); nao escreve nada")
    parser.add_argument("--binary", type=Path, default=DEFAULT_BINARY,
                         help="caminho do executavel dump-tree-model")
    args = parser.parse_args()

    if not args.binary.exists():
        sys.exit(
            f"{args.binary} nao existe -- compile primeiro:\n"
            f"  meson compile -C {MODEL_ROOT / 'build'} dump-tree-model"
        )

    tree_files = discover_tree_files()
    if not tree_files:
        sys.exit(f"nenhuma arvore .xml (com <BehaviorTree>) encontrada em {CONFIGS}")

    result = subprocess.run([str(args.binary)], capture_output=True, text=True, check=True)
    generated = result.stdout.rstrip("\n")
    if "<TreeNodesModel>" not in generated:
        sys.exit(f"dump-tree-model nao produziu um <TreeNodesModel> valido:\n{result.stdout}")

    changed = []
    outdated = []
    for path in tree_files:
        content = path.read_text()
        try:
            new_content, action = apply_tree_nodes_model(content, generated)
        except ValueError as exc:
            sys.exit(f"{path.name}: {exc} -- abortando sem tocar em nenhum arquivo")
        if action != "inalterado":
            if args.check:
                outdated.append(f"{path.name} ({action})")
            else:
                path.write_text(new_content)
                changed.append(f"{path.name} ({action})")

    if args.check:
        if outdated:
            sys.exit(
                f"Desatualizados (rode {sys.argv[0]} sem --check): "
                + ", ".join(outdated)
            )
        print(f"As {len(tree_files)} arvore(s) .xml de {CONFIGS} batem com o que "
              "bt_factory.cpp registram.")
        return

    if changed:
        print("Atualizados:", ", ".join(changed))
    else:
        print("Ja estava tudo em dia -- nenhum arquivo mudou.")


if __name__ == "__main__":
    main()
