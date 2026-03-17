#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
DIST_DIR="$ROOT_DIR/dist"
APP_BUNDLE_DIR="$DIST_DIR/Markdown Buddy.app"
APP_CONTENTS_DIR="$APP_BUNDLE_DIR/Contents"
APP_MACOS_DIR="$APP_CONTENTS_DIR/MacOS"
APP_FRAMEWORKS_DIR="$APP_CONTENTS_DIR/Frameworks"
SDK_PATH="$(xcrun --show-sdk-path)"
SDK_VERSION="$(xcrun --show-sdk-version)"
SWIFT_TRIPLE="arm64-apple-macosx${SDK_VERSION}"

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
require_tool install_name_tool

mkdir -p "$DIST_DIR"

odin build "$ROOT_DIR/backend-odin/src" \
    -build-mode:shared \
    -minimum-os-version:14.0.0 \
    -no-entry-point \
    -out:"$DIST_DIR/libmarkdown_buddy.dylib"

swift build \
    --package-path "$ROOT_DIR/frontend-mac" \
    --sdk "$SDK_PATH" \
    --triple "$SWIFT_TRIPLE" \
    --product MarkdownBuddyMac \
    -c release

cp "$ROOT_DIR/frontend-mac/.build/arm64-apple-macosx/release/MarkdownBuddyMac" "$DIST_DIR/markdown-buddy-mac"

rm -rf "$APP_BUNDLE_DIR"
mkdir -p "$APP_MACOS_DIR" "$APP_FRAMEWORKS_DIR"

cp "$DIST_DIR/markdown-buddy-mac" "$APP_MACOS_DIR/MarkdownBuddyMac"
cp "$DIST_DIR/libmarkdown_buddy.dylib" "$APP_FRAMEWORKS_DIR/libmarkdown_buddy.dylib"

install_name_tool -id "@rpath/libmarkdown_buddy.dylib" "$APP_FRAMEWORKS_DIR/libmarkdown_buddy.dylib"
install_name_tool \
    -change "$DIST_DIR/libmarkdown_buddy.dylib" "@executable_path/../Frameworks/libmarkdown_buddy.dylib" \
    "$APP_MACOS_DIR/MarkdownBuddyMac"

cat > "$APP_CONTENTS_DIR/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>MarkdownBuddyMac</string>
    <key>CFBundleIdentifier</key>
    <string>com.krisfur.markdown-buddy</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Markdown Buddy</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSMinimumSystemVersion</key>
    <string>14.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
</dict>
</plist>
EOF

codesign --force --sign - "$APP_BUNDLE_DIR" >/dev/null 2>&1 || true

printf 'Built %s\n' "$DIST_DIR/markdown-buddy-mac"
printf 'Bundled %s\n' "$APP_BUNDLE_DIR"
