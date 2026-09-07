#!/usr/bin/env python3
"""Regressao do binario 'edlcheck' (app/src/edlcheck_main.cpp): os
cenarios .edl/.edl.in REAIS deste repositorio tem que passar pelo parser
MIXR de verdade (a mesma cadeia de fabricas de producao, mixr_factory.cpp)
com exit code 0 -- e o oraculo DEFINITIVO, ao contrario do lint leve
(src/ui/scripts/edl_lint.py), que so aproxima a gramatica.

Depende de 'make install' ja ter rodado (precisa de dist/lib/mixr-plugins/
populado) -- mesma dependencia que toda suite 'scenario'/'plugin' ja tem;
por isso mora na mesma suite 'tools' mas so faz sentido dentro de
'make test' (que ja encadeia install), nao rodando 'meson test' isolado
sem 'make install' antes.

Uso:
    python3 tests/tools/test_edlcheck.py --binario <caminho/para/edlcheck>
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "src" / "ui" / "scripts"))

import edl_lint  # noqa: E402

REAL_SCENARIOS = [
    "src/poc/dis/single-thread/configs/scenario.edl.in",
    "src/poc/dis/single-thread/configs/scenario_missile_demo.edl.in",
    "src/poc/dis/multi-thread/configs/scenario.edl.in",
    "src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in",
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
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True, type=Path)
    args = ap.parse_args()

    if not args.binario.exists():
        raise SystemExit(f"binario nao encontrado: {args.binario}")

    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = Path(tmp)
        for rel in REAL_SCENARIOS:
            path = REPO_ROOT / rel
            check(path.exists(), f"cenario de referencia nao existe mais: {rel}")
            if not path.exists():
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            if "@" in text:
                text = edl_lint.expand_templates(text)
            expanded = tmp_dir / Path(rel).name
            expanded.write_text(text, encoding="utf-8")

            proc = subprocess.run(
                [str(args.binario), str(expanded)],
                cwd=REPO_ROOT, capture_output=True, text=True, timeout=30,
            )
            check(proc.returncode == 0,
                  f"{rel}: edlcheck saiu com codigo {proc.returncode} (esperado 0)\n"
                  f"    stdout: {proc.stdout.strip()}\n    stderr: {proc.stderr.strip()}")

        # Caso sintetico: um .edl com slot inexistente TEM que ser rejeitado
        # (>0 erros de parse) -- garante que o oraculo nao ficou "sempre OK"
        # por engano (ex.: um argumento trocado, o arquivo errado sendo lido).
        bad = tmp_dir / "bad.edl"
        bad.write_text("( Station\n   naoExisteDeVerdade: 1\n) // station\n", encoding="utf-8")
        proc = subprocess.run([str(args.binario), str(bad)], cwd=REPO_ROOT, capture_output=True, text=True, timeout=30)
        check(proc.returncode != 0, f"edlcheck deveria rejeitar um slot inexistente, saiu com codigo {proc.returncode}")

    if failures:
        print(f"FALHOU ({len(failures)}):")
        for f in failures:
            print(" -", f)
        return 1

    print(f"OK -- {len(REAL_SCENARIOS)} cenarios reais aceitos pelo parser MIXR de verdade (edlcheck); caso sintetico ruim corretamente rejeitado.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
