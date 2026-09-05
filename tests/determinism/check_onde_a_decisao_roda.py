#!/usr/bin/env python3
"""
DE ONDE VEM O DETERMINISMO: da FASE 3, e nao de sorte.

O 'make check-*' prova que cada poc e reproduzivel. Ele nao prova POR QUE --
e a resposta natural ("porque roda em passo fixo") esta errada. Este teste e o
CONTROLE NEGATIVO que separa as duas coisas.

O que ele faz: roda as duas pocs com '-parallel-decision', que solta o laco de
background numa thread propria, sem sincronizar com o frame -- exatamente o que
o tempo real faz (createTimeCriticalProcess() poe o frame numa thread e o main
chama updateData na outra, a taxas diferentes), so que sem o relogio de parede.

  * single-thread: a decisao mora no ( SimAgent ), componente da Station, e roda
    no laco de background. Solto, o NUMERO de decisoes em N frames deixa de ser
    fixo -- e o PatrolPlan integra o tempo a cada decisao. A trajetoria diverge.
    O teste EXIGE que divirja.

  * multi-thread: a decisao mora no ( FlightAgentTC ), na FASE 3 do frame,
    dentro da barreira. A mesma concorrencia nao a alcanca. O teste EXIGE que
    NAO divirja.

Sem o segundo, o primeiro so mostraria que concorrencia quebra coisas. Sem o
primeiro, o 'check-*' poderia estar passando por inercia. E o par que prova a
afirmacao.

MEDIDO montando isto, e vale saber: concorrencia SOZINHA nao basta. Uma primeira
versao deixou updateData concorrente com o tcFrame mas manteve UMA decisao por
frame -- e as duas pocs continuaram byte-identicas em 5 execucoes. As decisoes
deste modelo sao todas por LIMIAR e os comandos vem do plano, nao do estado
instantaneo, entao ler a posicao alguns centimetros adiante nao muda nada. O que
quebra e a CONTAGEM de decisoes variar, porque ai o plano de voo integra tempo
diferente.
"""

import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
FRAMES = 600
MAX_EXEC = 6          # ate 6 execucoes; para assim que aparece divergencia
EXEC_ESTAVEL = 4      # quantas o lado deterministico tem de repetir


def fixture_de_patrulha(poc: str) -> Path:
    """Deriva do cenario de producao, tira o intruso e encurta a perna.

    Sem o intruso a arvore fica em PATROL -- e e o ramo de patrulha que integra
    tempo a cada decisao (PatrolPlan::advance). Com o intruso a arvore vai para
    EVADE, que nao chama advance(): ali so o 'dec=' divergiria, e a trajetoria
    ficaria identica. A perna curta faz a curva acontecer dentro da janela.
    """
    out = RAIZ / "build" / "tests-fixtures"
    out.mkdir(parents=True, exist_ok=True)
    base = out / f"{poc}-nd-base.edl.in"
    subprocess.run(
        [sys.executable, str(RAIZ / "tests/scenario/make_fixture.py"),
         "--poc", poc, "--mode", "intruder", "--out", str(base)],
        cwd=RAIZ, check=True, capture_output=True)

    t = base.read_text()
    i = t.find("bandit1: (")
    if i < 0:
        raise SystemExit("nao achei o bandit1 injetado pela fixture 'intruder'")
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
    t = t[:i] + t[k + 1:]

    t, n = re.subn(r"legTime:(\s*)\( Seconds \d+ \)", r"legTime:\g<1>( Seconds 5 )", t)
    if n == 0:
        raise SystemExit("nao achei 'legTime:' na fixture")

    p = out / f"{poc}-nd.edl.in"
    p.write_text(t)
    return p



