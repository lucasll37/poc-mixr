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
#   scripts/fetch_srtm.sh --brasil                # a caixa do Brasil (~1600 tiles, ~12 GB)
#   scripts/fetch_srtm.sh --brasil --dry-run      # so lista e soma o tamanho
#
# Idempotente: pula todo tile que ja esta em disco e integro (gzip -t).
# Interrompeu no meio? Rode de novo -- continua de onde parou.
#
# ATENCAO AO VOLUME: --brasil sao ~12 GB de .gz. Eles NAO devem ser
# versionados (ver .gitignore desta pasta) -- so os poucos tiles do cenario
# de demonstracao entram no git.
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DESTINO="$RAIZ/shared/data/terrain/srtm"
BASE="https://s3.amazonaws.com/elevation-tiles-prod/skadi"
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
      --bbox)    mapfile -t -O "${#TILES[@]}" TILES < <(tiles_da_caixa "$2" "$3" "$4" "$5"); shift 5 ;;
      -h|--help) sed -n '2,26p' "$0"; exit 0 ;;
      *)         TILES+=("$1"); shift ;;
   esac
done

if [ "${#TILES[@]}" -eq 0 ]; then
   echo "fetch_srtm.sh: nenhum tile pedido (use --help)" >&2; exit 2
fi

mkdir -p "$DESTINO"

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
   local http rc
   http=$(curl -sS -o "$alvo.parcial" -w '%{http_code}' \
             --retry 3 --retry-delay 2 --connect-timeout 20 --max-time 600 \
             "$base/${t:0:3}/$t.hgt.gz" 2>/dev/null)
   rc=$?
   if [ "$rc" -ne 0 ]; then
      rm -f "$alvo.parcial"; echo "ERRO $t (curl rc=$rc, http=${http:-?})"; return 0
   fi
   if [ "$http" = "404" ]; then rm -f "$alvo.parcial"; echo "ausente $t"; return 0; fi
   if [ "$http" != "200" ]; then rm -f "$alvo.parcial"; echo "ERRO $t (http=$http)"; return 0; fi
   if ! gzip -t "$alvo.parcial" 2>/dev/null; then rm -f "$alvo.parcial"; echo "corrompido $t"; return 0; fi
   mv "$alvo.parcial" "$alvo"; echo "ok $t"
}
export -f baixar_um

if [ "$DRY" -eq 1 ]; then
   printf '%s\n' "${TILES[@]}"
   echo "-- ${#TILES[@]} tiles pedidos (dry-run, nada baixado)" >&2
   exit 0
fi

echo "fetch_srtm.sh: ${#TILES[@]} tiles -> $DESTINO  (paralelo: $PARALELO)" >&2
printf '%s\n' "${TILES[@]}" \
  | xargs -P "$PARALELO" -I{} bash -c 'baixar_um "$@"' _ {} "$DESTINO" "$BASE" \
  | awk '/^ok /{o++} /^skip /{s++} /^ausente /{a++} /^corrompido /{c++}
         /^ERRO /{e++; print > "/dev/stderr"}
         (NR%100==0){printf "  ... %d processados\n", NR > "/dev/stderr"}
         END{printf "baixados=%d  ja_tinha=%d  sem_dado_no_espelho=%d  corrompidos=%d  ERROS=%d\n",
                    o,s,a,c,e > "/dev/stderr";
             exit (e>0 || c>0) ? 1 : 0}'
