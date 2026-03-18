# Markdown buddy

Simple application for writing markdown files with live preview and a section list.

Made for educational purposes of learning how to combine a single backend with platform native frontends via the `C ABI`.

**Linux:**

![screenshot-l](./screenshot-linux.png)

**MacOS:**

![screenshot-m](./screenshot-mac.png)

**Windows (wine)**

![screenshot-w](./screenshot-win.png)

> Very much a work in progress.

## Structure

Multi language setup:
- Backend: `Odin` -> `C ABI`
- Frontend Linux: `GTK4`
- Frontend Mac: `SwiftUI`
- Frontend Windows: `Win32`

## Build

General requirement: `Odin`, `C/C++` compiler/build tools

- Linux: 

    extra requirement: `GTK4`

    ```bash
    bash build-linux.sh
    ./dist/markdown-buddy-gtk4 example.md
    ```

- macOS: 

    extra requirement: `Swift`

    ```bash
    bash build-mac.sh
    ./dist/markdown-buddy-mac example.md
    ```

- Windows (wine): 

    extra requirement: `zig`

    ```bash
    bash build-windows.sh
    DISPLAY=:0 wine ./dist/markdown-buddy-win.exe example.md
    ```
