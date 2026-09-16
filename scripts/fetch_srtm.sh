#!/usr/bin/env bash
#
# fetch_srtm.sh -- baixa tiles SRTM1 reais (.hgt.gz) para shared/data/terrain/srtm/
#
# FONTE: o espelho aberto "Terrain Tiles" da AWS Open Data
# (s3.amazonaws.com/elevation-tiles-prod/skadi), que serve o binario .hgt
# int16 big-endian 3601x3601 -- exatamente o formato que
# mixr::terrain::SrtmHgtFile exige -- SEM LOGIN. A fonte oficial da NASA
# passou a exigir Earthdata login; o CGIAR-CSI e aberto mas serve
# GeoTIFF/ASCII Grid, que precisaria de conversao. Ver o README da pasta.
#
# USO:
#   scripts/fetch_srtm.sh S23W042 S22W043 ...     # tiles nomeados
#   scripts/fetch_srtm.sh --bbox <lat0> <lat1> <lon0> <lon1>
#   scripts/fetch_srtm.sh --sudeste                # SP/RJ/MG/ES (~195 tiles)
#   scripts/fetch_srtm.sh --brasil                # a caixa do Brasil (~1600 tiles, ~12 GB)
#   scripts/fetch_srtm.sh --brasil --dry-run      # so sonda: quantidade + tamanho total
#
# Idempotente: pula todo tile que ja esta em disco e integro (gzip -t).
# Interrompeu no meio? Rode de novo -- continua de onde parou.
#
# ATENCAO AO VOLUME: --brasil sao ~12 GB de .gz. Eles NAO devem ser
# versionados (ver .gitignore desta pasta) -- so os poucos tiles do cenario
# de demonstracao entram no git.
#
# PROGRESSO: com stderr num terminal de verdade (nao redirecionado, TERM
# diferente de "dumb"), uma barra ao vivo mostra percentual, contagem por
# desfecho (baixados/ja_tinha/sem_dado/falhas), volume baixado nesta sessao,
# velocidade media e ETA -- redesenhada no maximo 1x/s, sem depender de
# nenhuma extensao especifica de awk (ver o comentario perto de
# humano_bytes() mais abaixo). Redirecionado pra arquivo/CI cai sozinho num
# modo silencioso por eventos -- uma linha a cada 100 tiles processados,
# sem sequencia ANSI nenhuma. NO_COLOR=1 desativa so' a cor (convencao
# https://no-color.org); a barra em si continua.
#
# SRTM_BASE (env, opcional) sobrepoe o espelho -- existe para os testes
# (tests/tools/test_fetch_srtm.py) apontarem para um servidor HTTP local em
# vez da AWS de verdade; uso normal nunca precisa dela. SRTM_PARALELO (env,
# opcional, default 12) controla quantos downloads/sondagens simultaneos.
set -euo pipefail
# Faz o ULTIMO estagio de um pipe ('xargs ... | while read; do ... done')
# rodar no shell ATUAL em vez de numa subshell -- sem isto, os contadores
# que o laco atualiza (ok, skip, bytes, ...) desapareceriam assim que o
# 'done' fechasse, e o resumo final nao teria o que reportar. Exige job
# control desligado, que ja e' o default de um script nao-interativo (sem
# 'set -m' em lugar nenhum daqui).
shopt -s lastpipe

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DESTINO="$RAIZ/shared/data/terrain/srtm"
BASE="${SRTM_BASE:-https://s3.amazonaws.com/elevation-tiles-prod/skadi}"
PARALELO="${SRTM_PARALELO:-12}"
DRY=0
TILES=()

nome_tile() {   # $1=lat (int, canto SW)  $2=lon (int, canto SW)
   local la=$1 lo=$2 ns ew
   if [ "$la" -ge 0 ]; then ns=N; else ns=S; la=$(( -la )); fi
   if [ "$lo" -ge 0 ]; then ew=E; else ew=W; lo=$(( -lo )); fi
   # o \n NAO e decorativo: sem ele o 'mapfile' do chamador recebe as 1600
   # linhas como UM elemento so, e o nome do "tile" vira a concatenacao
   # inteira (batido rodando: curl rc=27, "File name too long").
   printf '%s%02d%s%03d\n' "$ns" "$la" "$ew" "$lo"
}

tiles_da_caixa() {   # $1=lat0 $2=lat1 $3=lon0 $4=lon1  (graus, canto SW inclusivo)
   local la lo
   for (( la=$1; la<=$2; la++ )); do
      for (( lo=$3; lo<=$4; lo++ )); do nome_tile "$la" "$lo"; done
   done
}

