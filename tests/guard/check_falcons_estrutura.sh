#!/usr/bin/env bash
#
# Os cenarios de src/poc/single-thread e src/poc/multi-thread declaram
# falcon1..falcon4, cada um um bloco '( Aircraft ... )'. falcon1 carrega
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

for arquivo in src/poc/single-thread/configs/scenario.epp.in \
               src/poc/multi-thread/configs/scenario.epp.in; do
   if python3 tests/guard/skeleton_diff.py "$arquivo" falcon1 falcon2 falcon3 falcon4; then
      echo "  OK   $arquivo"
   else
      echo "  FALHA $arquivo"
      fail=1
   fi
done

if [ $fail -eq 0 ]; then
   echo "falcons-estrutura: OK (falcon1..4 tem o mesmo esqueleto de slots nos dois cenarios)"
   exit 0
fi
echo "falcons-estrutura: FALHOU"
exit 1
