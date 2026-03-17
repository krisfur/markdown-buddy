#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
DIST_DIR="$ROOT_DIR/dist"
SDK_PATH="$(xcrun --show-sdk-path)"
SDK_VERSION="$(xcrun --show-sdk-version)"
SWIFT_TRIPLE="arm64-apple-macosx${SDK_VERSION}"

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

swift build --quiet \
    --package-path "$ROOT_DIR/frontend-mac" \
    --sdk "$SDK_PATH" \
    --triple "$SWIFT_TRIPLE" \
    --product MarkdownBuddyAbiProbe

DYLD_LIBRARY_PATH="$DIST_DIR${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" \
    "$ROOT_DIR/frontend-mac/.build/arm64-apple-macosx/debug/MarkdownBuddyAbiProbe" "$ROOT_DIR/example.md"
