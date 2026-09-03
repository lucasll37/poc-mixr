#!/usr/bin/env python3
"""Uma politica rodando DENTRO do frame, no binario de verdade.

O que este teste prova, e que nenhum outro prova:

  1. A cadeia inteira funciona ponta a ponta -- o no carrega o artefato
     instalado (.onnx ou .py), empacota o WorldView na ordem canonica, decide,
     e o comando chega ao Autopilot (o rotulo esperado aparece no dump).
  2. Isso NAO quebra o determinismo. Os dumps de 1, 2 e 4 threads de tempo
     critico tem de sair BYTE-IDENTICOS, com quatro aeronaves decidindo em
     paralelo na fase 3 -- compartilhando UMA sessao do ONNX Runtime, ou UM
     interpretador Python cujo GIL e adquirido em ordem arbitraria.
  3. A decisao continua amarrada ao frame: 'dec' avanca na MESMA taxa que
     'frame' entre dumps consecutivos -- a mesma assercao de
     tests/determinism/check_determinism.sh.

A fixture e DERIVADA do cenario de producao (make_fixture.py), nao uma copia
versionada: uma copia comecaria certa e envelheceria em silencio. O unico
delta e o 'treeFile:', que passa a apontar para a arvore de deploy da
politica.
"""

import argparse
import pathlib
import re
import subprocess
import sys
import tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[2]


def gerar_fixture(destino: pathlib.Path, arvore: str) -> None:
    subprocess.run(
        [sys.executable, str(RAIZ / "tests/scenario/make_fixture.py"),
         "--poc", "multi-thread", "--mode", "intruder", "--out", str(destino)],
        check=True, cwd=RAIZ, stdout=subprocess.DEVNULL)
    texto = destino.read_text()
    if "flight_tree.xml" not in texto:
        sys.exit("fixture nao tem 'flight_tree.xml' -- o cenario mudou de forma?")
    destino.write_text(texto.replace("flight_tree.xml", arvore))


def rodar(binario: str, fixture: pathlib.Path, threads: int, frames: int) -> list[str]:
    saida = subprocess.run(
        [binario, "-f", str(fixture), "-threads", str(threads), "-deterministic", str(frames)],
        check=True, cwd=RAIZ, capture_output=True, text=True, timeout=600).stdout
    return [l for l in saida.splitlines() if l.startswith("frame=")]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--tree", required=True, help="nome do .xml da arvore")
    ap.add_argument("--label", required=True, help="rotulo esperado em bt=")
    ap.add_argument("--frames", type=int, default=600)
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        fixture = pathlib.Path(tmp) / "onnx.epp.in"
        gerar_fixture(fixture, args.tree)

        dumps = {}
        for t in (1, 2, 4):
            dumps[t] = rodar(args.binario, fixture, t, args.frames)
            if not dumps[t]:
                sys.exit(f"threads={t}: nenhuma linha 'frame=' -- o binario nao rodou?")
        # repeticao com 4 threads: pega nao-determinismo entre execucoes IGUAIS,
        # que a comparacao 1-vs-4 sozinha nao pegaria.
        dumps["4b"] = rodar(args.binario, fixture, 4, args.frames)

        # 1. a politica de fato decidiu
        rotulos = set(re.findall(r"bt=(\S+)", "\n".join(dumps[1])))
        if rotulos != {args.label}:
            sys.exit(f"esperava so 'bt={args.label}', veio {sorted(rotulos)} -- "
                     "o no caiu no Fallback (o artefato nao carregou?)")
        print(f"OK  a politica decidiu em todos os dumps (bt={args.label})")

        # 2. determinismo
        for a, b in ((1, 2), (1, 4), (4, "4b")):
            if dumps[a] != dumps[b]:
                for la, lb in zip(dumps[a], dumps[b]):
                    if la != lb:
                        sys.exit(f"threads {a} vs {b} divergiram:\n  {la}\n  {lb}")
                sys.exit(f"threads {a} vs {b}: numero de linhas diferente")
            print(f"OK  threads {a} vs {b}: dumps byte-identicos")

        # 3. dec avanca na mesma taxa que frame
        anterior: dict[str, tuple[int, int]] = {}
        for linha in dumps[1]:
            frame = int(re.search(r"frame=(\d+)", linha).group(1))
            player = re.search(r"player=(\S+)", linha).group(1)
            dec = int(re.search(r"dec=(\d+)", linha).group(1))
            if player in anterior:
                df, dd = frame - anterior[player][0], dec - anterior[player][1]
                if df != dd:
                    sys.exit(f"{player}: frame avancou {df} mas dec avancou {dd}")
            anterior[player] = (frame, dec)
        print(f"OK  dec avanca na mesma taxa que frame, para os {len(anterior)} avioes")

    print(f"politica {args.label}: OK")


if __name__ == "__main__":
    main()
