<div align="center">

<img src="Assets/logo.png" alt="Amatayakul Logo" width="140" />

# Amatayakul Client

**A legit client version of the Azyre hacked client, powered by DirectX 11 and Dear ImGui**

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![DirectX 11](https://img.shields.io/badge/DirectX-11-68217A?style=for-the-badge&logo=microsoft&logoColor=white)](https://learn.microsoft.com/en-us/windows/win32/direct3d11/dx-graphics-overviews)
[![Windows](https://img.shields.io/badge/Windows-10%2F11-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C?style=for-the-badge&logo=cmake&logoColor=white)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-ADPSL--1.1-red?style=for-the-badge)](LICENSE)
[![Version](https://img.shields.io/badge/Version-2.0.0-brightgreen?style=for-the-badge)](https://github.com/AnarchDevelopment/AmatayakulDLL/releases)

<br/>

> **Amatayakul Client v2** — a complete rewrite of the legacy [AmatayakulDLL](https://github.com/AnarchDevelopment/AmatayakulDLL), built as a legit client on top of the [Azyre](https://github.com/AnarchDevelopment/AzyreDll) codebase by [**an4rch Development**](https://anarchdevelopment.github.io/).

</div>

---

## Overview

<div align="center">

<img src="Assets/PreviewRe.png" alt="Amatayakul Preview" width="750" />

</div>

**Amatayakul Client** is a high-performance C++20 DLL client that hooks into Minecraft's rendering pipeline via DirectX 11. Built as a "legit client" counterpart to the Azyre hacked client, it provides a refined suite of modules — focused on utility, performance, and visual overlays — wrapped in a sleek, GPU-accelerated ImGui interface.

> **Lineage:** AmatayakulDLL (legacy) → Azyre fork → **Amatayakul Client v2.0.0** (complete rewrite)

| Layer | Technology |
|---|---|
| Language | C++20 (MSVC / CMake) |
| Rendering | DirectX 11 + HLSL shaders |
| GUI | Dear ImGui with custom DX11 backend |
| Hooking | MinHook (x64) |
| Config | nlohmann/json (built-in default preset) |
| Audio | miniaudio |
| Images | stb_image / stb_image_write |
| Networking | Winsock2 / IRC client |

---

## Project Structure

```
AmatayakulDLL/
├── Animations/          # Easing & animation system
├── ArrayList/           # HUD active-module list (centralized renderer)
├── Assets/              # Fonts, textures, shaders, audio, RC resources
│   ├── Fonts/
│   ├── CategoryImage/
│   ├── MarketAssets/
│   ├── stb/
│   ├── blur_ps.hlsl     # Blur pixel shader
│   └── blur_vs.hlsl     # Blur vertex shader
├── Config/              # JSON config serialization / deserialization
├── Core/                # DX11 Present hook & main thread
├── GUI/                 # ImGui window orchestration & DX11 renderer
├── Hook/                # WndProc / D3D hook management
├── ImGui/               # Dear ImGui source + markdown extension
├── Input/               # Keyboard/mouse input & UWP compatibility
├── MinHook/             # Inline function hooking library
├── Modules/             # All feature modules (see below)
│   ├── Combat/
│   ├── Movement/
│   ├── Visuals/
│   ├── Misc/
│   ├── Info/
│   ├── Terminal/
│   ├── Splash/
│   ├── PatternScan/
│   └── Alloc/
├── Networking/          # IRC client & chat overlay
├── Utils/               # HUD element base, WinRT title helper
├── miniaudio/           # Single-header audio library
├── nlohmann/            # Single-header JSON library
└── dllmain.cpp          # DLL entry point
```

---


## Building

### Prerequisites

- Windows 10 / 11
- [Visual Studio 2022+](https://visualstudio.microsoft.com/) with **Desktop development with C++**
- [CMake 3.20+](https://cmake.org/download/)
- Windows SDK 10.0+

### Quick Build (PowerShell)

```powershell
.\build.ps1
```

Runs the full CMake configure + Release build in one step. Output: `build/Release/Kitty.dll`

### Build with CMake (manual)

```bash
# Clone
git clone https://github.com/AnarchDevelopment/AmatayakulDLL.git
cd AmatayakulDLL

# Configure (x64 Release)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release
```

Output: `build/Release/Kitty.dll`

### Build with Visual Studio

1. Open `build/Kitty.sln` in Visual Studio
2. Select **Release | x64**
3. Build → Build Solution (`Ctrl+Shift+B`)

---

## Configuration

Configs are stored as JSON files and managed by `Config/ConfigManager`. A built-in default preset is embedded at compile time so the client works out of the box without a config file.

```json
{
  "modules": {
    "Watermark": { "enabled": true },
    "MotionBlur": { "enabled": false, "intensity": 0.5 },
    "NoHurtCam": { "enabled": false }
  }
}
```

HUD element positions and sizes are saved and restored across sessions. The config also persists keybind assignments for each module.

---

## Dependencies

All dependencies are **vendored** — no package manager needed.

| Library | Version | Purpose |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | Custom | GUI framework |
| [MinHook](https://github.com/TsudaKageyu/minhook) | Bundled | x86/x64 hooking |
| [nlohmann/json](https://github.com/nlohmann/json) | Bundled | JSON config |
| [stb_image](https://github.com/nothings/stb) | Bundled | Image loading |
| [miniaudio](https://miniaud.io/) | Bundled | Audio playback |
| [imgui-markdown](https://github.com/juliettef/imgui_markdown) | Bundled | Markdown in ImGui |

---

## Changelog

### v2.0.0 — Complete Rewrite
- Full rewrite of legacy AmatayakulDLL codebase
- Repurposed as a legit client based on Azyre
- New GUI with smooth appearance/disappearance animations
- Rise Background DX11 shader for ClickGUI
- Improved MotionBlur and expanded ClickGUI styles
- Centralized ArrayList renderer
- HUD resize/drag scale saved to config
- Built-in default config preset embedded at compile time
- Dynamic startup message
- Fixed RSHIFT/LSHIFT disambiguation in keybind system
- **New module**: NoHurtCam (pattern-scanned hurt cam disable)
- AutoSprint HUD text overlay

---

## License

This project is licensed under the **an4rch Development Public Source License v1.1 (ADPSL-1.1)**.  
See [LICENSE](LICENSE) for full terms.

This is a **source-available** permissive license. Attribution to the original authors and project must be preserved in all derivative works. Original work by [an4rch Development](https://anarchdevelopment.github.io/).

---

## Contact

<div align="center">

| Platform | Handle |
|---|---|
| GitHub | [@amatayakull](https://github.com/amatayakull) |
| Discord | `zhenrikepvp_` |
| Organization | [an4rch Development](https://anarchdevelopment.github.io/) |

</div>