while [ $# -gt 0 ]; do
   case "$1" in
      --dry-run) DRY=1; shift ;;
      # caixa do Brasil: Chui (-33,75) a Monte Caburai (+5,27);
      # Serra do Divisor (-73,99) a Ponta do Seixas (-34,79).
      --brasil)  mapfile -t -O "${#TILES[@]}" TILES < <(tiles_da_caixa -34 5 -74 -35); shift ;;
      # caixa do Sudeste (SP/RJ/MG/ES): aproximada, com ~1 grau de margem
      # sobre os extremos usuais dos quatro estados (~14S a ~26S de
      # latitude, ~39W a ~53W de longitude) -- nao e fronteira
      # administrativa exata, so o bastante para cobrir os quatro com folga.
      --sudeste) mapfile -t -O "${#TILES[@]}" TILES < <(tiles_da_caixa -26 -14 -53 -39); shift ;;
      --bbox)    mapfile -t -O "${#TILES[@]}" TILES < <(tiles_da_caixa "$2" "$3" "$4" "$5"); shift 5 ;;
      -h|--help) sed -n '2,30p' "$0"; exit 0 ;;
      *)         TILES+=("$1"); shift ;;
   esac
done

if [ "${#TILES[@]}" -eq 0 ]; then
   echo "fetch_srtm.sh: nenhum tile pedido (use --help)" >&2; exit 2
fi

mkdir -p "$DESTINO"

# --- terminal: barra ao vivo so' quando ha alguem olhando ----------------
# stderr redirecionado (arquivo, CI, 'tee') ou TERM=dumb cai no modo
# silencioso por eventos -- nunca sequencia ANSI num log.
if [ -t 2 ] && [ "${TERM:-dumb}" != "dumb" ]; then
   INTERATIVO=1
else
   INTERATIVO=0
fi

# Largura da barra adaptada ao terminal (quando da' pra perguntar) -- um
# numero fixo tanto faz pro calculo, so' muda a "resolucao" visual.
LARGURA_BARRA=20
if [ "$INTERATIVO" -eq 1 ] && command -v tput >/dev/null 2>&1; then
   COLUNAS=$(tput cols 2>/dev/null || echo 0)
   if [ "${COLUNAS:-0}" -ge 110 ]; then
      LARGURA_BARRA=30
   elif [ "${COLUNAS:-0}" -gt 0 ] && [ "${COLUNAS:-0}" -lt 80 ]; then
      LARGURA_BARRA=12
   fi
fi

# Cores -- desligadas fora do modo interativo ou com NO_COLOR setada
# (https://no-color.org). So' realcam contagem de sucesso/falha e a barra;
# os rotulos em texto continuam sempre presentes, com ou sem cor.
if [ "$INTERATIVO" -eq 1 ] && [ -z "${NO_COLOR:-}" ]; then
   C_RESET=$'\033[0m'
   C_VERDE=$'\033[32m'
   C_VERMELHO=$'\033[31m'
   C_CIANO=$'\033[36m'
else
   C_RESET=''; C_VERDE=''; C_VERMELHO=''; C_CIANO=''
fi
LIMPA_LINHA=$'\033[K'   # apaga do cursor ate o fim da linha (sobra de um redesenho anterior mais comprido)

if [ "$INTERATIVO" -eq 1 ]; then
   printf '\033[?25l' >&2   # esconde o cursor -- menos "piscante" com o \r a cada redesenho
   trap 'printf "\033[?25h" >&2' EXIT   # e' a UNICA coisa que o trap garante -- nao mexe no codigo de saida
fi

# Um tile so existe no espelho se houver dado ali -- oceano aberto costuma
# faltar, e isso NAO e erro: e a resposta honesta "nao ha terreno aqui".
baixar_um() {
   # ARMADILHA, ja paga: um unico 'local a="$1" b="$a/x"' NAO funciona --
   # bash expande TODAS as palavras da linha antes de fazer qualquer
   # atribuicao, entao 'b' via 'a' ainda vazio e o caminho saia "/x".
   # Uma declaracao por variavel, de proposito.
   local t="$1"
   local destino="$2"
   local base="$3"
   local alvo="$destino/$t.hgt.gz"

   if [ -f "$alvo" ] && gzip -t "$alvo" 2>/dev/null; then echo "skip $t"; return 0; fi

   # rc do curl e codigo HTTP sao coisas DIFERENTES, e confundir os dois faz
   # o script mentir: a primeira versao tratava falha de ESCRITA (rc 23) como
   # "tile ausente no espelho", escondendo um erro real atras de uma
   # explicacao plausivel. So 404 e ausencia de verdade.
   local saida http tam rc
   saida=$(curl -sS -o "$alvo.parcial" -w '%{http_code} %{size_download}' \
             --retry 3 --retry-delay 2 --connect-timeout 20 --max-time 600 \
             "$base/${t:0:3}/$t.hgt.gz" 2>/dev/null)
   rc=$?
   read -r http tam <<<"$saida"
   if [ "$rc" -ne 0 ]; then
      rm -f "$alvo.parcial"; echo "ERRO $t (curl rc=$rc, http=${http:-?})"; return 0
   fi
   if [ "$http" = "404" ]; then rm -f "$alvo.parcial"; echo "ausente $t"; return 0; fi
   if [ "$http" != "200" ]; then rm -f "$alvo.parcial"; echo "ERRO $t (http=$http)"; return 0; fi
   if ! gzip -t "$alvo.parcial" 2>/dev/null; then rm -f "$alvo.parcial"; echo "corrompido $t"; return 0; fi
   mv "$alvo.parcial" "$alvo"; echo "ok $t ${tam:-0}"
}
export -f baixar_um

