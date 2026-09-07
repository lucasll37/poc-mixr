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
