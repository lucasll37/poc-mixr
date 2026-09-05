#!/usr/bin/env python3
"""
Os MODOS DE FALHA da carga dinamica de modelos.

Esta camada existe porque metade do valor do mecanismo esta em falhar
legivelmente. Antes dela, os dois ultimos casos desta lista terminavam em
SIGSEGV mudo -- e a causa era do proprio edl_parser, nao do plugin:

  * a mensagem "undefined factory name" do parser (edl_parser.y:97-100) esta
    num ramo alcancavel so com arg_list == nullptr, mas a producao 'arglist:'
    SEMPRE aloca um PairStream. E codigo morto;
  * e 'slot_value : SLOT_ID form' (edl_parser.y:179) faz $2->unref() sem
    checar nulo. Referenced::unref() e inline e mexe em refCount por offset
    fixo, entao nome desconhecido em posicao de slot nomeado = SIGSEGV.

Por isso TODO caso aqui afirma rc != 139 (128+SIGSEGV) explicitamente. Se
alguem remover a checagem de nome desconhecido de mixrFactory(), estes testes
ficam vermelhos em vez de o repo voltar a estourar em silencio.
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
SIGSEGV = 128 + 11

falhas = []


def roda(binario, fixture, timeout=120):
    r = subprocess.run(
        [binario, "-f", str(fixture), "-threads", "1", "-deterministic", "20"],
        cwd=RAIZ, capture_output=True, text=True, timeout=timeout,
    )
    return r.returncode, (r.stdout + r.stderr)


def caso(nome, fixture, binario, espera_no_texto):
    rc, saida = roda(binario, fixture)

    if rc == 0:
        falhas.append(f"{nome}: o processo SAIU COM 0 -- deveria ter recusado")
        print(f"  FALHA {nome}: rc=0 (esperado != 0)")
        return
    if rc == SIGSEGV or rc == -11:
        falhas.append(f"{nome}: SIGSEGV -- e exatamente o que este teste existe para impedir")
        print(f"  FALHA {nome}: SIGSEGV (rc={rc})")
        return
    faltando = [t for t in espera_no_texto if not re.search(t, saida)]
    if faltando:
        falhas.append(f"{nome}: diagnostico sem {faltando}")
        print(f"  FALHA {nome}: mensagem nao casa {faltando}")
        print("\n".join("        " + l for l in saida.splitlines()[-12:]))
        return
    print(f"  OK   {nome} (rc={rc}, diagnostico correto)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--poc", required=True)
    ap.add_argument("--bad-nosym", required=True)
    ap.add_argument("--bad-abi", required=True)
    ap.add_argument("--bad-collide", required=True)
    args = ap.parse_args()

    out = RAIZ / "build" / "tests-fixtures"
    out.mkdir(parents=True, exist_ok=True)

    # Fixture HERMETICA de base, derivada do cenario de producao -- e nao uma
    # copia versionada, que comecaria certa e envelheceria em silencio.
    base = out / f"{args.poc}-plugin-base.edl.in"
    subprocess.run(
        [sys.executable, str(RAIZ / "tests/scenario/make_fixture.py"),
         "--poc", args.poc, "--mode", "intruder", "--out", str(base)],
        cwd=RAIZ, check=True, capture_output=True,
    )
    texto = base.read_text()

    if "( PluginLoader" not in texto:
        print("FALHA: a fixture derivada nao tem o bloco ( PluginLoader ).")
        print("       O cenario de producao declara plugin? make_fixture.py o removeu?")
        return 1

    def variante(nome, transforma):
        p = out / f"{args.poc}-plugin-{nome}.edl.in"
        p.write_text(transforma(texto))
        return p

    # Recorta o bloco 'plugins: ( PluginLoader ... )' inteiro, contando
    # parenteses -- e o unico jeito honesto, ja que ele tem aninhamento.
    def sem_bloco(t):
        i = t.index("plugins: ( PluginLoader")
        j = t.index("(", i)
        nivel, k = 0, j
        while k < len(t):
            if t[k] == "(":
                nivel += 1
            elif t[k] == ")":
                nivel -= 1
                if nivel == 0:
                    break
            k += 1
        return t[:i] + t[k + 1:]

    def troca_file(so):
        return lambda t: re.sub(r'file:\s*"[^"]*"', f'file: "{so}"', t, count=1)

    # O 'provides:' do modelo e multilinha (6 nomes), entao a substituicao e
    # por regex e nao por literal.
    def troca_provides(nomes):
        return lambda t: re.sub(r"provides:\s*\{[^}]*\}", f"provides: {{ {nomes} }}", t, count=1)

    print(f"--- controles negativos ({args.poc}) ---")

    # Sem o bloco, a PRIMEIRA classe do modelo que o parser encontra e a
    # ( FlightState ) do slot 'state:' do agente -- as formas irmas reduzem na
    # ordem do texto, e 'state:' vem antes de 'behavior:'.
    caso("cenario usa o modelo mas o bloco de plugin sumiu",
         variante("sem-loader", sem_bloco), args.binario,
         [r"nome de fabrica desconhecido", r"FlightState", r"PluginLoader"])

    caso("nome desconhecido em posicao de SLOT NOMEADO (o que segfaultava)",
         variante("slot-nomeado",
                  lambda t: t.replace("dynamicsModel: ( JSBSimModel", "dynamicsModel: ( NaoExiste", 1)),
         args.binario,
         [r"nome de fabrica desconhecido", r"NaoExiste"])

    caso(".so inexistente",
         variante("ausente", troca_file("libnaoexiste.so")), args.binario,
         [r"nao encontrei o modulo", r"procurei em", r"diretorio de trabalho"])

    caso("ponto de entrada invisivel ao dlsym (o 'static' do BTCPP:7248)",
         variante("nosym", troca_file(args.bad_nosym)), args.binario,
         [r"nao exporta 'mixr_plugin_v1'", r"MIXR_PLUGIN_DEFINE"])

    caso("ABI do contrato divergente",
         variante("abi", troca_file(args.bad_abi)), args.binario,
         [r"ABI do contrato", r"recompile o plugin"])

    # O 'provides:' tem de acompanhar, senao a assercao dele dispara ANTES e o
    # teste nunca chega na sonda de colisao (medido: era isso que acontecia).
    # A ordem do PluginRegistry esta certa -- provides e a checagem mais forte
    # e vem primeiro; quem estava errado era este caso de teste.
    caso("plugin sombreia um nome do framework",
         variante("collide",
                  lambda t: troca_provides("Aircraft")(troca_file(args.bad_collide)(t))),
         args.binario,
         [r"JA e construido pelo framework", r"Aircraft"])

    caso("'provides:' do cenario nao bate com o que a .so entrega",
         variante("provides", troca_provides("OutraCoisa")),
         args.binario,
         [r"nao bate com o que", r"OutraCoisa", r"FlightState"])

    if falhas:
        print(f"\ncontroles negativos: FALHOU ({len(falhas)})")
        for f in falhas:
            print(f"  - {f}")
        return 1
    print("controles negativos: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
