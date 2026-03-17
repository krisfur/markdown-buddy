#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
DIST_DIR="$ROOT_DIR/dist"
BACKEND_DIR="$ROOT_DIR/backend-odin"
FRONTEND_DIR="$ROOT_DIR/frontend-win"
RESOURCE_DIR="$FRONTEND_DIR/resources"
BUILD_DIR="$FRONTEND_DIR/.build"
EXE_PATH="$DIST_DIR/markdown-buddy-win.exe"
DLL_PATH="$DIST_DIR/markdown_buddy.dll"
RES_PATH="$BUILD_DIR/app.res"
RES_OBJ_PATH="$BUILD_DIR/app-resource.o"
BACKEND_PREFIX="$BUILD_DIR/markdown_buddy_backend.obj"
FLTUSED_OBJ_PATH="$BUILD_DIR/fltused.obj"

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required tool: %s\n' "$1" >&2
        exit 1
    fi
}

require_tool zig
require_tool llvm-rc
require_tool llvm-cvtres
require_tool odin

mkdir -p "$DIST_DIR" "$BUILD_DIR"

rm -f "$BUILD_DIR"/markdown_buddy_backend-*.obj "$BUILD_DIR"/markdown_buddy_backend.obj

odin build "$BACKEND_DIR/src" \
    -build-mode:obj \
    -no-entry-point \
    -target:windows_amd64 \
    -out:"$BACKEND_PREFIX"

zig cc \
    -target x86_64-windows-gnu \
    -c "$FRONTEND_DIR/src/fltused.c" \
    -o "$FLTUSED_OBJ_PATH"

zig c++ \
    -target x86_64-windows-gnu \
    -shared \
    "$BUILD_DIR"/markdown_buddy_backend-*.obj \
    "$FLTUSED_OBJ_PATH" \
    "$RESOURCE_DIR/markdown_buddy.def" \
    -lbcrypt \
    -o "$DLL_PATH"

llvm-rc /fo "$RES_PATH" "$RESOURCE_DIR/app.rc"
llvm-cvtres /MACHINE:X64 /OUT:"$RES_OBJ_PATH" "$RES_PATH"

zig c++ \
    -target x86_64-windows-gnu \
    -std=c++20 \
    -O2 \
    "$FRONTEND_DIR/src/main.cpp" \
    "$RES_OBJ_PATH" \
    -I"$ROOT_DIR/backend-odin/include" \
    -Xlinker /subsystem:windows \
    -lole32 \
    -luuid \
    -lcomctl32 \
    -lshell32 \
    -lgdi32 \
    -lmsimg32 \
    -o "$EXE_PATH"

printf 'Built %s\n' "$EXE_PATH"
printf 'Built %s\n' "$DLL_PATH"
