#!/usr/bin/env bash
set -euo pipefail
CONFIG="${1:-Release}"
EXE="build/linux-${CONFIG}/epoch_sand"
[[ -x "$EXE" ]] || ./build_linux.sh "$CONFIG"
exec "$EXE"
