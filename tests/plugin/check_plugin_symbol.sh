#!/usr/bin/env bash
#
# A GUARDA DA ARMADILHA MAIS CLASSICA DE PLUGIN.
#
# contexts/BTCPP-CONTEXT.md:7248 registra o caso: a macro BT_REGISTER_NODES do
# BehaviorTree.CPP expande para uma funcao 'static', que nao aparece no .dynsym
# -- entao o dlsym nao a encontra e o sintoma aparece longe, como "tipo nao
# registrado", com o autor jurando que escreveu a funcao.
#
# Aqui isso vira erro de TESTE, em milissegundos, em vez de erro de runtime.
# Duas assercoes, e a segunda importa tanto quanto a primeira:
#
#   1. 'mixr_plugin_v1' existe e e 'T' (texto, GLOBAL);
#   2. NADA ALEM dele vaza. E o que confirma que gnu_symbol_visibility:'hidden'
#      esta valendo -- sem isso, dois plugins com classes de mesmo nome mangled
#      poderiam se interpor (os simbolos de metaObject/slottable sao GLOBAL
#      OBJECT).
#
# Simbolos WEAK ('W'/'V') sao ignorados: sao vague linkage (typeinfo, inline de
# template) que o compilador emite e o loader funde -- inclusive
# type_info::operator==, que e justamente o fallback por strcmp que faz o
# dynamic_cast atravessar a fronteira do .so.
#
set -u

SO="${1:?uso: check_plugin_symbol.sh <caminho.so>}"

if ! command -v nm > /dev/null 2>&1; then
   echo "nm nao encontrado -- pulando"
   exit 77   # SKIP, no protocolo do meson
fi

fail=0

if nm -D --defined-only "$SO" | grep -qE '^[0-9a-f]+ T mixr_plugin_v1$'; then
   echo "  OK   $SO exporta 'mixr_plugin_v1' como T (global)"
else
   echo "  FALHA $SO nao exporta 'mixr_plugin_v1' como simbolo global de texto."
   echo "        Esqueceu a macro MIXR_PLUGIN_DEFINE? Ou compilou com"
   echo "        -fvisibility=hidden sem o MIXR_PLUGIN_EXPORT?"
   echo "        (ver contexts/BTCPP-CONTEXT.md:7248)"
   nm -D --defined-only "$SO" | sed 's/^/        /' | head -20
   fail=1
fi

extras="$(nm -D --defined-only "$SO" | awk '$2=="T" || $2=="B" || $2=="D"' \
          | grep -v ' mixr_plugin_v1$' || true)"
if [ -z "$extras" ]; then
   echo "  OK   $SO nao exporta nada alem do ponto de entrada"
else
   echo "  FALHA $SO exporta simbolos fortes alem do ponto de entrada:"
   echo "$extras" | sed 's/^/        /'
   echo "        gnu_symbol_visibility:'hidden' esta valendo neste alvo?"
   fail=1
fi

[ $fail -eq 0 ] && { echo "simbolos do plugin: OK"; exit 0; }
echo "simbolos do plugin: FALHOU"
exit 1
