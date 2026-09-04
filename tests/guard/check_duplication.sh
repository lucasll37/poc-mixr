#!/usr/bin/env bash
#
# As pocs de src/poc/ sao gemeas. Depois que o MODELO saiu para models/flight,
# o que sobra em src/poc/<poc>/ e so a camada de aplicacao -- e ela e byte a
# byte identica entre elas, com quatro excecoes conhecidas (main.cpp,
# ScenarioTemplate.cpp, meson.build e o cenario).
#
# Isso era convencao implicita -- 'make compare-single-multi' mostra as
# diferencas, mas nao falha, e ninguem le a saida toda. Aqui vira invariante
# verificado, e e ele que justifica os testes das camadas 1 e 2 compilarem
# contra UMA copia so: se as duas divergissem, metade do modelo ficaria sem
# teste em silencio.
#
# AS GEMEAS SAO DESCOBERTAS POR 'find', nao por lista fixa: cada poc nova
# (python-flight, onnx-policy, ...) nasce cobrada sem editar este arquivo. E a
# mesma licao ja registrada no cabecalho de check_host_opaco.sh e de
# check_modelo_estrutura.sh -- lista fixa envelhece em silencio. A referencia
# continua sendo a single-thread; bandit-dis fica de fora porque nao e gemea
# (um player so, sem UBF, sem Fleet -- ver o README dela).
#
set -u

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1

fail=0

# As gemeas: toda pasta de src/poc/ que tem a camada de FROTA (src/app/Fleet.cpp),
# menos a referencia. O criterio nao e "tem src/app/": a bandit-dis tem, mas
# nao e gemea -- ela pilota UM player, sem UBF e sem frota, e por isso nao
# carrega Fleet/DeterministicDump/StatusReport. Quem tem Fleet.cpp tem os
# quatro falcons e a mesma aplicacao inteira.
REF="single-thread"
GEMEAS="$(find src/poc -mindepth 4 -maxdepth 4 -type f -path 'src/poc/*/src/app/Fleet.cpp' \
            | sed 's|^src/poc/||; s|/src/app/Fleet.cpp$||' | grep -v "^$REF$" | sort)"

if [ -z "$GEMEAS" ]; then
   echo "  FALHA nenhuma poc gemea encontrada sob src/poc/ -- o find quebrou?"
   exit 1
fi

compara() {   # compara <caminho relativo dentro da poc>
   local sub="$1"
   local a="src/poc/$REF/$sub"
   local poc b

   if [ ! -e "$a" ]; then
      echo "  FALHA $sub nao existe na poc de referencia ($REF)"
      fail=1
      return
   fi

   for poc in $GEMEAS; do
      b="src/poc/$poc/$sub"
      if [ ! -e "$b" ]; then
         echo "  FALHA $sub nao existe em $poc"
         fail=1
         continue
      fi
      if diff -r "$a" "$b" > /dev/null 2>&1; then
         echo "  OK   $sub identico em $poc"
      else
         echo "  FALHA $sub divergiu entre $REF e $poc:"
         diff -r "$a" "$b" | sed 's/^/        /' | head -20
         fail=1
      fi
   done
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
compara src/app/Shutdown.cpp

if [ $fail -eq 0 ]; then
   echo "duplicacao: OK (a camada de aplicacao e a mesma em $REF e em: $(echo $GEMEAS | tr '\n' ' '))"
   exit 0
fi
echo "duplicacao: FALHOU"
exit 1
