#!/usr/bin/env python3
"""
O MODELO DESCONHECIDO -- a prova que nenhum outro teste da.

Todos os outros testes de plugin carregam O MESMO modelo, compilado do mesmo
fonte. Nenhum deles pode falhar por "um .so que eu nao escrevi nao serve":
o modelo e conhecido e identico. Este pode.

Ele roda o cenario de PRODUCAO -- as mesmas 13 construcoes, os mesmos ~20
slots com tipo e unidade -- trocando UMA COISA: o 'file:' do ( PluginModule ).
O 'provides:' nao muda, porque o stub declara exatamente os mesmos 6 nomes;
se declarasse outros, a assercao de igualdade do registro ja recusaria a carga.

O que isso verifica, e nada mais verifica:
   que os 6 nomes de fabrica + os slots + as classes base + a escrita no
   xboard sao SUFICIENTES para um modelo desconhecido fazer a aplicacao
   funcionar.

E o que ele obriga e o ganho maior: para o stub existir, alguem teve de
DESCOBRIR e ESCREVER todas as obrigacoes que estavam implicitas. Cada uma que
faltasse apareceria aqui -- como falha de carga, ou como 'bt=--' no dump.
Ver models/fixtures/stub/docs/CONTRATO.md.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
SIGSEGV = 128 + 11


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--poc", required=True)
    ap.add_argument("--stub", required=True)
    args = ap.parse_args()

    if not Path(args.stub).is_file():
        print(f"FALHA: {args.stub} nao existe -- rode 'make models'")
        return 1

    out = RAIZ / "build" / "tests-fixtures"
    out.mkdir(parents=True, exist_ok=True)

    base = out / f"{args.poc}-stub-base.epp.in"
    subprocess.run(
        [sys.executable, str(RAIZ / "tests/scenario/make_fixture.py"),
         "--poc", args.poc, "--mode", "intruder", "--out", str(base)],
        cwd=RAIZ, check=True, capture_output=True,
    )
    texto = base.read_text()

    # A UNICA edicao. Se fosse preciso mexer em mais alguma coisa para um
    # modelo estranho rodar, seria exatamente isso que este teste teria de
    # denunciar -- entao a edicao minima e parte da assercao.
    novo, n = re.subn(r'file:\s*"[^"]*"', f'file:     "{args.stub}"', texto, count=1)
    if n != 1:
        print("FALHA: nao achei o 'file:' do ( PluginModule ) na fixture")
        return 1

    fixture = out / f"{args.poc}-stub.epp.in"
    fixture.write_text(novo)

    r = subprocess.run(
        [args.binario, "-f", str(fixture), "-threads", "1", "-deterministic", "200"],
        cwd=RAIZ, capture_output=True, text=True, timeout=600,
    )
    saida = r.stdout + r.stderr
    falhas = []

    if r.returncode == SIGSEGV or r.returncode == -11:
        falhas.append("SIGSEGV")
    elif r.returncode != 0:
        falhas.append(f"o processo saiu com {r.returncode}")

    if "[plugin] carregado 'stub'" not in saida:
        falhas.append("o stub nao foi carregado")

    linhas = [l for l in saida.splitlines() if l.startswith("frame=")]
    if not linhas:
        falhas.append("nenhuma linha 'frame=' -- a aplicacao nao chegou a rodar")

    campos = [dict(re.findall(r"(\w+)=(\S+)", l)) for l in linhas]
    players = {c.get("player") for c in campos}
    if len(players) != 4:
        falhas.append(f"esperava 4 players no dump, vieram {len(players)}: {sorted(players)}")

    # As DUAS assercoes que so o xboard satisfaz. Sem elas, um modelo que
    # esquecesse de escrever no quadro passaria em todo o resto.
    rotulos = {c.get("bt") for c in campos}
    if rotulos == {"--"} or "--" in rotulos:
        falhas.append(f"'bt=' veio vazio ({rotulos}) -- o modelo nao escreveu no xboard")

    ultimos = [c for c in campos if c.get("frame") == "200"]
    decs = {int(c.get("dec", 0)) for c in ultimos}
    if not decs or max(decs) == 0:
        falhas.append("'dec=' ficou em 0 -- o modelo nao contou decisao no xboard")

    # E que o voo aconteceu de verdade: a aeronave se moveu.
    primeiros = [c for c in campos if c.get("frame") == "100"]
    if primeiros and ultimos:
        p0 = {c["player"]: float(c["n"]) for c in primeiros}
        p1 = {c["player"]: float(c["n"]) for c in ultimos}
        parados = [k for k in p0 if abs(p1.get(k, p0[k]) - p0[k]) < 1.0]
        if len(parados) == len(p0):
            falhas.append("nenhum aviao se moveu entre os frames 100 e 200")

    # A varredura de radar so passa pelo caminho de TEMPO REAL -- o
    # '-deterministic' nao chama o RealTimeRun. Sem esta parte, a obrigacao de
    # publicar a varredura (CONTRATO.md) ficaria sem verificacao nenhuma para
    # um modelo estranho: o host degrada em silencio quando ninguem publica.
    acmi = RAIZ / "build" / "tests-recordings" / f"{args.poc}-stub.acmi"
    fixture_rt = out / f"{args.poc}-stub-rt.epp.in"
    fixture_rt.write_text(re.sub(r'fileName:\s*"[^"]*\.acmi"',
                                 f'fileName: "{acmi}"', novo, count=1))
    acmi.parent.mkdir(parents=True, exist_ok=True)
    if acmi.exists():
        acmi.unlink()
    try:
        subprocess.run([args.binario, "-f", str(fixture_rt)],
                       cwd=RAIZ, capture_output=True, text=True, timeout=8)
    except subprocess.TimeoutExpired:
        pass   # o laco de tempo real so para com Ctrl+C; o timeout E o fim

    if not acmi.exists():
        falhas.append("a execucao de tempo real nao gerou .acmi")
    else:
        texto_acmi = acmi.read_text(errors="replace")
        n = texto_acmi.count("RadarAzimuth")
        if n == 0:
            falhas.append("nenhuma linha com RadarAzimuth no .acmi -- o modelo"
                          " estranho nao publicou a varredura no xboard")
        else:
            print(f"  OK   varredura de radar chegou ao Tacview ({n} linhas)")

    if falhas:
        print("modelo estranho: FALHOU")
        for f in falhas:
            print(f"  - {f}")
        print("\n".join("    " + l for l in saida.splitlines()[-25:]))
        return 1

    print(f"  OK   o cenario de PRODUCAO rodou com um modelo desconhecido")
    print(f"  OK   4 players, bt={sorted(rotulos)}, dec ate {max(decs)}")
    print("modelo estranho: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
