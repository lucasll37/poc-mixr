#!/usr/bin/env bash
#
# A DEMONSTRACAO AO VIVO de "sem precisar recompilar tudo".
#
# O teste 'plugin-hotswap' da suite prova a PROPRIEDADE (mesmo binario + .so
# diferente = comportamento diferente) usando duas variantes pre-buildadas.
# Este alvo prova o FLUXO: editar o fonte do plugin, rebuildar SO o plugin, e
# ver o binario -- nao tocado, mesmo sha256 -- mudar de comportamento.
#
# A prova de verdade e a saida do ninja no passo 3: tem de listar DUAS edges e
# NAO citar o executavel do host. Compare com o contra-exemplo: mexer em
# app/include/app/Fleet.hpp dispara um relink do 'app' inteiro -- e o modelo
# nao e nem tocado.
#
# O binario e o ./app: as pocs nao tem mais executavel proprio (ver
# src/poc/meson.build). A fixture continua saindo do cenario da single-thread,
# e entra por '-f', como em todo teste deste repositorio.
#
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ"

BIN=build/app/src/app
SO=dist/lib/mixr-plugins/libflight.so
# O diretorio de build do MODELO. Ficou apontando para 'build-flight' (um
# layout que nao existe mais: models/player/flight/Makefile builda em ./build dentro
# do proprio projeto), e como todas as chamadas de meson aqui terminam em
# '|| true' ou com a saida descartada, o alvo falhava sem dizer por que.
MODEL_BUILD=models/player/flight/build
FIX=build/tests-fixtures/single-thread-hotswap-vivo.edl.in

# 'meson install' do MODELO deposita em models/player/flight/dist/ -- o dist LOCAL
# daquele projeto, nao o do host. Quem o host carrega e dist/lib/mixr-plugins/,
# e o caminho normal ate la e 'make install-host' (-> plugins/) seguido
# do 'sync-plugins' do Makefile raiz. Aqui a demonstracao precisa do .so novo
# no lugar onde o ./app vai abri-lo, entao a copia e feita direto -- e a MESMA
# copia que o sync-plugins faz, sem reconstruir mais nada.
publica_so() {
   cp -f models/player/flight/dist/lib/mixr-plugins/libflight.so "$SO"
}

[ -x "$BIN" ] || { echo "rode 'make build' primeiro"; exit 1; }

# Fixture hermetica, com duas correcoes que a demonstracao exige (as mesmas de
# tests/plugin/run_plugin_hotswap.py, e as duas foram descobertas quebrando):
#   * sem o intruso -- com ele a arvore vai para EVADE e a patrulha, que e o
#     ramo que muda, quase nao roda;
#   * perna de 10 s -- a de producao vira a cada 45-60 s, longe demais para a
#     janela do teste; e uma perna curta demais faz o rumo comandado saltar
#     mais rapido do que a aeronave (maxRateOfTurnDps=3) consegue seguir.
mkdir -p build/tests-fixtures
python3 tests/scenario/make_fixture.py --poc single-thread --mode intruder --out "$FIX" > /dev/null
python3 - "$FIX" <<'PYEOF'
import re, sys
p = sys.argv[1]
t = open(p).read()
i = t.find("bandit1: (")
if i >= 0:
    j = t.index("(", i); nivel = 0; k = j
    while k < len(t):
        if t[k] == "(": nivel += 1
        elif t[k] == ")":
            nivel -= 1
            if nivel == 0: break
        k += 1
    t = t[:i] + t[k+1:]
t = re.sub(r"legTime:(\s*)\( Seconds \d+ \)", r"legTime:\g<1>( Seconds 10 )", t)
open(p, "w").write(t)
PYEOF

# Backup em disco, e nao 'git checkout': o plugin pode ainda nao estar
# versionado, e um alvo de demonstracao nao pode depender disso.
HDR=models/player/flight/src/domain/PatrolPlan.cpp
BKP=$(mktemp)
cp "$HDR" "$BKP"
restaura() {
   cp "$BKP" "$HDR"
   rm -f "$BKP"
   meson compile -C "$MODEL_BUILD" flight > /dev/null 2>&1 || true
   meson install -C "$MODEL_BUILD" --no-rebuild > /dev/null 2>&1 || true
   publica_so || true
}
trap restaura EXIT

# Normaliza o ponto de partida antes de medir. Sem isto o resultado dependeria
# do valor que estivesse no header e do que houvesse em disco de uma execucao
# anterior -- medido quebrando: as duas execucoes deram o mesmo dump porque o
# .so em disco ja era o "depois".
echo "=== 0) normalizando o ponto de partida ==="
sed -i 's/^\( *\)#define POC_MODEL_TURN_SIGN .*/\1#define POC_MODEL_TURN_SIGN 1.0/' "$HDR"
meson compile -C "$MODEL_BUILD" flight > /dev/null 2>&1
meson install -C "$MODEL_BUILD" --no-rebuild > /dev/null 2>&1
publica_so
echo "    default do modelo: curva de patrulha no sentido normal"

echo "=== 1) estado inicial ==="
SHA_ANTES=$(sha256sum "$BIN" | cut -d' ' -f1)
MT_ANTES=$(stat -c %Y "$BIN")
echo "    executavel: ${SHA_ANTES:0:16}..."

echo "=== 2) execucao A ==="
"$BIN" -f "$FIX" -threads 1 -deterministic 1500 2>/dev/null | grep '^frame=' > /tmp/hotswap-A.txt
grep 'player=falcon1' /tmp/hotswap-A.txt | tail -1 | grep -o ' hdg=[0-9.]*' | sed 's/^/    falcon1/'

echo "=== 3) editando SO o modelo e rebuildando SO o .so ==="
sed -i 's/^\( *\)#define POC_MODEL_TURN_SIGN .*/\1#define POC_MODEL_TURN_SIGN -1.0/' "$HDR"
meson compile -C "$MODEL_BUILD" flight 2>&1 | grep -E "^\[|Compiling|Linking" | sed 's/^/    /'
meson install -C "$MODEL_BUILD" --no-rebuild > /dev/null 2>&1
publica_so

echo "=== 4) o executavel foi tocado? ==="
SHA_DEPOIS=$(sha256sum "$BIN" | cut -d' ' -f1)
MT_DEPOIS=$(stat -c %Y "$BIN")
if [ "$SHA_ANTES" != "$SHA_DEPOIS" ] || [ "$MT_ANTES" != "$MT_DEPOIS" ]; then
   echo "    FALHA: o executavel mudou -- a tese nao se sustenta"
   exit 1
fi
echo "    NAO. mesmo sha256, mesmo mtime."

echo "=== 5) execucao B, MESMO binario ==="
"$BIN" -f "$FIX" -threads 1 -deterministic 1500 2>/dev/null | grep '^frame=' > /tmp/hotswap-B.txt
grep 'player=falcon1' /tmp/hotswap-B.txt | tail -1 | grep -o ' hdg=[0-9.]*' | sed 's/^/    falcon1/'

if diff -q /tmp/hotswap-A.txt /tmp/hotswap-B.txt > /dev/null; then
   echo "=== FALHOU: as duas execucoes deram o mesmo dump ==="
   exit 1
fi
echo "=== OK: mesmo executavel, comportamento diferente ==="
