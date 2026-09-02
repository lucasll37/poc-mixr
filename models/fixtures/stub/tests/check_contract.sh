#!/usr/bin/env bash
#
# Os dois criterios que models/README.md §2.3 ja documentava a mao, agora
# automatizados: um `.so` de plugin tem de exportar EXATAMENTE um simbolo
# global (o ponto de entrada da macro MIXR_PLUGIN_DEFINE -- ver
# CONTRATO.md) e nao pode ter dependencia dinamica nao resolvida.
#
# Nao afirma nada sobre COMPORTAMENTO (isso e tests/plugin/run_stub_model.py,
# na raiz, rodando o cenario de producao de verdade) -- so a forma do
# artefato.
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
