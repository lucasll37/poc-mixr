#!/usr/bin/env python3
#
# Atualiza o <TreeNodesModel> de toda arvore .xml em configs/ com a saida de
# verdade do binario dump-tree-model (BT::writeTreeNodesModelXML() nativo do
# BT.CPP, sobre a MESMA factory que o modelo registra em
# bt_factory.cpp/bt_factory_sdk.cpp -- ver CLAUDE.md, "Groot -- editor e
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

BLOCK_RE = re.compile(r"  <TreeNodesModel>.*?</TreeNodesModel>\n", re.DOTALL)


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
    new_content, count = BLOCK_RE.subn(block, content)
    if count > 1:
        raise ValueError(f"achei {count} blocos <TreeNodesModel>, esperava no maximo 1")
    if count == 1:
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
              "bt_factory.cpp/bt_factory_sdk.cpp registram.")
        return

    if changed:
        print("Atualizados:", ", ".join(changed))
    else:
        print("Ja estava tudo em dia -- nenhum arquivo mudou.")


if __name__ == "__main__":
    main()