# So sonda (HEAD -- sem body nenhum): usado pelo --dry-run para responder
# "quantos tiles, quantos bytes" sem baixar dado nenhum. Mesma distincao
# rc-do-curl x codigo-HTTP de 'baixar_um' acima, pelo mesmo motivo.
tamanho_um() {
   local t="$1"
   local base="$2"
   local resp rc http tam
   resp=$(curl -sS -I \
             --retry 3 --retry-delay 2 --connect-timeout 20 --max-time 60 \
             "$base/${t:0:3}/$t.hgt.gz" 2>/dev/null)
   rc=$?
   if [ "$rc" -ne 0 ]; then echo "ERRO $t (curl rc=$rc)"; return 0; fi
   http=$(printf '%s' "$resp" | head -n1 | awk '{print $2}')
   if [ "$http" = "404" ]; then echo "ausente $t"; return 0; fi
   if [ "$http" != "200" ]; then echo "ERRO $t (http=${http:-?})"; return 0; fi
   tam=$(printf '%s' "$resp" | tr -d '\r' | tr 'A-Z' 'a-z' | awk '/^content-length:/{print $2; exit}')
   if [ -z "$tam" ]; then echo "ERRO $t (sem content-length)"; return 0; fi
   echo "tam $tam $t"
}
export -f tamanho_um

# --- barra de progresso: puro bash, sem fork no laco quente --------------
#
# 'systime()' do awk foi cogitado pra cronometrar e descartado: e' extensao
# de gawk/mawk recente (confirmado funcionando no mawk 1.3.4 20240123 desta
# maquina), mas o mawk que o Ubuntu 22.04 publica (1.3.4 20200120) e
# ANTERIOR a essa extensao existir -- nao dava pra verificar so' no
# ambiente da vez e confiar. $SECONDS (builtin do bash, sem fork nenhum,
# resolucao de 1s -- de sobra pra um download de minutos) resolve isso sem
# depender de qual awk o sistema tem.
#
# humano_bytes()/duracao_legivel() fazem so' aritmetica INTEIRA (o bash nao
# tem ponto flutuante nativo) -- uma casa decimal e' tudo que uma barra de
# progresso precisa; o resumo final do --dry-run, que TEM teste conferindo
# o texto exato, continua usando um unico 'awk' pontual pra "%.2f GB" (essa
# chamada nao esta no laco quente, e divisao+printf sao awk puro, sem
# extensao nenhuma -- funciona em qualquer awk).

humano_bytes() {   # $1=bytes (inteiro) -> "N.N UN" (ou "N B" abaixo de 1024)
   local b=${1:-0} unidades=(B KB MB GB TB) i=0 escala=1 val10
   while [ "$b" -ge $((escala * 1024)) ] && [ "$i" -lt 4 ]; do
      escala=$((escala * 1024)); i=$((i + 1))
   done
   if [ "$i" -eq 0 ]; then
      printf '%d %s' "$b" "${unidades[$i]}"
   else
      val10=$((b * 10 / escala))
      printf '%d.%d %s' "$((val10 / 10))" "$((val10 % 10))" "${unidades[$i]}"
   fi
}

duracao_legivel() {   # $1=segundos (inteiro) -> "1h02m03s" / "2m03s" / "9s"
   local s=${1:-0} h m
   [ "$s" -lt 0 ] && s=0
   h=$((s / 3600)); m=$(((s % 3600) / 60)); s=$((s % 60))
   if [ "$h" -gt 0 ]; then printf '%dh%02dm%02ds' "$h" "$m" "$s"
   elif [ "$m" -gt 0 ]; then printf '%dm%02ds' "$m" "$s"
   else printf '%ds' "$s"
   fi
}

