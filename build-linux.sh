#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
DIST_DIR="$ROOT_DIR/dist"
BACKEND_DIR="$ROOT_DIR/backend-odin"
FRONTEND_DIR="$ROOT_DIR/frontend-linux"
GTK_CFLAGS="$(pkg-config --cflags gtk4)"
GTK_LIBS="$(pkg-config --libs gtk4)"

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required tool: %s\n' "$1" >&2
        exit 1
    fi
}

require_tool odin
require_tool cc
require_tool pkg-config

if ! pkg-config --exists gtk4; then
    printf 'Missing GTK4 development files (pkg-config gtk4 failed).\n' >&2
    exit 1
fi

mkdir -p "$DIST_DIR"

odin build "$BACKEND_DIR/src" \
    -build-mode:shared \
    -out:"$DIST_DIR/libmarkdown_buddy.so"

cc "$FRONTEND_DIR/src/main.c" \
    -o "$DIST_DIR/markdown-buddy-gtk4" \
    -I"$BACKEND_DIR/include" \
    -L"$DIST_DIR" \
    -lmarkdown_buddy \
    $GTK_CFLAGS \
    $GTK_LIBS \
    -Wl,-rpath,'$ORIGIN'

printf 'Built %s\n' "$DIST_DIR/markdown-buddy-gtk4"
