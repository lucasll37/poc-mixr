#!/usr/bin/env python3
"""'-threads'/'-deterministic' com um token nao numerico nao pode DERRUBAR o
processo com um sinal -- tem de recusar com uma mensagem clara e sair com um
codigo de erro, o MESMO padrao que toda outra entrada invalida desta
aplicacao ja segue (arquivo de cenario ausente, player que nao existe no
cenario, banco de elevacao corrompido -- ver TerrainData/StationBuilder/
Fleet, todos em app/, todos saindo por std::exit com mensagem).

POR QUE ISTO EXISTE: app::parseCommandLine() usava std::stol()/std::stoi()
sem tratar excecao -- um '-threads abc' ou '-deterministic xyz' terminava com
std::invalid_argument nao capturada (e um '-threads 99999999999999999999'
com std::out_of_range), as duas virando abort()/SIGABRT sem nenhuma mensagem
apontando QUAL argumento estava errado. Nenhum teste ponta-a-ponta cobria
esse caminho porque todo test() deste repositorio so passa numeros validos
para essas duas flags.

Nao precisa de fixture nem de cenario -- o parse de argv acontece ANTES de
qualquer coisa em main() (antes ate de app::ensureTerrainData()), entao o
processo sai quase instantaneamente nos dois sentidos.
"""

import argparse
import subprocess
import sys

CASOS = [
    (["-threads", "abc"], "threads-nao-numerico"),
    (["-deterministic", "xyz"], "deterministic-nao-numerico"),
    (["-threads", "99999999999999999999"], "threads-estoura-o-tipo"),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    args = ap.parse_args()

    falhas = []
    for extra, rotulo in CASOS:
        proc = subprocess.run(
            [args.binario] + extra,
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, timeout=30,
        )
        if proc.returncode < 0:
            falhas.append(
                f"{rotulo}: terminou pelo sinal {-proc.returncode} (crash) -- "
                "esperava saida limpa com codigo de erro"
            )
        elif proc.returncode == 0:
            falhas.append(f"{rotulo}: saiu com rc=0 -- deveria recusar o argumento invalido")
        print(f"  [{rotulo}] rc={proc.returncode} stderr={proc.stderr.strip()!r}")

    if falhas:
        print()
        for f in falhas:
            print(f"  FALHA {f}")
        print("argumentos invalidos: FALHOU")
        return 1

    print("argumentos invalidos: OK (saida limpa, sem crash, nas tres formas)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
