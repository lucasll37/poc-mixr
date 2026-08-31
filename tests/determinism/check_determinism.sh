#!/usr/bin/env bash
#
# Determinismo: o MESMO estado com 1, 2 e 4 threads de tempo critico.
#
# E a logica que vivia inline em check-single-thread/check-multi-thread no
# Makefile, extraida para poder virar test() do Meson e, principalmente, para
# AFIRMAR o que antes so era impresso (a contagem de decisoes).
#
# Vale para as DUAS pocs, que e o ponto: elas diferem so em ONDE a decisao
# roda -- laco de background (SimAgent) contra fase 3 do frame
# (FlightAgentTC) -- e as duas tem de dar o mesmo estado nas tres
# configuracoes de threads.
#
# A fixture e HERMETICA (sem 'networks:') e gerada aqui mesmo: com o bloco de
# rede o processo ingere PDUs DIS de quem estiver na LAN, e duas execucoes
# iguais divergem -- o que este teste leria como falha de determinismo sem que
# houvesse nenhuma. Passar <poc> vazio usa o cenario de producao.
#
#   uso: check_determinism.sh <binario> <rotulo> [frames] [poc]
#
set -u

BIN="${1:?uso: check_determinism.sh <binario> <rotulo> [frames] [poc]}"
ROTULO="${2:?falta o rotulo}"
FRAMES="${3:-2000}"
POC="${4:-}"

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CENARIO=""
if [ -n "$POC" ]; then
   CENARIO="$RAIZ/build/tests-fixtures/$POC-intruder.epp.in"
   mkdir -p "$RAIZ/build/tests-recordings"
   python3 "$RAIZ/tests/scenario/make_fixture.py" --poc "$POC" --mode intruder \
      --out "$CENARIO" || exit 1
fi

OUT="$(dirname "$BIN")/../../../${ROTULO}-determinism"
mkdir -p "$OUT" || exit 1

args=()
[ -n "$CENARIO" ] && args+=(-f "$CENARIO")

MSGDIR="$RAIZ/build/tests-messages"

roda() {   # roda <n-threads> <arquivo-de-saida>
   "$BIN" "${args[@]}" -threads "$1" -deterministic "$FRAMES" 2>/dev/null \
      | grep '^frame=' > "$2"
   # O shared/xmsg grava por fora do stdout, e cada corrida trunca o mesmo
   # arquivo -- guardar uma copia por configuracao de thread e o que permite
   # comparar a saida de mensagens do mesmo jeito que se compara o dump.
   if [ -n "$POC" ] && [ -f "$MSGDIR/$POC-intruder.jsonl" ]; then
      cp "$MSGDIR/$POC-intruder.jsonl" "$OUT/messages-$1.jsonl"
   fi
}

for n in 1 2 4; do
   echo "  rodando $FRAMES frames com numTcThreads=$n ..."
   roda "$n" "$OUT/threads-$n.txt" || { echo "  FALHA execucao com $n threads"; exit 1; }
done
echo "  repetindo a execucao de 4 threads ..."
roda 4 "$OUT/threads-4b.txt" || { echo "  FALHA repeticao com 4 threads"; exit 1; }

for f in "$OUT"/threads-*.txt; do
   if [ ! -s "$f" ]; then echo "  FALHA $f saiu vazio"; exit 1; fi
done

fail=0

