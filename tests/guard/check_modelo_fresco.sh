#!/usr/bin/env bash
#
# Os .so DE PRODUCAO instalados sao mais novos que o fonte deles?
#
# Existe por causa de um efeito colateral da separacao, e ele e silencioso: o
# meson do host NAO tem aresta de dependencia ate o plugin (ele vem pronto de
# dist/, construido por 'make models', noutro projeto). Sem esta guarda, a
# suite inteira do host passa contra um plugin VELHO, e uma regressao no
# modelo fica invisivel -- as assercoes de cenario sao semanticas e grossas.
#
# Conferir mtime nao e conhecer o fonte: e conhecer um CAMINHO, exatamente
# como o 'searchPaths:' do cenario ja faz.
#
# Descobre os modelos por find sob models/player/ (mesma filosofia de
# check_modelo_estrutura.sh/check_colisao_fabrica.py: um modelo novo ja entra
# na checagem, sem editar este arquivo) -- EXCETO fixtures/ e template/, que
# nunca sao instalados em dist/lib/mixr-plugins/ do host (nao sao producao,
# ver models/README.md secao 2.4 e o docstring de check_colisao_fabrica.py).
#
# Os basenames de .so a checar de CADA modelo vem do PROPRIO './dist' local
# dele (populado por 'make build'/'make install-host' daquele projeto) -- nao
# de uma lista escrita a mao, que envelheceria em silencio (ex.: A4 produz
# DOIS .so, libflight.so + libflight_tc.so, e um modelo futuro pode produzir
# mais de um tambem).
set -u
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1

fail=0
checados=0

for modelo in models/player/*/; do
   modelo="${modelo%/}"
   nome="$(basename "$modelo")"
   case "$nome" in
      fixtures|template) continue ;;
   esac

   locais="$modelo/dist/lib/mixr-plugins"
   if [ ! -d "$locais" ] || [ -z "$(ls -A "$locais"/*.so 2>/dev/null)" ]; then
      echo "  FALHA $modelo nunca foi construido localmente -- rode 'make models' (ou 'make -C $modelo build')"
      fail=1
      continue
   fi

   for so_local in "$locais"/*.so; do
      base="$(basename "$so_local")"
      so_host="dist/lib/mixr-plugins/$base"
      checados=$((checados + 1))

      if [ ! -f "$so_host" ]; then
         echo "  FALHA $so_host nao existe -- rode 'make install'"
         fail=1
         continue
      fi

      novo="$(find "$modelo/src" "$modelo/include" "$modelo/configs" -type f -newer "$so_host" 2>/dev/null | head -5)"
      if [ -n "$novo" ]; then
         echo "  FALHA fonte de $modelo mais novo que $so_host:"
         echo "$novo" | sed 's/^/        /'
         echo "        rode 'make models' antes de testar o host"
         fail=1
      fi
   done
done

if [ "$fail" -ne 0 ]; then
   echo "modelo fresco: FALHOU"
   exit 1
fi

echo "  OK   $checados .so verificado(s), todos mais novos que o proprio fonte"
echo "modelo fresco: OK"
