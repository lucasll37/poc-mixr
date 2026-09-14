#!/usr/bin/env bash
#
# Os cenarios das pocs de producao (flight, python-flight, onnx-policy)
# declaram falcon1..falcon4, cada um um bloco '( Aircraft ... )'. falcon1 carrega
# comentario explicando cada slot; falcon2/3/4 sao a mesma estrutura,
# terse, sem comentario -- uma forma de duplicacao que nunca bate byte a
# byte (as 4 pistas tem posicao/altitude/velocidade calibradas por aviao),
# entao o que vale garantir e a FORMA (mesmos slots, mesma fabrica), nao o
# valor. Ver skeleton_diff.py para o metodo e o "porque" completo.
#
set -u

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1

fail=0

# Descoberto por glob, nao por lista fixa: poc nova com os quatro falcons
# nasce cobrada sem editar este arquivo (a bandit fica de fora sozinha --
# o cenario dela e um scenario.edl, sem template e sem falcon nenhum).
# Por 'find', nao por glob de um nivel: agrupar as pocs de DIS em
# src/poc/dis/<poc>/ empurrou tres cenarios um nivel para baixo, e o glob
# 'src/poc/*/configs/...' deixaria de ve-los -- guarda verde cobrindo menos.
# Achado por auditoria (revisao completa do repositorio): sem esta checagem,
# um 'find' que voltasse vazio (renomeacao de pasta, cenarios movidos pra
# fora de configs/scenario.edl.in) faria o loop abaixo simplesmente nao
# rodar, 'fail' continuar 0 e o script imprimir "OK" -- a mesma passagem
# vacua que motivou a aposentadoria de check_duplication.sh. Mesmo padrao ja
# usado em check_modelo_estrutura.sh.
arquivos="$(find src/poc -type f -path '*/configs/scenario.edl.in' | sort)"

if [ -z "$arquivos" ]; then
   echo "  FALHA nenhum scenario.edl.in encontrado sob src/poc -- o find quebrou?"
   exit 1
fi

for arquivo in $arquivos; do
   if python3 tests/guard/skeleton_diff.py "$arquivo" falcon1 falcon2 falcon3 falcon4; then
      echo "  OK   $arquivo"
   else
      echo "  FALHA $arquivo"
      fail=1
   fi
done

if [ $fail -eq 0 ]; then
   echo "falcons-estrutura: OK (falcon1..4 tem o mesmo esqueleto de slots em todos os cenarios)"
   exit 0
fi
echo "falcons-estrutura: FALHOU"
exit 1
