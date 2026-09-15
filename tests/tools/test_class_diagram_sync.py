#!/usr/bin/env python3
"""Staleness do CLASS_DIAGRAM embutido em docs/manual/doc.jsx.

A aba "Estrutura" de docs/manual/doc.jsx cola o JSON que
tools/extract_class_diagram.py imprime direto como
`const CLASS_DIAGRAM = {...};` (linha unica, colada a mao -- nao ha passo
de 'make open-docs' que regenere isso sozinho). Sem este teste, um header
do MIXR mudar sem que alguem rode o extrator e cole o resultado de volta
nunca e pego por 'make test'. Mesmo padrao ja usado para as arvores BT.CPP
(update_bt_models.py --check, alvo 'tree-model-sync').

Sem framework nenhum, mesmo estilo de test_extract_class_diagram.py: um
'main()' com exit code, sem asserts espalhados.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools"))

import extract_class_diagram as ext  # noqa: E402

DOC_JSX = REPO_ROOT / "docs" / "manual" / "doc.jsx"
MARKER = "const CLASS_DIAGRAM = "


def embedded_class_diagram() -> dict:
    """O que esta DE FATO commitado em doc.jsx hoje."""
    src = DOC_JSX.read_text(encoding="utf-8")
    idx = src.find(MARKER)
    if idx < 0:
        sys.exit(f"'{MARKER.strip()}' nao encontrado em {DOC_JSX} -- o marcador mudou? ajuste este teste.")
    start = idx + len(MARKER)
    # json.JSONDecoder().raw_decode() para de ler assim que o objeto fecha --
    # nao precisa achar o ';' final a mao (o JSON nunca contem ';' bruto fora
    # de string, mas procurar pelo fechamento de verdade do objeto e' mais
    # robusto do que confiar nisso).
    obj, _end = json.JSONDecoder().raw_decode(src, start)
    return obj


def fresh_class_diagram() -> dict:
    """O que tools/extract_class_diagram.py produziria SE rodado agora --
    mesma montagem de 'output' que main() daquele script faz, sem passar
    pelo stdout/subprocess (chamando build_diagram() direto, mais rapido e
    sem depender do CLI)."""
    classes, tier2, warnings = ext.build_diagram()
    if warnings:
        sys.exit(f"tools/extract_class_diagram.py emitiu avisos -- corrija a extracao antes de "
                  f"comparar staleness: {warnings}")
    # _bodyLength/_leftover sao so' para a auto-verificacao do proprio
    # extrator (ver extract_class_diagram.py::main()) -- nunca vazam para o
    # JSON que de fato vai para doc.jsx.
    public_classes = {
        name: {k: v for k, v in data.items() if not k.startswith("_")}
        for name, data in classes.items()
    }
    return {"generatedBy": "tools/extract_class_diagram.py", "classes": public_classes, "tier2": tier2}


def main() -> int:
    embutido = embedded_class_diagram()
    fresco = fresh_class_diagram()

    if embutido == fresco:
        print("OK -- CLASS_DIAGRAM embutido em docs/manual/doc.jsx bate com "
              "tools/extract_class_diagram.py.")
        return 0

    chaves_divergentes = sorted(
        chave for chave in (set(embutido) | set(fresco))
        if embutido.get(chave) != fresco.get(chave)
    )
    classes_divergentes = sorted(
        nome for nome in (set(embutido.get("classes", {})) | set(fresco.get("classes", {})))
        if embutido.get("classes", {}).get(nome) != fresco.get("classes", {}).get(nome)
    )
    print(f"FALHOU: docs/manual/doc.jsx esta com CLASS_DIAGRAM DESATUALIZADO.")
    print(f"  chaves de topo divergentes: {chaves_divergentes}")
    if classes_divergentes:
        print(f"  classes divergentes: {classes_divergentes}")
    print("  Rode 'python3 tools/extract_class_diagram.py' e cole o JSON de saida como")
    print(f"  '{MARKER}<json>;' em {DOC_JSX.relative_to(REPO_ROOT)} (a mesma linha, ainda uma so).")
    return 1


if __name__ == "__main__":
    sys.exit(main())
