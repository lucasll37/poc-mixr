#!/usr/bin/env bash
#
# O HOST NAO PODE CONHECER O FONTE DO MODELO.
#
# Esta e a guarda do invariante que a separacao existe para criar. Ela nao
# afirma que o binario nao contem o modelo (isso ja era verdade antes da
# separacao -- medido: 'nm -C' no executavel dava 0 simbolos do modelo mesmo
# quando o .cpp dele era compilado junto). Ela afirma a coisa que a separacao
# de fato mudou: que o BUILD do host nao referencia, nao inclui e nao compila
# uma linha do modelo.
#
# E o que torna verificavel o cenario pedido: um terceiro entrega so o .so.
#
set -u
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1
fail=0

# 1) nenhum arquivo de build do host cita fonte do modelo
for mb in src/*/src/meson.build meson.build; do
   if grep -nE "'(domain|bt|ubf)/" "$mb" > /dev/null 2>&1; then
      echo "  FALHA $mb voltou a listar fonte do modelo:"
      grep -nE "'(domain|bt|ubf)/" "$mb" | sed 's/^/        /'
      fail=1
   fi
done
[ $fail -eq 0 ] && echo "  OK   nenhum meson.build do host lista fonte do modelo"

# 2) nenhum .cpp/.hpp do host inclui header do modelo
#    (xtrack/, xboard/ e xlog/ sao do SDK, nao do modelo -- por isso nao entram)
achados="$(grep -rnE '#include "(domain|bt|ubf|xnative)/' src/*/src src/*/include 2>/dev/null || true)"
if [ -n "$achados" ]; then
   echo "  FALHA codigo do host incluindo header do modelo:"
   echo "$achados" | sed 's/^/        /'
   fail=1
else
   echo "  OK   nenhum fonte do host inclui header do modelo"
fi

# 3) as arvores do modelo nao existem mais dentro das pocs
for d in domain bt ubf xnative; do
   for p in single-thread multi-thread; do
      if [ -e "src/$p/src/$d" ] || [ -e "src/$p/include/$d" ]; then
         echo "  FALHA src/$p ainda tem a arvore '$d' -- o modelo mora em models/flight"
         fail=1
      fi
   done
done
[ $fail -eq 0 ] && echo "  OK   domain/, bt/, ubf/ e xnative/ nao existem mais sob src/"

[ $fail -eq 0 ] && { echo "host opaco: OK"; exit 0; }
echo "host opaco: FALHOU"
exit 1
