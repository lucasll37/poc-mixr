#!/usr/bin/env python3
"""
A AFIRMACAO CENTRAL: o comportamento vem da .so, nao do executavel.

O mesmo binario, sem ser recompilado nem relinkado, produz dumps diferentes
conforme a .so que o cenario manda carregar. E o que significa "sem precisar
recompilar tudo".

Por que duas variantes PRE-BUILDADAS em vez de chamar 'ninja' aqui: invocar o
sistema de build de dentro de um teste que o proprio build disparou e fragil (o
diretorio esta em uso). A prova com rebuild de verdade -- incluindo a conferencia
de que o ninja toca DUAS edges e nao cita o executavel -- fica no alvo
'make check-plugin-hotswap'. Esta camada afirma a propriedade; aquele alvo
demonstra o fluxo.

As duas variantes diferem no SENTIDO da curva do circuito de patrulha
(POC_MODEL_TURN_SIGN, ver domain/PatrolPlan.cpp) -- uma regra de negocio que o
EDL nao sobrescreve. O slot 'legTurn:' ajusta o ANGULO da curva, nunca o
sentido; se o teste mexesse nele, provaria so que a configuracao funciona, e
nao que CODIGO novo entrou no processo.

A fixture faz DUAS coisas de proposito, e as duas foram descobertas quebrando:

  * REMOVE o intruso. A fixture 'intruder' injeta um bandit1 local, e com ele
    a arvore vai para EVADE -- a patrulha quase nao roda e o sentido da curva
    deixa de importar (medido: bt=EVADE nos 400 frames, rumos a 0,2 grau um do
    outro);
  * encurta o 'legTime:' para 10 s e roda 1500 frames (30 s a 50 Hz). O
    circuito de producao vira a cada 45-60 s, entao numa janela curta o aviao
    nem trocaria de perna. E a perna nao pode ser CURTA demais: com
    maxRateOfTurnDps=3, um legTime de 2 s faz o rumo comandado saltar mais
    rapido do que a aeronave consegue seguir, e em 4 pernas os +90 e os -90
    somam 360 e voltam a coincidir.
"""

import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def dump(binario, fixture):
    r = subprocess.run(
        [binario, "-f", str(fixture), "-threads", "1", "-deterministic", "1500"],
        cwd=RAIZ, capture_output=True, text=True, timeout=300,
    )
    if r.returncode != 0:
        print(r.stdout[-3000:])
        print(r.stderr[-3000:])
        raise SystemExit(f"execucao falhou (rc={r.returncode})")
    return [l for l in r.stdout.splitlines() if l.startswith("frame=")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--poc", required=True)
    ap.add_argument("--so-a", required=True)
    ap.add_argument("--so-b", required=True)
    args = ap.parse_args()

    out = RAIZ / "build" / "tests-fixtures"
    out.mkdir(parents=True, exist_ok=True)

    base = out / f"{args.poc}-hotswap-base.edl.in"
    subprocess.run(
        [sys.executable, str(RAIZ / "tests/scenario/make_fixture.py"),
         "--poc", args.poc, "--mode", "intruder", "--out", str(base)],
        cwd=RAIZ, check=True, capture_output=True,
    )
    texto = base.read_text()

    # Fora o intruso: sem ele a arvore fica em PATROL, que e o ramo cujo
    # comportamento as duas variantes trocam.
    i = texto.find("bandit1: (")
    if i < 0:
        raise SystemExit("nao achei o bandit1 injetado pela fixture 'intruder'")
    j = texto.index("(", i)
    nivel, k = 0, j
    while k < len(texto):
        if texto[k] == "(":
            nivel += 1
        elif texto[k] == ")":
            nivel -= 1
            if nivel == 0:
                break
        k += 1
    texto = texto[:i] + texto[k + 1:]

    # Perna curta o bastante para virar dentro da janela, longa o bastante para
    # a aeronave conseguir seguir o comando (ver o cabecalho).
    texto, n = re.subn(r"legTime:(\s*)\( Seconds \d+ \)", r"legTime:\g<1>( Seconds 10 )", texto)
    if n == 0:
        raise SystemExit("nao achei nenhum 'legTime: ( Seconds N )' na fixture")

    def fixture_para(so, nome):
        p = out / f"{args.poc}-hotswap-{nome}.edl.in"
        p.write_text(re.sub(r'file:\s*"[^"]*"', f'file: "{so}"', texto, count=1))
        return p

    antes = sha(args.binario)
    a = dump(args.binario, fixture_para(args.so_a, "a"))
    b = dump(args.binario, fixture_para(args.so_b, "b"))
    depois = sha(args.binario)

    falhas = []

    if antes != depois:
        falhas.append("o executavel mudou entre as duas execucoes (nao deveria)")
    else:
        print(f"  OK   o executavel e o MESMO nas duas execucoes (sha256 {antes[:16]}...)")

    if not a or not b:
        falhas.append("alguma execucao nao produziu linhas 'frame='")
    elif a == b:
        falhas.append("as duas .so produziram o MESMO dump -- o plugin nao esta governando nada")
    else:
        print(f"  OK   mesmo binario + .so diferente = dump diferente ({len(a)} linhas cada)")

    # A diferenca tem de estar no RUMO -- e o circuito de patrulha que inverte.
    # Todos os quatro avioes patrulham, entao todos divergem; conferimos um.
    def hdg_de(linhas, player):
        ultimo = None
        for l in linhas:
            if f"player={player} " in l:
                m = re.search(r" hdg=([\d.-]+)", l)
                if m:
                    ultimo = float(m.group(1))
        return ultimo

    h_a, h_b = hdg_de(a, "falcon1"), hdg_de(b, "falcon1")
    if h_a is None or h_b is None:
        falhas.append("nao achei hdg= do falcon1")
    elif abs(h_a - h_b) < 5.0:
        falhas.append(f"rumo do falcon1 quase igual ({h_a:.1f} x {h_b:.1f});"
                      " o circuito nao inverteu -- a perna e curta o bastante?")
    else:
        print(f"  OK   falcon1 hdg= {h_a:.1f} (so-a) x {h_b:.1f} (so-b),"
              f" delta {h_b - h_a:+.1f} deg")

    if falhas:
        print("\nhotswap: FALHOU")
        for f in falhas:
            print(f"  - {f}")
        return 1
    print("hotswap: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
