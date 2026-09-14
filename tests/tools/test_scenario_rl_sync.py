#!/usr/bin/env python3
"""src/rl/configs/scenario_rl.edl e src/poc/rl-training/configs/scenario_rl.edl
tem de continuar BYTE A BYTE identicos.

As duas copias existem por uma razao ja documentada em CLAUDE.md (secao
"src/rl"): 'src/rl' e' so' o AMBIENTE (mixr_gym.MixrFlightEnv, dependencias
minimas -- gymnasium+numpy), 'src/poc/rl-training' e' quem TREINA (venv proprio,
stable-baselines3/torch) -- os dois moram sob raizes diferentes e nenhum dos
dois importa o outro, entao o cenario que cada lado carrega e' uma copia, nao
um symlink nem um caminho compartilhado. A duplicacao e' deliberada
(documentada), mas so' enquanto os dois arquivos continuarem IDENTICOS -- o
dia em que alguem editar um lado e esquecer do outro, o ambiente de treino e o
de execucao passam a rodar cenarios SUTILMENTE diferentes sem nenhum aviso
(os dois passam no proprio parser/lint, cada um sozinho, porque os dois sao
.edl validos -- so' o CONTEUDO diverge).

Sem framework nenhum, mesmo estilo de test_class_diagram_sync.py/
test_extract_class_diagram.py: um 'main()' com exit code.
"""
from __future__ import annotations

import difflib
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

A = REPO_ROOT / "src" / "rl" / "configs" / "scenario_rl.edl"
B = REPO_ROOT / "src" / "poc" / "rl-training" / "configs" / "scenario_rl.edl"


def main() -> int:
    for caminho in (A, B):
        if not caminho.is_file():
            sys.exit(f"FALHOU: {caminho.relative_to(REPO_ROOT)} nao existe mais -- "
                      f"este teste afirma que as DUAS copias existem e sao identicas.")

    texto_a = A.read_bytes()
    texto_b = B.read_bytes()

    if texto_a == texto_b:
        print(f"OK -- {A.relative_to(REPO_ROOT)} e {B.relative_to(REPO_ROOT)} "
              f"sao byte a byte identicos ({len(texto_a)} bytes).")
        return 0

    diff = list(difflib.unified_diff(
        texto_a.decode("utf-8", errors="replace").splitlines(keepends=True),
        texto_b.decode("utf-8", errors="replace").splitlines(keepends=True),
        fromfile=str(A.relative_to(REPO_ROOT)),
        tofile=str(B.relative_to(REPO_ROOT)),
    ))
    print("FALHOU: as duas copias de scenario_rl.edl divergiram.")
    print("".join(diff[:200]))
    print("  Reconcilie os dois arquivos (copie o lado correto por cima do outro) --")
    print("  ver CLAUDE.md, secao 'src/rl', para o porque de existirem duas copias.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
