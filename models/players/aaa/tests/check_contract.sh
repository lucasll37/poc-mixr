#!/usr/bin/env bash
#
# Os dois criterios que CONTRIBUTING.md secao 5.1 ja documentava a mao,
# automatizados: um .so de plugin tem de exportar EXATAMENTE um simbolo
# global (o ponto de entrada da macro MIXR_PLUGIN_DEFINE -- ver
# models/template/docs/CONTRATO.md) e nao pode ter dependencia
# dinamica nao resolvida.
#
# Nao afirma nada sobre COMPORTAMENTO -- so a forma do artefato. Roda contra
# aaa_lib (ver tests/meson.build) -- o script em si e generico, nao cita
# nome de artefato nenhum. Este projeto, ao contrario de models/template/,
# nao hospeda mirror de contrato (src/mirror.cpp nao faz parte do scaffold
# copiavel, ja removido por 'make new-model').
#
set -euo pipefail

SO="${1:?uso: check_contract.sh <caminho-do-.so>}"

n="$(nm -D --defined-only "$SO" | grep -c ' T ' || true)"
if [ "$n" -ne 1 ]; then
   echo "FALHA: esperava exatamente 1 simbolo T exportado em $SO, achei $n"
   nm -D --defined-only "$SO" | grep ' T ' || true
   exit 1
fi
echo "OK: 1 simbolo T exportado"

if ldd "$SO" | grep -q 'not found'; then
   echo "FALHA: dependencia dinamica nao resolvida em $SO:"
   ldd "$SO" | grep 'not found'
   exit 1
fi
echo "OK: todas as dependencias dinamicas resolvidas"
