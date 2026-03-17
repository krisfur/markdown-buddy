# Markdown buddy

Simple application for writing markdown files with live preview and a section list.

Made for educational purposes of learning how to combine a single backend with platform native frontends via the `C ABI`.

**Linux:**

![screenshot-l](./screenshot-linux.png)

**MacOS:**

![screenshot-m](./screenshot-mac.png)

> Very much a work in progress.

## Structure

Multi language setup:
- Backend: `Odin` -> `C ABI`
- Frontend Linux: `GTK4`
- Frontend Mac: `SwiftUI`
- Frontend Windows: `Win32` + `Zig C++`

## Build

- Linux: `./build-linux.sh`
- macOS: `./build-mac.sh`
- Windows: `./build-windows.sh`

The Windows frontend currently builds a native Win32 executable with `zig c++` from `frontend-win/` and embeds a modern manifest for common controls and DPI awareness.

The build outputs land in `dist/`, alongside the Linux and macOS artifacts.

`build-windows.sh` works around Odin's current Windows cross-linking limitation by building Windows COFF objects with Odin and linking `markdown_buddy.dll` with Zig.
