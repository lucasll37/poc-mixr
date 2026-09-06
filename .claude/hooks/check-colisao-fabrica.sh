#!/usr/bin/env bash
# PostToolUse (Edit|Write): depois de editar um .cpp/.hpp sob models/,
# reconfirma que nenhum PAR de modelos de producao publica o MESMO nome de
# fabrica MIXR -- colisao derruba o processo em runtime (die() em
# PluginRegistry::loadModule::loadModule, ja aconteceu de verdade com
# ThreadTagProbe). Delega para tests/guard/check_colisao_fabrica.py.
#
# Fail-safe: sai 0 em silencio se faltar python3/o proprio script, ou se o
# arquivo editado nao for relevante -- nunca trava a sessao.
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
    */models/*.cpp|*/models/*.hpp) : ;;
    *) exit 0 ;;
esac

ROOT="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
GUARD="$ROOT/tests/guard/check_colisao_fabrica.py"
[ -f "$GUARD" ] || exit 0

OUTPUT="$(python3 "$GUARD" 2>&1)"
STATUS=$?

if [ "$STATUS" -ne 0 ]; then
    {
        echo "check_colisao_fabrica.py FALHOU apos esta edicao -- dois modelos publicando o mesmo nome"
        echo "de fabrica derrubam o processo em runtime (die() em PluginRegistry::loadModule)."
        echo "Corrija antes de continuar:"
        echo "$OUTPUT"
    } >&2
    exit 2
fi

exit 0
