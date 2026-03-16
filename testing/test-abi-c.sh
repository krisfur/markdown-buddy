#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
DIST_DIR="$ROOT_DIR/dist"
TEST_BIN="$ROOT_DIR/testing/abi_c_test"

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required tool: %s\n' "$1" >&2
        exit 1
    fi
}

require_tool odin
require_tool cc

mkdir -p "$DIST_DIR"

odin build "$ROOT_DIR/backend-odin/src" \
    -build-mode:shared \
    -no-entry-point \
    -out:"$DIST_DIR/libmarkdown_buddy.so"

cc "$ROOT_DIR/testing/abi_c_test.c" \
    -o "$TEST_BIN" \
    -I"$ROOT_DIR/backend-odin/include" \
    -L"$DIST_DIR" \
    -lmarkdown_buddy \
    -Wl,-rpath,"$DIST_DIR"

"$TEST_BIN"
