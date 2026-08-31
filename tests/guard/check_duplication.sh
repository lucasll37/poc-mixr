#!/usr/bin/env bash
#
# As duas pocs sao gemeas: diferem SO no agente do UBF (onde a decisao roda).
# Em particular, domain/ e bt/ sao copias byte a byte uma da outra.
#
# Isso era convencao implicita -- 'make compare-single-multi' mostra as
# diferencas, mas nao falha, e ninguem le a saida toda. Aqui vira invariante
# verificado, e e ele que justifica os testes das camadas 1 e 2 compilarem
# contra UMA copia so: se as duas divergissem, metade do modelo ficaria sem
# teste em silencio.
#
set -u

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1

fail=0

compara() {   # compara <caminho relativo dentro da poc>
   local sub="$1"
   local a="src/single-thread/$sub"
   local b="src/multi-thread/$sub"

   if [ ! -e "$a" ] || [ ! -e "$b" ]; then
      echo "  FALHA $sub nao existe nas duas pocs"
      fail=1
      return
   fi

   if diff -r "$a" "$b" > /dev/null 2>&1; then
      echo "  OK   $sub identico nas duas pocs"
   else
      echo "  FALHA $sub divergiu entre as pocs:"
      diff -r "$a" "$b" | sed 's/^/        /' | head -20
      fail=1
   fi
}

compara include/domain
compara src/domain
compara include/bt
compara src/bt

if [ $fail -eq 0 ]; then
   echo "duplicacao: OK (domain/ e bt/ sao a mesma coisa nas duas pocs)"
   exit 0
fi
echo "duplicacao: FALHOU"
exit 1