barra_progresso() {   # $1=celulas preenchidas (0..LARGURA_BARRA) -> "####----"
   local n=${1:-0} vazio preenchido='' vazias=''
   [ "$n" -lt 0 ] && n=0
   [ "$n" -gt "$LARGURA_BARRA" ] && n=$LARGURA_BARRA
   vazio=$((LARGURA_BARRA - n))
   printf -v preenchido '%*s' "$n" ''
   printf -v vazias '%*s' "$vazio" ''
   printf '%s%s' "${preenchido// /#}" "${vazias// /-}"
}

redesenhar_download() {   # processados total ok bytes ja_tinha sem_dado falhas elapsed
   local processados=$1 total=$2 ok=$3 bytes=$4 ja=$5 sem=$6 falhas=$7 elapsed=$8
   local pct=0 preenchido=0 restantes=$((total - processados)) taxa=0 eta='0s' cor_falhas=''
   if [ "$total" -gt 0 ]; then
      pct=$((processados * 100 / total))
      preenchido=$((processados * LARGURA_BARRA / total))
   fi
   [ "$elapsed" -gt 0 ] && taxa=$((bytes / elapsed))
   if [ "$processados" -gt 0 ] && [ "$restantes" -gt 0 ]; then
      eta=$(duracao_legivel $((elapsed * restantes / processados)))
   fi
   [ "$falhas" -gt 0 ] && cor_falhas="$C_VERMELHO"
   local linha
   linha=$(printf '%s[%s]%s %d/%d (%d%%)  %sok=%d%s(%s)  ja=%d  sem=%d  %sfalhas=%d%s  |  %s/s  ETA %s' \
      "$C_CIANO" "$(barra_progresso "$preenchido")" "$C_RESET" \
      "$processados" "$total" "$pct" \
      "$C_VERDE" "$ok" "$C_RESET" "$(humano_bytes "$bytes")" \
      "$ja" "$sem" \
      "$cor_falhas" "$falhas" "$C_RESET" \
      "$(humano_bytes "$taxa")" "$eta")
   printf '\r%s%s' "$linha" "$LIMPA_LINHA" >&2
}

redesenhar_sonda() {   # processados total disponiveis bytes ausentes erros elapsed
   local processados=$1 total=$2 disponiveis=$3 bytes=$4 ausentes=$5 erros=$6 elapsed=$7
   local pct=0 preenchido=0 restantes=$((total - processados)) eta='0s' cor_erros=''
   if [ "$total" -gt 0 ]; then
      pct=$((processados * 100 / total))
      preenchido=$((processados * LARGURA_BARRA / total))
   fi
   if [ "$processados" -gt 0 ] && [ "$restantes" -gt 0 ]; then
      eta=$(duracao_legivel $((elapsed * restantes / processados)))
   fi
   [ "$erros" -gt 0 ] && cor_erros="$C_VERMELHO"
   local linha
   linha=$(printf '%s[%s]%s %d/%d (%d%%)  %sdisponiveis=%d%s(~%s)  sem=%d  %serros=%d%s  |  ETA %s' \
      "$C_CIANO" "$(barra_progresso "$preenchido")" "$C_RESET" \
      "$processados" "$total" "$pct" \
      "$C_VERDE" "$disponiveis" "$C_RESET" "$(humano_bytes "$bytes")" \
      "$ausentes" \
      "$cor_erros" "$erros" "$C_RESET" \
      "$eta")
   printf '\r%s%s' "$linha" "$LIMPA_LINHA" >&2
}

