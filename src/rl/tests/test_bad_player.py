#!/usr/bin/env python3
"""Regressao do bug corrigido em src/rl/bindings/NativeSimulation.cpp: um
'player_name' que nao existe no cenario (typo, ou nome de um player real que
nao e o configurado com ( RLBridgeBehavior ) no .epp) tem de FALHAR alto em
reset(), nao rodar silenciosamente com 'terminated' preso em False para
sempre -- ver o comentario em NativeSimulation.hpp/reset() e a secao
"Limites conhecidos" do src/rl/README.md.

Processo PROPRIO (nao entra em test_contract.py/test_smoke.py): construir
esta MixrFlightEnv aqui e a UNICA Station deste processo -- ver a armadilha
"uma Station por processo" documentada nos outros dois arquivos de teste.
"""

import os
import sys

if not os.path.exists("./src/rl/configs/scenario_rl.epp"):
    print("ERRO: rode este script com cwd na raiz do repositorio", file=sys.stderr)
    sys.exit(1)

from mixr_gym import MixrFlightEnv  # noqa: E402  (ver ARMADILHA nos outros testes -- antes de numpy)


def main() -> int:
    env = MixrFlightEnv(player_name="player_que_nao_existe")
    try:
        env.reset()
        print("FALHOU: reset() com player_name inexistente nao levantou nada", file=sys.stderr)
        return 1
    except RuntimeError as exc:
        if "nao existe no cenario" not in str(exc):
            print(f"FALHOU: RuntimeError levantada, mas mensagem inesperada: {exc}",
                  file=sys.stderr)
            return 1
        print(f"OK  player_name inexistente levanta RuntimeError claro: {exc}")

    print("\ntest_bad_player: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
