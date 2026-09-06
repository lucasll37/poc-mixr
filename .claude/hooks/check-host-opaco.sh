#!/usr/bin/env bash
# PostToolUse (Edit|Write): depois de editar algo sob app/, src/, shared/ ou
# qualquer meson.build, reconfirma que o build do HOST continua sem
# referenciar fonte de MODELO (domain/bt/ubf/xnative moram so em
# models/player/<nome>/). Delega para tests/guard/check_host_opaco.sh, que
# ja e o script canonico dessa checagem (rodado tambem por 'make test').
#
# Fail-safe: sai 0 em silencio se faltar python3/bash/o proprio script, ou
# se o arquivo editado nao for relevante -- nunca trava a sessao.
set -u

command -v python3 >/dev/null 2>&1 || exit 0

FILE_PATH="$(python3 -c '
import json, sys
try:
    d = json.load(sys.stdin)
except Exception:
    sys.exit(0)
print(d.get("tool_input", {}).get("file_path", ""))
' 2>/dev/null)"

[ -z "$FILE_PATH" ] && exit 0

case "$FILE_PATH" in
    */app/*|*/src/*|*/shared/*|*meson.build) : ;;
    *) exit 0 ;;
esac

ROOT="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
GUARD="$ROOT/tests/guard/check_host_opaco.sh"
[ -f "$GUARD" ] || exit 0

OUTPUT="$(bash "$GUARD" 2>&1)"
STATUS=$?

if [ "$STATUS" -ne 0 ]; then
    {
        echo "check_host_opaco.sh FALHOU apos esta edicao -- o build do host (app/src/shared) nao pode"
        echo "referenciar fonte de MODELO (domain/bt/ubf/xnative moram so em models/player/<nome>/)."
        echo "Corrija antes de continuar:"
        echo "$OUTPUT"
    } >&2
    exit 2
fi

exit 0
