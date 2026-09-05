#!/usr/bin/env bash
#
# O RNG de patrulha (shared/xrandom + domain::PatrolPlan::setHeadingJitter):
# a MESMA patrolMasterSeed tem de dar o MESMO estado com 1, 2 e 4 threads de
# tempo critico -- e DUAS sementes DIFERENTES tem de dar estados DIFERENTES,
# tambem em qualquer numero de threads (prova que quem muda o resultado e a
# semente, nao o agendamento entre threads).
#
# Duas fixtures HERMETICAS (modo 'plain' do make_fixture.py -- so PATROL,
# sem EVADE/RTB/SAFETY competindo, que sao ramos que nao leem PatrolPlan e so
# atrapalhariam isolar o efeito da semente), diferindo APENAS em
# patrolMasterSeed. patrolJitterHeading (a amplitude) ja vem do proprio
# cenario de producao -- sem ele, nenhuma semente mudaria nada.
#
#   uso: check_patrol_seed.sh <binario> <rotulo> <poc> [frames]
#
set -u

BIN="${1:?uso: check_patrol_seed.sh <binario> <rotulo> <poc> [frames]}"
ROTULO="${2:?falta o rotulo}"
POC="${3:?falta o nome da poc (single-thread ou multi-thread)}"
FRAMES="${4:-600}"

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# ONDE OS DUMPS DESTA EXECUCAO FICAM.
#
# ARMADILHA JA PAGA (nao redescobrir): isto era aritmetica de caminho sobre o
# BINARIO -- '$(dirname "$BIN")/../../../<rotulo>-{sufixo}' --, o que so dava
# num lugar sensato enquanto o binario estava a exatamente tres niveis de
# profundidade ('build/src/poc/<poc>/src/<poc>'). Quando o ./app virou o runner
# unico ('build/app/src/app', um nivel a menos), os tres '..' passaram a
# aterrissar na RAIZ DO REPOSITORIO e cada execucao largava uma pasta de lixo
# la -- nao versionada, mas suja e facil de commitar por engano.
#
# Agora sai de RAIZ, explicitamente, e vai para 'build/' junto com os outros
# artefatos de teste (build/tests-fixtures, tests-recordings, tests-messages):
# um lugar so, ja gitignorado, e que nao depende de onde o binario mora.
OUT="$RAIZ/build/tests-patrol-seed/${ROTULO}"
mkdir -p "$OUT" || exit 1

FIXTURES="$RAIZ/build/tests-fixtures"
mkdir -p "$FIXTURES" || exit 1

gera_fixture() {   # gera_fixture <semente> <arquivo-out>
   python3 "$RAIZ/tests/scenario/make_fixture.py" --poc "$POC" --mode plain \
      --patrol-seed "$1" --out "$2"
}

roda() {   # roda <cenario> <n-threads> <arquivo-saida>
   # Mesma armadilha ja documentada em check_determinism.sh: '$BIN | grep'
   # devolve o rc do grep, nao do binario -- um crash com pelo menos uma
   # linha 'frame=' impressa antes passaria batido. Binario roda para um
   # arquivo bruto e o rc e checado a parte, antes de filtrar.
   local raw rc
   raw="$(mktemp)"
   "$BIN" -f "$1" -threads "$2" -deterministic "$FRAMES" > "$raw" 2>/dev/null
   rc=$?
   grep '^frame=' "$raw" > "$3"
   rm -f "$raw"
   if [ "$rc" -ne 0 ]; then
      echo "  FALHA $BIN saiu com codigo $rc (cenario=$1 threads=$2)"
      return 1
   fi
}

CEN_A="$FIXTURES/$POC-patrolseed-a.edl.in"
CEN_B="$FIXTURES/$POC-patrolseed-b.edl.in"
gera_fixture 111111 "$CEN_A" || exit 1
gera_fixture 222222 "$CEN_B" || exit 1

for n in 1 2 4; do
   echo "  rodando $FRAMES frames (semente A) com numTcThreads=$n ..."
   roda "$CEN_A" "$n" "$OUT/a-$n.txt" || { echo "  FALHA execucao A/$n threads"; exit 1; }
   echo "  rodando $FRAMES frames (semente B) com numTcThreads=$n ..."
   roda "$CEN_B" "$n" "$OUT/b-$n.txt" || { echo "  FALHA execucao B/$n threads"; exit 1; }
done

for f in "$OUT"/a-*.txt "$OUT"/b-*.txt; do
   if [ ! -s "$f" ]; then echo "  FALHA $f saiu vazio"; exit 1; fi
done

fail=0

# 1) MESMA semente -> MESMO estado, em qualquer numero de threads T/C.
for pair in "a-1 a-2" "a-1 a-4" "b-1 b-2" "b-1 b-4"; do
   set -- $pair
   if diff -q "$OUT/$1.txt" "$OUT/$2.txt" > /dev/null; then
      echo "  OK   $1 == $2 (mesma semente reproduz entre threads)"
   else
      echo "  FALHA $1 != $2 -- mesma semente tinha de dar o mesmo estado"
      fail=1
   fi
done

# 2) SEMENTES DIFERENTES -> estado DIVERGE, em qualquer numero de threads.
#    (se isto der FALHA e a amplitude de jitter que zerou em algum lugar, nao
#    um bug de threading -- ver patrolJitterHeading no .edl de producao)
for n in 1 2 4; do
   if diff -q "$OUT/a-$n.txt" "$OUT/b-$n.txt" > /dev/null; then
      echo "  FALHA a-$n == b-$n -- sementes diferentes tinham de divergir"
      fail=1
   else
      echo "  OK   a-$n != b-$n (sementes diferentes, $n threads)"
   fi
done

if [ $fail -eq 0 ]; then
   echo "determinismo do RNG de patrulha ($ROTULO): OK"
else
   echo "determinismo do RNG de patrulha ($ROTULO): FALHOU"
fi
exit $fail
