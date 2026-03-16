#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
DIST_DIR="$ROOT_DIR/dist"

if [[ "$(uname -s)" != "Darwin" ]]; then
    printf 'build-mac.sh must be run on macOS.\n' >&2
    exit 1
fi

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required tool: %s\n' "$1" >&2
        exit 1
    fi
}

require_tool odin
require_tool swift

mkdir -p "$DIST_DIR"

odin build "$ROOT_DIR/backend-odin/src" \
    -build-mode:shared \
    -no-entry-point \
    -out:"$DIST_DIR/libmarkdown_buddy.dylib"

swift build --package-path "$ROOT_DIR/frontend-mac" --product MarkdownBuddyMac -c release

cp "$ROOT_DIR/frontend-mac/.build/release/MarkdownBuddyMac" "$DIST_DIR/markdown-buddy-mac"

printf 'Built %s\n' "$DIST_DIR/markdown-buddy-mac"
