#!/usr/bin/env bash
#
# As duas pocs sao gemeas. Depois que o MODELO saiu para models/flight,
# o que sobra em src/<poc>/ e so a camada de aplicacao -- e ela e byte a byte
# identica entre as duas, com quatro excecoes conhecidas (main.cpp,
# ScenarioTemplate.cpp, meson.build e o cenario).
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
   local a="src/poc/single-thread/$sub"
   local b="src/poc/multi-thread/$sub"

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

# O que sobrou de duplicado entre as gemeas depois que o MODELO saiu para
# models/flight: so a camada de aplicacao. domain/, bt/ e ubf/ nao
# aparecem mais aqui porque nao existem mais em duas copias -- a duplicacao
# foi dissolvida por construcao, e nao mais sustentada por este teste.
compara include/app
compara include/mixr_factory.hpp
compara src/mixr_factory.cpp
compara src/app/StationBuilder.cpp
compara src/app/MetaObjectReport.cpp
compara src/app/DeterministicDump.cpp
compara src/app/StatusReport.cpp
compara src/app/Options.cpp
compara src/app/TerrainData.cpp
compara src/app/Fleet.cpp
compara src/app/DeterministicRun.cpp
compara src/app/RealTimeRun.cpp

if [ $fail -eq 0 ]; then
   echo "duplicacao: OK (a camada de aplicacao e a mesma nas duas pocs)"
   exit 0
fi
echo "duplicacao: FALHOU"
exit 1
