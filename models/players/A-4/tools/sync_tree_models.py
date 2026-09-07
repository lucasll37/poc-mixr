#!/usr/bin/env python3
#
# Resincroniza o <TreeNodesModel> nos 5 flight_tree*.xml de producao com a
# saida de verdade do binario dump-tree-model (BT::writeTreeNodesModelXML()
# nativo do BT.CPP, sobre a MESMA factory que o modelo registra em
# bt_factory.cpp/bt_factory_sdk.cpp -- ver CLAUDE.md, "Groot -- editor e
# monitor ao vivo").
#
# Dois modos:
#   sync_tree_models.py             -- corrige os 5 arquivos in-place
#   sync_tree_models.py --check     -- so' verifica, nao escreve; exit 1 se
#                                       algum arquivo estiver desatualizado
#                                       (--check e' o que o teste 'tree-model-
#                                       sync' do meson roda, ver tests/meson.build)
#
# --binary aponta pro executavel dump-tree-model; default e' o caminho de
# build convencional deste projeto (models/players/A-4/build/tests/), mas o
# teste do meson passa o caminho de verdade explicitamente (nao pode
# presumir que o build dir se chama "build").
#
# uso: models/players/A-4/tools/sync_tree_models.py [--check] [--binary PATH]
import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODEL_ROOT = HERE.parent
CONFIGS = MODEL_ROOT / "configs"
DEFAULT_BINARY = MODEL_ROOT / "build" / "tests" / "dump-tree-model"

FILES = [
    "flight_tree.xml",
    "flight_tree_missile_demo.xml",
    "flight_tree_onnx.xml",
    "flight_tree_py.xml",
    "flight_tree_nav.xml",
]

BLOCK_RE = re.compile(r"  <TreeNodesModel>.*?</TreeNodesModel>\n", re.DOTALL)


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

    result = subprocess.run([str(args.binary)], capture_output=True, text=True, check=True)
    generated = result.stdout.rstrip("\n")
    if "<TreeNodesModel>" not in generated:
        sys.exit(f"dump-tree-model nao produziu um <TreeNodesModel> valido:\n{result.stdout}")

    changed = []
    outdated = []
    for name in FILES:
        path = CONFIGS / name
        content = path.read_text()
        new_content, count = BLOCK_RE.subn("  " + generated + "\n", content)
        if count != 1:
            sys.exit(
                f"{name}: esperava exatamente 1 bloco <TreeNodesModel>, achei {count} "
                "-- abortando sem tocar em nenhum arquivo"
            )
        if new_content != content:
            if args.check:
                outdated.append(name)
            else:
                path.write_text(new_content)
                changed.append(name)

    if args.check:
        if outdated:
            sys.exit(
                "Desatualizados (rode models/players/A-4/tools/sync_tree_models.py sem --check): "
                + ", ".join(outdated)
            )
        print("Os 5 flight_tree*.xml batem com o que bt_factory.cpp/bt_factory_sdk.cpp registram.")
        return

    if changed:
        print("Resincronizados:", ", ".join(changed))
    else:
        print("Ja estava tudo em dia -- nenhum arquivo mudou.")


if __name__ == "__main__":
    main()