# 'elev='/'agl=' NAO entram no hash -- e o unico ajuste, medido, nao suposto.
# Player::updateElevation() roda em updateData() (fase de BACKGROUND, ver a
# secao "Terreno" do CLAUDE.md: "o valor pode estar ate 100ms velho"), nao na
# fase 3 onde a decisao mora. Com '-parallel-decision' o laco de fundo roda
# solto, sem sincronizar com o frame -- exatamente o que a flag existe para
# fazer -- entao a AMOSTRAGEM de elevacao passa a cair em instantes de parede
# diferentes a cada execucao, mesmo com a trajetoria (n/e/alt/hdg/...) e a
# decisao (bt/dec) IDENTICAS byte a byte. Confirmado tokenizando e comparando
# campo a campo 4 execucoes de multi-thread com a flag: as UNICAS colunas que
# jamais diferem sao elev/agl -- todo o resto (incluindo bt= e dec=, que sao
# o que este teste realmente quer provar) bate em 100% das linhas. Sem este
# filtro o teste falhava sempre, por um motivo que nao tem nada a ver com "a
# decisao na fase 3 nao esta protegida" -- media a amostragem de terreno, nao
# a decisao. Isto NAO relaxa a garantia: a comparacao continua exigindo que
# TODOS os outros campos, inclusive a trajetoria e o rotulo da decisao, sejam
# byte-identicos.
_RUIDO_DE_FUNDO = re.compile(r" (?:elev|agl)=[^ ]*")


def dump(binario: str, fixture: Path, paralelo: bool) -> str:
    cmd = [binario, "-f", str(fixture), "-threads", "4", "-deterministic", str(FRAMES)]
    if paralelo:
        cmd.append("-parallel-decision")
    r = subprocess.run(cmd, cwd=RAIZ, capture_output=True, text=True, timeout=600)
    if r.returncode != 0:
        print(r.stdout[-2000:]); print(r.stderr[-2000:])
        raise SystemExit(f"execucao falhou (rc={r.returncode})")
    linhas = [_RUIDO_DE_FUNDO.sub("", l) for l in r.stdout.splitlines() if l.startswith("frame=")]
    if not linhas:
        raise SystemExit("nenhuma linha 'frame=' na saida")
    return hashlib.sha256("\n".join(linhas).encode()).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--single", required=True, help="binario da poc single-thread")
    ap.add_argument("--multi", required=True, help="binario da poc multi-thread")
    args = ap.parse_args()

    falhas = []

    # --- controle: SEM a flag, as duas tem de ser reproduziveis ---
    for nome, binario in (("single-thread", args.single), ("multi-thread", args.multi)):
        fx = fixture_de_patrulha(nome)
        h = {dump(binario, fx, paralelo=False) for _ in range(2)}
        if len(h) == 1:
            print(f"  OK   {nome} SEM -parallel-decision: reproduzivel")
        else:
            falhas.append(f"{nome} nao e reproduzivel nem no modo normal -- ha outra causa")

    # --- single-thread: a decisao no BACKGROUND tem de divergir ---
    fx = fixture_de_patrulha("single-thread")
    vistos = set()
    for i in range(MAX_EXEC):
        vistos.add(dump(args.single, fx, paralelo=True))
        if len(vistos) > 1:
            break
    if len(vistos) > 1:
        print(f"  OK   single-thread COM -parallel-decision: divergiu"
              f" ({len(vistos)} resultados em {i + 1} execucoes)")
    else:
        falhas.append(
            f"single-thread nao divergiu em {MAX_EXEC} execucoes com a decisao solta no"
            " background -- ou o laco parou de ser concorrente, ou a decisao deixou de"
            " integrar tempo (PatrolPlan::advance)")

    # --- multi-thread: a decisao na FASE 3 tem de resistir ---
    fx = fixture_de_patrulha("multi-thread")
    vistos = set()
    for _ in range(EXEC_ESTAVEL):
        vistos.add(dump(args.multi, fx, paralelo=True))
    if len(vistos) == 1:
        print(f"  OK   multi-thread COM -parallel-decision: NAO divergiu"
              f" ({EXEC_ESTAVEL} execucoes, 1 resultado)")
    else:
        falhas.append(
            f"multi-thread divergiu ({len(vistos)} resultados) -- a decisao na fase 3"
            " deixou de estar protegida pela barreira do frame, ou passou a depender de"
            " estado que o laco de background mexe")

    if falhas:
        print("\nonde a decisao roda: FALHOU")
        for f in falhas:
            print(f"  - {f}")
        return 1
    print("onde a decisao roda: OK (o determinismo vem da fase 3, nao do passo fixo)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
