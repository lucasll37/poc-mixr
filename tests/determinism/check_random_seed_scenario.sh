#!/usr/bin/env bash
#
# O RNG de acrobacia (libs/xrandom + domain::AerobaticPlan, ver sandbox/
# A4-6DOF-RANDOM/README.md): a MESMA patrolMasterSeed tem de dar o MESMO
# estado com 1, 2 e 4 threads de tempo critico -- e DUAS sementes DIFERENTES
# tem de dar estados DIFERENTES, tambem em qualquer numero de threads (prova
# que quem muda o resultado e a semente, nao o agendamento entre threads).
#
# Mesma propriedade que check_patrol_seed.sh ja prova para
# domain::PatrolPlan/'flight' -- este script existe A PARTE porque
# check_patrol_seed.sh depende de tests/scenario/make_fixture.py, que so'
# conhece cenarios sob src/poc/<poc>/configs/scenario.edl.in (frota
# falcon1..4, modos 'plain'/'intruder'/'lowfuel'). sandbox/A4-6DOF-RANDOM
# nao e' uma dessas pocs (8 aeronaves a4_1..a4_8, sem EVADE/RTB/SAFETY
# nenhum na arvore -- so' SlowRoll/Navigate), entao nao ha nada a "remover"
# do cenario para isola-lo: ele ja e' hermetico e ja so' testa a acrobacia.
# Este script recebe o ARQUIVO de cenario direto, sem passar por
# make_fixture.py -- so troca 'patrolMasterSeed:' com o MESMO regex que
# make_fixture.py usa (re.sub sobre 'patrolMasterSeed:\s*\d+'), assim
# qualquer cenario com esse slot (nao so' A4-6DOF-RANDOM) pode reusar isto.
#
#   uso: check_random_seed_scenario.sh <binario> <rotulo> <arquivo-de-cenario> \
#           [frames] [semente-a] [semente-b]
#
# FRAMES default (8000 = 160s simulados) e' deliberado: o intervalo entre
# giros e' sorteado em [slowRollMinInterval, slowRollMaxInterval] -- 45-120s
# no cenario de referencia -- e 8000 frames garante que TODAS as aeronaves
# ja tenham rolado ao menos uma vez ate o fim da janela, mesmo a que sorteou
# o pior caso (120s = 6000 frames a 50 Hz).
set -u

BIN="${1:?uso: check_random_seed_scenario.sh <binario> <rotulo> <arquivo-de-cenario> [frames] [semente-a] [semente-b]}"
ROTULO="${2:?falta o rotulo}"
CENARIO_ORIGINAL="${3:?falta o arquivo de cenario}"
FRAMES="${4:-8000}"
SEMENTE_A="${5:-111111}"
SEMENTE_B="${6:-222222}"

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [ ! -f "$CENARIO_ORIGINAL" ]; then
   echo "  FALHA cenario nao encontrado: $CENARIO_ORIGINAL"
   exit 1
fi

# Mesmo motivo de path ja documentado em check_determinism.sh/
# check_patrol_seed.sh (armadilha "Testes automatizados" do CLAUDE.md): sai
# de RAIZ, explicitamente, nunca de aritmetica de caminho sobre o binario.
OUT="$RAIZ/build/tests-random-seed/${ROTULO}"
mkdir -p "$OUT" || exit 1

# ARMADILHA (a MESMA ja documentada em check_determinism.sh, repetida aqui
# porque este script tambem monta '-f'/'-folder' na mao): '-f <arquivo>'
# SEMPRE assume a frota falcon1..4 (app::adHocScenario()) -- sandbox/
# A4-6DOF-RANDOM e' a4_1..a4_8, e o binario morreria com "player 'falcon1'
# nao encontrado!" antes do primeiro frame. A saida e' '-folder <pasta>
# -scenario <nome>', que descobre a frota em runtime
# (app::discoverFleet()) -- mas isso exige o layout <pasta>/<nome>/
# configs/<arquivo>, entao cada semente ganha sua PROPRIA pasta de cenario
# dentro de build/tests-fixtures/, com o MESMO nome-base do arquivo
# original (discoverFolderScenarios() so exige um .edl/.edl.in por
# 'configs/', o nome em si e livre).
FIXTURES="$RAIZ/build/tests-fixtures/${ROTULO}-randomseed"
NOME_BASE="$(basename "$CENARIO_ORIGINAL")"
mkdir -p "$FIXTURES/seed-a/configs" "$FIXTURES/seed-b/configs" || exit 1

gera_fixture() {   # gera_fixture <semente> <arquivo-out>
   python3 - "$CENARIO_ORIGINAL" "$1" "$2" <<'PYEOF'
import re
import sys

origem, semente, saida = sys.argv[1], sys.argv[2], sys.argv[3]
with open(origem, encoding="utf-8") as f:
   texto = f.read()

texto, n = re.subn(r"(patrolMasterSeed:\s*)\d+", rf"\g<1>{semente}", texto)
if n == 0:
   raise SystemExit(f"nenhum slot patrolMasterSeed encontrado em {origem}")

with open(saida, "w", encoding="utf-8") as f:
   f.write(texto)
PYEOF
}

roda() {   # roda <nome-subpasta> <n-threads> <arquivo-saida>
   # Mesma armadilha ja documentada em check_determinism.sh: '$BIN | grep'
   # devolve o rc do grep, nao do binario -- um crash com pelo menos uma
   # linha 'frame=' impressa antes passaria batido. Binario roda para um
   # arquivo bruto e o rc e checado a parte, antes de filtrar.
   local raw rc
   raw="$(mktemp)"
   "$BIN" -folder "$FIXTURES" -scenario "$1" -threads "$2" -deterministic "$FRAMES" > "$raw" 2>/dev/null
   rc=$?
   grep '^frame=' "$raw" > "$3"
   rm -f "$raw"
   if [ "$rc" -ne 0 ]; then
      echo "  FALHA $BIN saiu com codigo $rc (cenario=$1 threads=$2)"
      return 1
   fi
}

gera_fixture "$SEMENTE_A" "$FIXTURES/seed-a/configs/$NOME_BASE" || exit 1
gera_fixture "$SEMENTE_B" "$FIXTURES/seed-b/configs/$NOME_BASE" || exit 1

for n in 1 2 4; do
   echo "  rodando $FRAMES frames (semente A=$SEMENTE_A) com numTcThreads=$n ..."
   roda "seed-a" "$n" "$OUT/a-$n.txt" || { echo "  FALHA execucao A/$n threads"; exit 1; }
   echo "  rodando $FRAMES frames (semente B=$SEMENTE_B) com numTcThreads=$n ..."
   roda "seed-b" "$n" "$OUT/b-$n.txt" || { echo "  FALHA execucao B/$n threads"; exit 1; }
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
#    (se isto der FALHA e a amplitude/intervalo do sorteio que nao esta
#    variando de fato entre sementes, nao um bug de threading)
for n in 1 2 4; do
   if diff -q "$OUT/a-$n.txt" "$OUT/b-$n.txt" > /dev/null; then
      echo "  FALHA a-$n == b-$n -- sementes diferentes tinham de divergir"
      fail=1
   else
      echo "  OK   a-$n != b-$n (sementes diferentes, $n threads)"
   fi
done

if [ $fail -eq 0 ]; then
   echo "determinismo do RNG de acrobacia ($ROTULO): OK"
else
   echo "determinismo do RNG de acrobacia ($ROTULO): FALHOU"
fi
exit $fail
