#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXE="$ROOT/build/linux/fastfreddy_testbed"
if [[ ! -x "$EXE" ]]; then
    echo "Missing $EXE. Run ./build_linux.sh first."
    exit 1
fi
cd "$(dirname "$EXE")"
exec ./fastfreddy_testbed
