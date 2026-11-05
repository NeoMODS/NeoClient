# NeoClient

NeoClient is a free, open-source utility client for Minecraft 26.3, written entirely in C++. The menu is built with Dear ImGui and injected with MinHook, while in-game actions go through the JVM using JNI. It is intended for single-player use only.

[![Download latest release](https://img.shields.io/github/v/release/NeoMODS/NeoClient?style=for-the-badge&label=Download&color=2ea44f&logo=github)](https://github.com/NeoMODS/NeoClient/releases/latest)

## Features

- **Movement**: Fly, Speed, No Clip, Air Jump, Boat Fly, Auto Jump, Auto Walk, Sprint
- **Visuals**: X-Ray with ore markers, Fullbright, Zoom, HUD
- **Utility**: No Fall, Auto Respawn
- **Menu**: categorized tabs, per-module settings and keybinds, search, adjustable font scale, window size, opacity, corner radius and animations
- **Config**: settings persist to `%APPDATA%\Neo\config.json`

## Requirements

- Windows 10 or 11 (64-bit)
- Minecraft 26.3 (single-player)

## Usage

1. Download `NeoClient.dll` and `NeoInjector.exe` from the [latest release](https://github.com/NeoMODS/NeoClient/releases/latest).
2. Keep both files in the same folder and start Minecraft 26.3.
3. Run `NeoInjector.exe`. It locates the Minecraft window automatically; you can also use `--pid <pid>` or `--name <exe>`.
4. Press **Right Shift** to open or close the menu. **Insert** works as an emergency toggle.

## Building from source

Requirements: CMake 3.24 or newer, a C++20 compiler (MSVC or MinGW-w64) and Ninja.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binaries are written to `build/bin`: `NeoClient.dll`, `NeoInjector.exe` and `NeoPreview.exe`.

`NeoPreview.exe` is a standalone UI harness. Use `NeoPreview.exe --screenshot out.png` to render the menu headlessly, or `--tab menu` to open a specific tab.

## Configuration

Settings are saved to `%APPDATA%\Neo\config.json`. Delete the file to restore the defaults.

## Notes

- NeoClient is designed for single-player worlds only.
- Vendored third-party code keeps its original licenses under `third_party/`.
