<div align="center">

<img src="docs/icon/icon_1024.png" alt="PokeL-ARecomp Icon" width="140">

# PokeL-ARecomp (PLA Recomp)

**A native PC port of *Pokémon Legends: Arceus*, built by static recompilation.**

**English** · [Português (Brasil)](README.pt-BR.md)

[Report an issue](https://github.com/fabioschmidt01/PokeL-ARecomp-privado-teste/issues) ·
[Releases](https://github.com/fabioschmidt01/PokeL-ARecomp-privado-teste/releases)

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
- **Native 60 FPS** at regular game speed or 30 FPS console parity.
- **Internal Resolution Scaling**: From 25% up to 4x native 1080p (4K UHD) with Dynamic Resolution scaling.
- **High Core-Count Scaling (Intel Xeon & Multi-Core CPUs)**: Dynamic worker thread scaling (up to 8 parallel texture deswizzling threads on 16-24+ thread CPUs) eliminating texture streaming stutter.
- **Dedicated GPU Memory Unlocking (Intel Arc & High-VRAM GPUs)**: Full 3.1 GB physical pool unlocked for GPUs with dedicated VRAM (such as the 12 GB Intel Arc B580 Battlemage), preventing memory thrashing.

### Graphics & Upscaling
- **Intel XeSS (Xe Super Sampling)**: AI neural upscaling accelerated by hardware XMX engines on Intel Arc GPUs.
- **AMD FidelityFX FSR 1, FSR 2 (temporal)**, and **SMAA** anti-aliasing.
- **Custom Shading & Lighting**: Screen-space ambient occlusion (XeGTAO), water reflections, screen-space GI, and high-resolution shadow mapping.
- **HDR Display Output**: scRGB / HDR10 support.
- **HD Texture Pack Loader**: Direct loading from `textures/hd/`.

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

1. Download the latest release from the [Releases](https://github.com/fabioschmidt01/PokeL-ARecomp-privado-teste/releases) tab (`PokeL-ARecomp-windows-x64.zip` for Windows, or `.AppImage` for Linux).
2. Extract the archive into a writable folder (e.g. `C:\Games\PokeL-ARecomp`).
3. Prepare your game files:
   - Base game (`.nsp` or `.xci`), the **1.1.1 update** (`.nsp`), and your `prod.keys` dumped from your console.
4. Launch `lasrecomp.exe` (or `./plarecomp.AppImage`).
5. In the start screen, choose your game folder and click **Set up the game**.
6. The initial static recompilation takes 10 to 25 minutes depending on your CPU core count (this occurs **only once**).
7. Click **Play** and enjoy exploring Hisui!

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

## Credits & Acknowledgements

- **Peiikai**: Creator of [LAS Recomp](https://github.com/peiikai/LASRecomp), the static recompiler, runtime architecture, and Maxwell shader translator.
- **Hexveil404**: Creator of [PokeZ-ARecomp](https://github.com/Hexveil404/PokeZ-ARecomp), introducing Switch OS HLE services and Pokémon decompilation paradigms.
- **Ryujinx Project**: Behavioral reference for Switch kernel and NVN GPU execution.
- **Open-source libraries**: SDL3, FFmpeg, Dear ImGui, Vulkan Memory Allocator, Shaderc, XeGTAO, LibHac, Intel XeSS SDK.

---

## Legal Notice

*PokeL-ARecomp* is a fan-created research and preservation project. It is **not** endorsed, affiliated with, or sponsored by Nintendo, Game Freak, Creatures Inc., or The Pokémon Company. *Pokémon* and *Pokémon Legends: Arceus* are registered trademarks of their respective owners. No proprietary Nintendo code, game binaries, textures, models, or font files are distributed in this repository.
