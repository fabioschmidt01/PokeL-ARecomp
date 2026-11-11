<div align="center">

<img src="docs/icon/icon_1024.png" alt="PokeL-ARecomp Icon" width="140">

# PokeL-ARecomp (PLA Recomp)

**A native PC port of *Pokémon Legends: Arceus*, built by static recompilation.**

**English** · [Português (Brasil)](README.pt-BR.md)

[Report an issue](https://github.com/fabioschmidt01/PokeL-ARecomp/issues) ·
[Releases](https://github.com/fabioschmidt01/PokeL-ARecomp/releases)

</div>

---

**PokeL-ARecomp** (PLA Recomp) turns *Pokémon Legends: Arceus* Nintendo Switch code into a native 64-bit Windows and Linux program.
The game's ARM64 machine code is translated ahead-of-time (AOT) into C and compiled natively for x86-64. The Nintendo Switch OS kernel, filesystem, audio and Maxwell NVN graphics are reimplemented directly on top of Vulkan.

**No emulator runs while you play.** The game executes natively on your CPU and GPU.

> [!IMPORTANT]
> **You need your own copy of the game.** PokeL-ARecomp does not bundle any game code, ROMs, assets, textures or encryption keys. You must dump your legally purchased game (`.nsp` / `.xci` + update 1.1.1) and `prod.keys` from your own console.

---

## Features

### Performance & Engine
- **30 or 60 FPS Target (Option)**: Native game speed with dynamic delta-time pacing. The frame rate is unsteady in the open world (around 40–50 fps on the test machine, with drops in demanding areas), and there are brief hitches the first time each shader is compiled.
- **Frame Generation (90–120 FPS)**: Optional hardware-accelerated optical flow frame interpolation smooths motion up to 90–120 FPS without speeding up game logic.
- **HUD Decoupling**: Isolates 2D UI elements (minimap, health gauges, reticles, text) via `hdr_snapshot` to eliminate ghosting artifacts during Frame Generation.
- **Explicit VSync Modes**: Choose between *Off (Low Latency / Mailbox)*, *On (VSync / 60 Hz - FIFO)*, or *Off (Unlimited / Immediate)* with instant swapchain recreation.
- **Internal Resolution Scaling**: From 25% up to 4x native 1080p (4K UHD) with Dynamic Resolution scaling.
- **High Core-Count Scaling (Intel Xeon & Multi-Core CPUs)**: Dynamic worker thread scaling (up to 8 parallel texture deswizzling threads on 16-24+ thread CPUs) eliminating texture streaming stutter.
- **Dedicated GPU Memory Unlocking (Intel Arc & High-VRAM GPUs)**: Full 3.1 GB physical pool unlocked for GPUs with dedicated VRAM (such as the 12 GB Intel Arc B580 Battlemage), preventing memory thrashing.

### Graphics & Upscaling
- **Separated Options Menu**: Dedicated **Video** (display, resolution, fps, vsync) and **Advanced Video** (XeSS, FSR, Frame Gen, shadows, AO, SSR, GI) tabs for clean customization.
- **Intel XeSS (Xe Super Sampling)**: AI neural upscaling accelerated by hardware XMX engines on Intel Arc GPUs (Ultra Performance to Native AA).
- **AMD FidelityFX FSR 1, FSR 2 (temporal)**, and **SMAA** anti-aliasing.
- **Contrast-Adaptive Sharpening (RCAS)**: Real-time image clarity adjustment.
- **Custom Shading & Lighting**: Screen-space ambient occlusion (XeGTAO), water reflections (SSR), screen-space indirect lighting (SSGI), and high-resolution shadow mapping (up to 4x).
- **HDR Display Output**: scRGB / HDR10 support with peak brightness tuning.
- **HD Texture Pack Loader**: Direct loading from `textures/hd/`.

### Tools & Utilities
- **PokeL-Extractor (Standalone ROM Tool)**: Bundled single-file GUI & CLI utility based on LibHac. Inspects NSP/XCI dumps, auto-detects base games, updates (v262144), and DLCs, displays Nintendo Switch game icons, and merges BKTR RomFS patches with real-time speed and ETA.

### Controls & Input
- Any modern gamepad via SDL3: Xbox, DualShock 4, DualSense, Nintendo Switch Pro Controller, Steam Deck.
- Full Keyboard & Mouse support with smooth mouse-directed camera and customizable keybindings.
- Dynamic controller button glyphs matching your active controller layout.

---

## Supported Version

| Game | Title ID | Target Version | `main` Build ID |
|---|---|---|---|
| **Pokémon Legends: Arceus** | `01001F5010DFA000` | **1.1.1** (update v262144) | `AEE8F150DDA1B5A8` |

---

## Requirements

| Component | Minimum | Recommended |
|---|---|---|
| **Operating System** | Windows 10/11 64-bit or Linux x86-64 (glibc 2.35+) | Windows 11 / Modern Linux |
| **Processor** | x86-64 with AVX2 (4 Cores) | Intel Xeon E5 / Core i5/i7/i9 or AMD Ryzen (8+ Cores) |
| **Graphics Card** | Vulkan 1.3 compatible GPU (4 GB VRAM) | **Intel Arc B580 (12 GB)** / RTX 3060+ / RX 6600+ |
| **System Memory** | 8 GB RAM | 16 GB+ RAM |
| **Storage** | ~12 GB free space | SSD / NVMe |

---

## Quick Start (Playing)

1. Download the latest release from the [Releases](https://github.com/fabioschmidt01/PokeL-ARecomp/releases) tab (`PokeL-ARecomp-windows-x64.zip` for Windows, or `.AppImage` for Linux).
2. Extract the archive into a writable folder (e.g. `C:\Games\PokeL-ARecomp`).
3. Prepare your game files:
   - Base game (`.nsp` or `.xci`), the **1.1.1 update** (`.nsp`), and your `prod.keys` dumped from your console.
4. Launch `PokeL-ARecomp.exe` (or `./plarecomp.AppImage`).
5. In the start screen, choose your game folder and click **Set up the game**.
6. The initial static recompilation takes 10 to 25 minutes depending on your CPU core count (this occurs **only once**).
7. Click **Play** and enjoy exploring Hisui!

---

## Standalone ROM Extractor (PokeL-Extractor)

Bundled with every release is **PokeL-Extractor** (`PokeL-Extractor.exe`), a standalone Nintendo Switch ROM analysis and extraction utility built with LibHac and C# / .NET 9.

- **Recursive Directory Scanning**: Scans entire directories and subfolders for `.nsp`, `.nsz`, `.xci`, and `.xcz` files.
- **Automatic Title ID & CNMT Detection**: Identifies whether a dump is a **Base Game**, an **Update** (such as v262144), or **DLC**, even if files have been arbitrarily renamed.
- **Switch Game Icon Extraction**: Reads embedded JPEG/PNG icons directly from Control NCAs and displays them in the UI.
- **RomFS & ExeFS Extraction**: Merges BKTR base and update patches automatically into clean directories ready for PokeL-ARecomp.
- **Real-time Progress & Telemetry**: Reports elapsed time, transfer speed (MB/s), estimated time remaining (ETA), and line-by-line logs.
- **Dual GUI & CLI Modes**: Run interactively with the modern dark GUI or automate via terminal (`PokeL-Extractor.exe --scan <folder> --export <out>`).

---

## Controls

| Keyboard | Action | Gamepad |
|---|---|---|
| W / A / S / D | Move player | Left Stick |
| Mouse Movement | Camera control | Right Stick |
| Left Click | Throw Pokéball / Attack (ZR) | ZR |
| Right Click | Aim (ZL) | ZL |
| Space / Shift | Dodge Roll / Sprint (Y / B) | Y / B |
| F1 / Esc | Settings Menu | + / − |
| F10 | Save Screenshot (`screenshots/`) | - |
| F11 | Fullscreen toggle | - |

---

## Version History & Changelog

### v0.0.26 (Latest)
- **F1 Menu Stability & Transparency Bug Fix**: Completely resolved the issue where opening F1 menus with Frame Generation made options semi-transparent, ghosted, or flickering. Frame Generation now automatically disengages while inside the pause/settings menu, providing rock-solid, fully opaque UI.
- **HUD & Overlay Frame Generation Persistence**: Implemented `menu_render_repeat` so in-game HUD elements (FPS counter, performance monitor, timers) are drawn onto both generated and base frames, eliminating HUD flicker during gameplay.
- **Temporal Presentation Pacing for Frame Generation**: Added half-frame pacing (`dt_half`) between intermediate and base frame presentations, ensuring monitors actually scan out both frames (reaching true 90–120 FPS output) rather than discarding them in swapchain mailboxes.
- **True Uncapped FPS Support**: Unlocked presentation pacing in `present_pace` and scaled dynamic delta-time floor down to 4.16 ms (240 FPS) when VSync is set to *Off (Uncapped)*, enabling framerates to soar beyond 60 FPS without game speed acceleration.
- **UI Text Overlap Protection**: Fixed text collision in `option_rows` by enforcing strict clipping boundaries between option labels, navigation chevrons, and value texts, while shortening VSync options for crystal-clear readability.

### v0.0.25
- **Menu Reorganization**: Separated into dedicated **Video** (Display, Resolution, Framerate, VSync) and **Advanced Video** (XeSS, FSR, Frame Gen, Shadows, Ambient Occlusion, SSR, SSGI) tabs.
- **Explicit VSync Selector**: Direct control over swapchain presentation modes:
  - *Off (Low Latency / Mailbox)*: Zero vblank tearing, non-blocking acquisition for high refresh rate and Frame Gen.
  - *On (VSync / 60 Hz - FIFO)*: Standard sync to display vertical refresh rate.
  - *Off (Unlimited / Immediate)*: Direct unbuffered presentation for raw latency testing.
- **Frame Generation 30 FPS Cap Resolution**: Fixed double-vblank blocking under FIFO when Frame Gen is engaged, preventing the game loop from dropping to 30 FPS.
- **PokeL-Extractor Integration**: Standalone ROM extractor bundled directly into release packages and CI artifacts.

### v0.0.24
- **Release Branding Cleanup**: Renamed application distribution to **PokeL-ARecomp** (`PokeL-ARecomp.exe`).
- **Official Arceus PE Icon**: Embedded high-resolution (16x16 up to 256x256) Arceus Celestial Wheel icon into Windows PE executable.
- **Log Streamlining**: Cleared legacy game log messages and aligned engine identifiers.
- **Intel Arc B580 Xe2 Tuning**: Dedicated 3.1 GB physical VRAM pool unlocked, preventing iGPU 1 GB fallback.
- **Intel Xeon Multi-Threading**: Dynamic worker thread scaling up to 8 parallel texture deswizzling threads.

### v0.0.23
- **Crash Fix (`0xc0000005`)**: Moved `dcache` and `jmps` state into per-thread CPU structures, eliminating memory access violations on launch.
- **Improved Game Loop Reliability**: Normalized initialization lifecycle for sound, filesystem, and NVN graphics.

### v0.0.22
- **Dynamic 60 FPS Engine**: Implemented dynamic delta-time pacing for *Pokémon Legends: Arceus*, allowing smooth 60 FPS gameplay without double-speed physics.

### v0.0.21
- **Vulkan HUD Decoupling**: Added `hdr_snapshot` isolation to prevent optical flow ghosting on HUD, minimap, and battle reticles during Frame Generation.

### v0.0.20
- **Intel XeSS SDK Integration**: Added AI neural upscaling support via Intel XeSS XMX engines and DP4a fallback.

---

## Credits & Acknowledgements

This project stands on the shoulders of giants. We express our deepest gratitude to the upstream authors and open-source pioneers whose breakthroughs made PokeL-ARecomp possible:

- **[Peiikai](https://github.com/peiikai)**: Creator of [LAS Recomp](https://github.com/peiikai/LASRecomp), who invented the static recompilation architecture, ARM64-to-C ahead-of-time code translation pipeline, fast-memory execution model, and Maxwell-to-SPIR-V shader translation.
- **[Hexveil404](https://github.com/Hexveil404)**: Creator of [PokeZ-ARecomp](https://github.com/Hexveil404/PokeZ-ARecomp), who extended LAS Recomp to modern Pokémon engine architectures, developed custom Switch OS HLE services, and proved that full Switch Pokémon titles can run natively on PC.
- **fabioschmidt01 & Antigravity**: Architecture, Intel Xeon multi-threading, Intel Arc B580 tuning, Intel XeSS integration, HUD decoupling, standalone extractor tool, and the Pokémon Legends: Arceus & Violet ports.
- **[Thealexbarney](https://github.com/Thealexbarney)**: Creator of [LibHac](https://github.com/Thealexbarney/LibHac), powering our standalone `PokeL-Extractor` ROM analysis and BKTR patch merger.
- **[Ryujinx Project](https://github.com/Ryujinx/Ryujinx)**: Exceptional behavioral reference and documentation for Nintendo Switch OS kernel and Maxwell NVN graphics behavior.
- **Open-source libraries**: SDL3, FFmpeg, Dear ImGui, Vulkan Memory Allocator, Shaderc, XeGTAO, AMD FidelityFX, Intel XeSS.

---

## Legal Notice

*PokeL-ARecomp* is a fan-created research and preservation project. It is **not** endorsed, affiliated with, or sponsored by Nintendo, Game Freak, Creatures Inc., or The Pokémon Company. *Pokémon* and *Pokémon Legends: Arceus* are registered trademarks of their respective owners. No proprietary Nintendo code, game binaries, textures, models, or font files are distributed in this repository.
