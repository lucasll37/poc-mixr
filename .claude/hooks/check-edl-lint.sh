#!/usr/bin/env bash
# PostToolUse (Edit|Write) em arquivos .edl/.edl.in/.edl.frag: roda o lint
# leve (src/ui/scripts/edl_lint.py) contra o catalogo de fabricas/slots. E SO
# UM AVISO -- best-effort, nao substitui o binario 'edlcheck' (o parser C++
# de verdade, ver o docstring do proprio edl_lint.py).
#
# Fail-safe: sai 0 em silencio se faltar python3, se o catalogo ainda nao
# foi gerado (rode 'make open-edl' uma vez) ou se o arquivo editado
# nao for relevante -- nunca trava a sessao por causa de infraestrutura que
# nao foi montada.
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
    *.edl|*.edl.in|*.edl.frag) : ;;
    *) exit 0 ;;
esac

[ -f "$FILE_PATH" ] || exit 0

ROOT="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
CATALOG="$ROOT/src/ui/edl_catalog.generated.json"
LINT="$ROOT/src/ui/scripts/edl_lint.py"

[ -f "$CATALOG" ] || exit 0
[ -f "$LINT" ] || exit 0

OUTPUT="$(python3 "$LINT" "$FILE_PATH" 2>&1)"

case "$OUTPUT" in
    *ERRO:*|*AVISO:*)
        {
            echo "edl_lint.py encontrou algo em $FILE_PATH (lint leve, best-effort -- confirme com"
            echo "o binario 'edlcheck $FILE_PATH' (dist/bin/edlcheck ou build/app/src/edlcheck)"
            echo "antes de considerar a tarefa concluida):"
            echo "$OUTPUT"
        } >&2
        exit 2
        ;;
    *)
        exit 0
        ;;
esac