# ------------------------------------------------------------------------------
# 1) mesmo estado, byte a byte
#
# Quando falha, a mensagem diz QUAL aviao divergiu em QUAL frame e com qual
# rotulo de comportamento -- "os arquivos diferem" nao ajuda ninguem.
# ------------------------------------------------------------------------------
for pair in "threads-4 threads-4b" "threads-1 threads-2" "threads-1 threads-4"; do
   set -- $pair
   if diff -q "$OUT/$1.txt" "$OUT/$2.txt" > /dev/null; then
      echo "  OK   $1 == $2"
   else
      echo "  FALHA $1 != $2"
      linha_a=$(diff "$OUT/$1.txt" "$OUT/$2.txt" | grep '^< ' | head -1 | cut -c3-)
      linha_b=$(diff "$OUT/$1.txt" "$OUT/$2.txt" | grep '^> ' | head -1 | cut -c3-)
      frame=$(echo "$linha_a" | grep -o 'frame=[0-9]*')
      player=$(echo "$linha_a" | grep -o 'player=[a-z0-9]*')
      bt_a=$(echo "$linha_a" | grep -o 'bt=[A-Z?-]*')
      bt_b=$(echo "$linha_b" | grep -o 'bt=[A-Z?-]*')
      echo "        primeira divergencia: $frame $player  $1:$bt_a  $2:$bt_b"
      fail=1
   fi
done

# ------------------------------------------------------------------------------
# 2) a decisao esta amarrada ao frame -- UMA por frame, por aviao
#
# Antes este numero so era impresso. O dump byte a byte sozinho nao pega o
# problema: um desvio consistente entre as tres execucoes continua batendo.
#
# A assercao NAO e 'dec == frames', e sim que 'dec' avanca exatamente na
# mesma taxa que 'frame' entre dois dumps consecutivos. A diferenca importa:
# a multi-thread decide uma vez a mais na inicializacao da Station (medido:
# 601 decisoes em 600 frames, identico nas tres configuracoes de thread), o
# que e offset de partida e nao perda de vinculo com o frame. Comparar
# DELTAS mede a propriedade que interessa e ignora o offset.
# ------------------------------------------------------------------------------
for n in 1 2 4; do
   if ! awk -v cfg="$n" '
      {
         frame=""; player=""; dec="";
         for (i = 1; i <= NF; i++) {
            split($i, kv, "=");
            if (kv[1] == "frame")       frame = kv[2];
            else if (kv[1] == "player") player = kv[2];
            else if (kv[1] == "dec")    dec   = kv[2];
         }
         if (dec == "") {
            print "  FALHA dump sem campo dec= -- nao da para afirmar o vinculo com o frame";
            bad = 1; exit;
         }
         if (player in pframe) {
            df = frame - pframe[player];
            dd = dec   - pdec[player];
            if (df != dd) {
               printf "  FALHA %s: %d decisoes em %d frames (entre os frames %d e %d, %s threads)\n",
                      player, dd, df, pframe[player], frame, cfg;
               bad = 1;
            }
         }
         pframe[player] = frame; pdec[player] = dec;
      }
      END { exit bad ? 1 : 0 }
   ' "$OUT/threads-$n.txt"; then
      fail=1
   fi
done
[ $fail -eq 0 ] && echo "  OK   uma decisao por frame, por aviao, nas 3 configuracoes"

# ------------------------------------------------------------------------------
# 3) as MENSAGENS tambem tem de ser identicas
#
# O shared/xmsg NAO e desligado em -deterministic, ao contrario do xlog: tudo
# que ele emite carrega tempo simulado, nunca relogio de parede nem id de
# thread. Logo a saida tem de ser byte-identica nas tres configuracoes -- e
# isso e assercao, nao precaucao.
# ------------------------------------------------------------------------------
if [ -f "$OUT/messages-1.jsonl" ]; then
   for pair in "1 2" "1 4"; do
      set -- $pair
      if diff -q "$OUT/messages-$1.jsonl" "$OUT/messages-$2.jsonl" > /dev/null; then
         echo "  OK   mensagens: threads-$1 == threads-$2"
      else
         echo "  FALHA mensagens divergiram entre $1 e $2 threads"
         diff "$OUT/messages-$1.jsonl" "$OUT/messages-$2.jsonl" | head -4 | sed 's/^/        /'
         fail=1
      fi
   done
fi

if [ $fail -eq 0 ]; then
   echo "determinismo ($ROTULO): OK"
   exit 0
fi
echo "determinismo ($ROTULO): FALHOU"
exit 1
