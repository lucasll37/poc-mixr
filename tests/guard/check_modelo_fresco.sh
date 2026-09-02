#!/usr/bin/env bash
#
# O .so DO MODELO INSTALADO E MAIS NOVO QUE O FONTE DELE?
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
set -u
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1

SO="dist/lib/mixr-plugins/libflight.so"
FONTE="models/flight"

if [ ! -f "$SO" ]; then
   echo "  FALHA $SO nao existe -- rode 'make models'"
   echo "modelo fresco: FALHOU"; exit 1
fi

novo="$(find "$FONTE/src" "$FONTE/include" "$FONTE/configs" -type f -newer "$SO" 2>/dev/null | head -5)"
if [ -n "$novo" ]; then
   echo "  FALHA o fonte do modelo e mais novo que o .so instalado:"
   echo "$novo" | sed 's/^/        /'
   echo "        rode 'make models' antes de testar o host"
   echo "modelo fresco: FALHOU"; exit 1
fi

echo "  OK   $SO mais novo que todo o fonte de $FONTE"
echo "modelo fresco: OK"
