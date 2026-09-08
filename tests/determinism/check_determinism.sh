#!/usr/bin/env bash
#
# Determinismo: o MESMO estado com 1, 2 e 4 threads de tempo critico.
#
# E a logica que vivia inline nos extintos check-single-thread/check-multi-thread
# no Makefile, extraida para poder virar test() do Meson e, principalmente,
# para AFIRMAR o que antes so era impresso (a contagem de decisoes).
#
# Generico para qualquer poc baseada em ( FlightAgentTC ) -- todo agente
# deste repositorio decide na fase 3 do frame de tempo critico, e tem de dar
# o mesmo estado nas tres configuracoes de threads.
#
# A fixture e HERMETICA (sem 'networks:') e gerada aqui mesmo: com o bloco de
# rede o processo ingere PDUs DIS de quem estiver na LAN, e duas execucoes
# iguais divergem -- o que este teste leria como falha de determinismo sem que
# houvesse nenhuma. Passar <poc> vazio usa o cenario de producao.
#
#   uso: check_determinism.sh <binario> <rotulo> [frames] [poc] [arquivo-de-cenario]
#
# O <binario> hoje e sempre o ./app -- as pocs nao tem executavel proprio (ver
# src/poc/meson.build). Quando ha <poc>, a fixture gerada aqui entra por '-f'
# e ja diz tudo; quando nao ha (cenario que ja e hermetico de fabrica, como o
# de built-in_mixr_1/full-systems-nav), e preciso dizer ao runner QUAL
# arquivo carregar, e e isso que <arquivo-de-cenario> faz (tambem por '-f' --
# nao ha mais catalogo estatico de cenarios, so caminho de arquivo) -- sem
# ele o ./app recusaria de cara (uma das duas opcoes e obrigatoria).
#
set -u

BIN="${1:?uso: check_determinism.sh <binario> <rotulo> [frames] [poc]}"
ROTULO="${2:?falta o rotulo}"
FRAMES="${3:-2000}"
POC="${4:-}"
ARQUIVO="${5:-}"

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CENARIO=""
if [ -n "$POC" ]; then
   CENARIO="$RAIZ/build/tests-fixtures/$POC-intruder.edl.in"
   mkdir -p "$RAIZ/build/tests-recordings"
   python3 "$RAIZ/tests/scenario/make_fixture.py" --poc "$POC" --mode intruder \
      --out "$CENARIO" || exit 1
elif [ -n "$ARQUIVO" ]; then
   CENARIO="$ARQUIVO"
fi

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
OUT="$RAIZ/build/tests-determinism/${ROTULO}"
mkdir -p "$OUT" || exit 1

args=()
[ -n "$CENARIO" ] && args+=(-f "$CENARIO")

MSGDIR="$RAIZ/build/tests-messages"

roda() {   # roda <n-threads> <arquivo-de-saida>
   # ARMADILHA CONFIRMADA (nao redescobrir): '$BIN | grep > arquivo' devolve o
   # rc do GREP, nao do binario -- um crash do $BIN (sinal, abort) que ainda
   # tenha impresso ALGUMA linha 'frame=' antes de morrer passava batido
   # aqui, e o 'roda ... || exit 1' do chamador nunca disparava. Reproduzido
   # com um binario fake que imprime uma linha e sai com rc=137: a funcao
   # devolvia 0. Por isso o binario roda para um arquivo bruto e o rc dele e
   # checado A PARTE, antes de filtrar.
   local raw errf rc
   raw="$(mktemp)"
   errf="$(mktemp)"
   "$BIN" "${args[@]}" -threads "$1" -deterministic "$FRAMES" > "$raw" 2>"$errf"
   rc=$?
   grep '^frame=' "$raw" > "$2"
   rm -f "$raw"
   if [ "$rc" -ne 0 ]; then
      echo "  FALHA $BIN saiu com codigo $rc (threads=$1)"
      # ARMADILHA CONFIRMADA (nao redescobrir): stderr era jogado fora
      # (2>/dev/null) e a unica pista que sobrava do motivo real da falha era
      # o codigo de saida -- justamente no caso em que mais se precisa dela
      # (depuracao de CI). Guardado ao lado do dump para nao se perder e
      # ecoado aqui, ja que e a informacao que mais importa nesse instante.
      if [ -s "$errf" ]; then
         echo "  ultimas linhas do stderr:"
         tail -5 "$errf" | sed 's/^/        /'
      fi
      cp "$errf" "$OUT/$(basename "$2" .txt)-stderr.txt" 2>/dev/null
      rm -f "$errf"
      return 1
   fi
   rm -f "$errf"
   # O libs/xmsg grava por fora do stdout, e cada corrida trunca o mesmo
   # arquivo -- guardar uma copia por CORRIDA (nao por numero de threads: a
   # repeticao de 4 threads tambem passa "4" aqui, e as duas corridas
   # colidiriam na mesma chave) e o que permite comparar a saida de mensagens
   # do mesmo jeito que se compara o dump, inclusive a repeticao contra si
   # mesma. A chave sai do rotulo do proprio dump ($2), nunca do numero de
   # threads.
   if [ -n "$POC" ] && [ -f "$MSGDIR/$POC-intruder.jsonl" ]; then
      cp "$MSGDIR/$POC-intruder.jsonl" "$OUT/messages-$(basename "$2" .txt | sed 's/^threads-//').jsonl"
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
# o agente decide uma vez a mais na inicializacao da Station (medido:
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
# O libs/xmsg NAO e desligado em -deterministic, ao contrario do xlog: tudo
# que ele emite carrega tempo simulado, nunca relogio de parede nem id de
# thread. Logo a saida tem de ser byte-identica nas tres configuracoes -- e
# isso e assercao, nao precaucao.
# ------------------------------------------------------------------------------
if [ -f "$OUT/messages-1.jsonl" ]; then
   for pair in "1 2" "1 4" "4 4b"; do
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