if [ "$DRY" -eq 1 ]; then
   echo "fetch_srtm.sh: sondando tamanho de ${#TILES[@]} tiles -> $DESTINO  (paralelo: $PARALELO, dry-run)" >&2
   TOTAL=${#TILES[@]}
   disponiveis=0; bytes=0; ausentes=0; erros=0
   inicio=$SECONDS; ultimo_redesenho=-1
   printf '%s\n' "${TILES[@]}" \
     | xargs -P "$PARALELO" -I{} bash -c 'tamanho_um "$@"' _ {} "$BASE" \
     | while read -r campo1 campo2 campo3; do
          case "$campo1" in
             tam)     disponiveis=$((disponiveis + 1)); bytes=$((bytes + campo2)) ;;
             ausente) ausentes=$((ausentes + 1)) ;;
             ERRO)
                erros=$((erros + 1))
                [ "$INTERATIVO" -eq 1 ] && printf '\r%s\n' "$LIMPA_LINHA" >&2
                printf '%s%s %s %s%s\n' "$C_VERMELHO" "$campo1" "$campo2" "$campo3" "$C_RESET" >&2
                ;;
          esac
          processados=$((disponiveis + ausentes + erros))
          if [ "$INTERATIVO" -eq 1 ]; then
             if [ "$SECONDS" != "$ultimo_redesenho" ] || [ "$processados" -eq "$TOTAL" ]; then
                ultimo_redesenho=$SECONDS
                redesenhar_sonda "$processados" "$TOTAL" "$disponiveis" "$bytes" "$ausentes" "$erros" \
                                  "$((SECONDS - inicio))"
             fi
          elif (( processados % 100 == 0 )); then
             printf '  ... %d sondados (disponiveis=%d, ~%s)\n' "$processados" "$disponiveis" \
                    "$(humano_bytes "$bytes")" >&2
          fi
       done
   [ "$INTERATIVO" -eq 1 ] && printf '\n' >&2

   gb=$(awk -v b="$bytes" 'BEGIN{printf "%.2f", b/1073741824}')
   linha="${TOTAL} tiles pedidos -- tamanho total estimado: ${gb} GB (${bytes} bytes)"
   [ "$ausentes" -gt 0 ] && linha="$linha -- $ausentes sem dado no espelho (fora do total)"
   [ "$erros" -gt 0 ] && linha="$linha -- $erros com erro de sondagem"
   echo "$linha (dry-run, nada baixado)"
   exit $(( erros > 0 ? 1 : 0 ))
fi

echo "fetch_srtm.sh: ${#TILES[@]} tiles -> $DESTINO  (paralelo: $PARALELO)" >&2
TOTAL=${#TILES[@]}
ok=0; skip=0; ausente=0; corrompido=0; erro=0; bytes=0
inicio=$SECONDS; ultimo_redesenho=-1
printf '%s\n' "${TILES[@]}" \
  | xargs -P "$PARALELO" -I{} bash -c 'baixar_um "$@"' _ {} "$DESTINO" "$BASE" \
  | while read -r campo1 campo2 campo3; do
       case "$campo1" in
          ok)   ok=$((ok + 1)); bytes=$((bytes + ${campo3:-0})) ;;
          skip) skip=$((skip + 1)) ;;
          ausente) ausente=$((ausente + 1)) ;;
          corrompido)
             corrompido=$((corrompido + 1))
             [ "$INTERATIVO" -eq 1 ] && printf '\r%s\n' "$LIMPA_LINHA" >&2
             printf '%s%s %s%s\n' "$C_VERMELHO" "$campo1" "$campo2" "$C_RESET" >&2
             ;;
          ERRO)
             erro=$((erro + 1))
             [ "$INTERATIVO" -eq 1 ] && printf '\r%s\n' "$LIMPA_LINHA" >&2
             printf '%s%s %s %s%s\n' "$C_VERMELHO" "$campo1" "$campo2" "$campo3" "$C_RESET" >&2
             ;;
       esac
       processados=$((ok + skip + ausente + corrompido + erro))
       if [ "$INTERATIVO" -eq 1 ]; then
          if [ "$SECONDS" != "$ultimo_redesenho" ] || [ "$processados" -eq "$TOTAL" ]; then
             ultimo_redesenho=$SECONDS
             redesenhar_download "$processados" "$TOTAL" "$ok" "$bytes" "$skip" "$ausente" \
                                  "$((corrompido + erro))" "$((SECONDS - inicio))"
          fi
       elif (( processados % 100 == 0 )); then
          printf '  ... %d/%d processados (baixados=%d %s, ja_tinha=%d, falhas=%d)\n' \
                 "$processados" "$TOTAL" "$ok" "$(humano_bytes "$bytes")" "$skip" "$((corrompido + erro))" >&2
       fi
    done
[ "$INTERATIVO" -eq 1 ] && printf '\n' >&2

COR_FINAL="$C_VERDE"; ROTULO_FINAL="concluido"
if [ "$erro" -gt 0 ] || [ "$corrompido" -gt 0 ]; then
   COR_FINAL="$C_VERMELHO"; ROTULO_FINAL="concluido com falhas"
fi
printf '%s%s%s em %s -- baixados=%d (%s)  ja_tinha=%d  sem_dado_no_espelho=%d  corrompidos=%d  ERROS=%d\n' \
   "$COR_FINAL" "$ROTULO_FINAL" "$C_RESET" "$(duracao_legivel $((SECONDS - inicio)))" \
   "$ok" "$(humano_bytes "$bytes")" "$skip" "$ausente" "$corrompido" "$erro" >&2

exit $(( (erro > 0 || corrompido > 0) ? 1 : 0 ))
