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

# A fiacao da carga dinamica de modelos (shared/xplugin) e IDENTICA nas duas
# pocs, e continuar identica e o que mantem o 'make compare-single-multi'
# mostrando so o agente do UBF.
#
# Estes quatro ja eram byte a byte antes de existir plugin nenhum -- entram
# aqui porque agora sao TOCADOS por um assunto novo, e uma edicao aplicada
# numa poc so passaria despercebida: o compare-single-multi mostra, mas nao
# falha, e ninguem le a saida toda.
compara include/mixr_factory.hpp
compara src/mixr_factory.cpp
compara src/app/StationBuilder.cpp
compara src/app/MetaObjectReport.cpp

# O modelo virou plugin, e isso tornou identicos mais tres arquivos que antes
# divergiam ou nao eram guardados:
#
#   include/ubf   ja era byte a byte, so nunca tinha sido guardado;
#   plugin.cpp    a fronteira C do modelo -- o que diverge entre as pocs
#                 (o FlightAgentTC) mora nas listas de xnative/factory.cpp;
#   Deterministic/StatusReport  pararam de incluir header do modelo e passaram
#                 a ler o quadro (shared/xboard), entao o 'dec=' e o 'thr='
#                 deixaram de vir de lugares diferentes em cada poc.
compara include/ubf
compara src/ubf
compara src/plugin.cpp
compara src/app/DeterministicDump.cpp
compara src/app/StatusReport.cpp

if [ $fail -eq 0 ]; then
   echo "duplicacao: OK (domain/, bt/ e a fiacao de plugin sao a mesma coisa nas duas pocs)"
   exit 0
fi
echo "duplicacao: FALHOU"
exit 1
