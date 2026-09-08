#!/usr/bin/env python3
"""Regressao de src/ui/scripts/edl_lint.py: os cenarios .edl/.edl.in REAIS
deste repositorio nao podem acusar ERRO nenhum -- eles ja rodam de verdade
(make test os exercita via meson), entao um erro aqui e sempre um FALSO
POSITIVO do lint, nunca um problema real do cenario. Nao afirma nada sobre
AVISOS (referencia-por-nome pendurada é best-effort e pode legitimamente
disparar).

Puro Python, sem framework, mesmo estilo de tests/tools/test_edl_catalog.py.
"""
from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "src" / "ui" / "scripts"))

import edl_lint  # noqa: E402

REAL_SCENARIOS = [
    "src/poc/dis/flight/configs/scenario.edl.in",
    "src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in",
    "src/poc/full-systems-nav/configs/scenario_full_nav.edl.in",
    "src/poc/onnx-policy/configs/scenario.edl.in",
    "src/poc/python-flight/configs/scenario.edl.in",
    "src/poc/dis/bandit/configs/scenario.edl",
    "src/poc/rl-training/configs/scenario_rl.edl",
    "src/rl/configs/scenario_rl.edl",
]

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def main():
    catalog = edl_lint.load_catalog()

    for rel in REAL_SCENARIOS:
        path = REPO_ROOT / rel
        check(path.exists(), f"cenario de referencia nao existe mais: {rel}")
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if "@" in text:
            text = edl_lint.expand_templates(text)
        result = edl_lint.lint_text(text, catalog)
        for e in result.errors:
            failures.append(f"{rel}: {e}")

    # Caso sintetico: uma fabrica e um slot inventados TEM que ser pegos --
    # garante que o lint nao ficou mudo por engano (ex.: um bug que faca
    # lint_text() nunca entrar no loop principal passaria pelos 12 arquivos
    # reais sem denunciar nada).
    bad = "( Station\n   naoExisteDeVerdade: 1\n   fabricaFalsa: ( OutraFalsa )\n) // station\n"
    result = edl_lint.lint_text(bad, catalog)
    check(len(result.errors) >= 2, f"caso sintetico deveria acusar >=2 erros, acusou {len(result.errors)}: {result.errors}")

    # ASCII-only: um acento em QUALQUER lugar tem que ser pego (mesma regra
    # que derruba o parser real por inteiro).
    result = edl_lint.lint_text('( Station\n   // comentario com acento: está\n) // station\n', catalog)
    check(any("ASCII" in e for e in result.errors), f"deveria acusar caractere fora de ASCII, acusou: {result.errors}")

    if failures:
        print(f"FALHOU ({len(failures)}):")
        for f in failures:
            print(" -", f)
        return 1

    print(f"OK -- {len(REAL_SCENARIOS)} cenarios reais sem erro de lint; casos sinteticos de erro detectados corretamente.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
