#!/usr/bin/env bash
#
# Resolve o caminho do binario Groot no cache Conan (deps/groot/conanfile.py).
# Usado por 'make open-groot' -- separado em script proprio porque resolver
# o pacote nao e' uma linha so: 'conan cache path groot/1.0.0' (so a
# referencia, sem package_id) devolve a pasta de EXPORT/recipe, nao a de
# PACKAGE -- precisa do package_id completo, que muda conforme compilador/
# settings da maquina.
set -e

PKG_ID=$(conan list "groot/1.0.0:*" --format=json 2>/dev/null | python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
    revs = data['Local Cache']['groot/1.0.0']['revisions']
    rev = next(iter(revs.values()))
    print(next(iter(rev['packages'])))
except (KeyError, StopIteration, json.JSONDecodeError):
    sys.exit(1)
")

PKG_PATH=$(conan cache path "groot/1.0.0:${PKG_ID}")
echo "${PKG_PATH}/bin/Groot"

# Aviso (nunca erro) quando o binario em cache e' ANTERIOR a FIX 6 de
# deps/groot/conanfile.py -- o patch que impede o Monitor de fechar sozinho.
# Um pacote velho reintroduz o bug EM SILENCIO, e o sintoma (a janela some sem
# mensagem) nao aponta pra ca de jeito nenhum. O marcador vem dos literais de
# qDebug de FIX 6c/6d, entao sobrevive no binario.
#
# Vai em STDERR de proposito: stdout deste script e' consumido por
# 'make open-groot' como o CAMINHO do executavel -- qualquer outra linha ali
# quebraria o alvo.
#
# ACHADO RODANDO (nao redescobrir): o marcador NAO esta em bin/Groot --
# 'sidepanel_monitor.cpp' compila em lib/libbehavior_tree_editor.so, a .so
# intermediaria entre o QtNodeEditor e o executavel. Procurar so' no binario
# dava falso alarme mesmo com a FIX 6 aplicada; dai os dois caminhos abaixo.
if command -v strings >/dev/null 2>&1; then
  if ! strings "${PKG_PATH}/bin/Groot" "${PKG_PATH}/lib/libbehavior_tree_editor.so" 2>/dev/null \
       | grep -q 'POC-MIXR-FIX6'; then
    echo "find_groot: AVISO -- este Groot em cache e' anterior a FIX 6 (deps/groot/conanfile.py)." >&2
    echo "find_groot: no modo Monitor ele FECHA SOZINHO, sem dialogo. Reconstrua com:" >&2
    echo "find_groot:   conan remove 'groot/*' -c && conan create ./deps/groot --build=missing --settings=build_type=Release" >&2
  fi
fi
