<div align="center">

<img src="docs/icon/icon_1024.png" alt="PokeL-ARecomp Icon" width="140">

# PokeL-ARecomp (PLA Recomp)

**A native PC port of *Pokémon Legends: Arceus*, built by static recompilation.**

**English** · [Português (Brasil)](README.pt-BR.md)

[Report an issue](https://github.com/fabioschmidt01/PokeL-ARecomp/issues) ·
[![Latest release](https://img.shields.io/github/v/release/fabioschmidt01/PokeL-ARecomp?label=latest)](https://github.com/fabioschmidt01/PokeL-ARecomp/releases/latest) ·
[Download](https://github.com/fabioschmidt01/PokeL-ARecomp/releases/latest)

</div>

---

> [!CAUTION]
> ## 🔊 Known issues in this version
>
> **1. Audio hiss.** Background music plays and most sounds are normal in towns, the field and battles, but the game's sound comes out quieter than it should. Turning the volume up brings out a **hiss**, heard most clearly in quiet scenes and on the **title screen**, whose soft music is buried under it.
>
> **2. Grey boxes on big titles.** When a large title leaves the screen (area names, *Strong Style* / *Agile Style* moves), **grey rectangles flash** where the text was, then disappear. The text itself reads correctly.
>
> Both are known and **being worked on**. You don't need to report them again.

> [!WARNING]
> ### ⚠️ Projeto em Desenvolvimento Ativo / Project in Active Development
>
> 🇧🇷 **Aviso da Comunidade:**  
> Este é um projeto de código aberto em estágio ativo de desenvolvimento. Durante os testes internos (feitos por mim e amigos da comunidade), observamos que **bugs visuais, quedas temporárias de performance em transições (telas de carregamento, início de batalhas e fades) e eventuais travamentos/crashes são esperados**.
> 
> Pedimos a sua paciência e compreensão! Se você encontrar qualquer problema ou crash, **nos ajude reportando na aba [Issues](https://github.com/fabioschmidt01/PokeL-ARecomp/issues)** anexando o `larecomp.ini` e os logs da pasta `logs` (se o jogo travar, o botão **Reportar no GitHub** da janela de crash junta tudo e abre a issue preenchida). Com o seu feedback, conseguiremos lapidar e melhorar o port cada vez mais rápido!
>
> ---
>
> 🇺🇸 **Community Notice:**  
> This is an open-source project under active development. Throughout testing (conducted by myself, friends, and community testers), we have observed that **visual glitches, temporary performance drops during transitions (loading screens, battle entries, fades), and occasional crashes are expected**.
>
> We kindly ask for your patience and understanding! If you encounter any bugs or crashes, **please help us by opening a report in the [Issues](https://github.com/fabioschmidt01/PokeL-ARecomp/issues) tab**, attaching your `larecomp.ini` and the logs from the `logs` folder (after a crash, the crash window's **Report on GitHub** button gathers everything and opens the issue already filled in). Your feedback is essential to helping us improve the port as quickly as possible!

**PokeL-ARecomp** (PLA Recomp) turns *Pokémon Legends: Arceus* Nintendo Switch code into a native 64-bit Windows and Linux program.
The game's ARM64 machine code is translated ahead-of-time (AOT) into C and compiled natively for x86-64. The Nintendo Switch OS kernel, filesystem, audio and Maxwell NVN graphics are reimplemented directly on top of Vulkan.

**No emulator runs while you play.** The game executes natively on your CPU and GPU.

> [!IMPORTANT]
> **You need your own copy of the game.** PokeL-ARecomp does not bundle any game code, ROMs, assets, textures or encryption keys. You must dump your legally purchased game (`.nsp` / `.xci` + update 1.1.1) and `prod.keys` from your own console.

---

## Features

### Performance & Engine
- **30 FPS (recommended) or 60 FPS (⚠️ experimental)**: 30 FPS runs everything at the Switch's intended speed. At 60 FPS camera and movement keep their speed through dynamic delta-time pacing, but parts of the game count frames instead of time and run **twice as fast**: thrown food disappears before wild Pokémon can eat it ([#9](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/9)), and menus, dialogue and some effects are faster. These are **known, long-standing limits of Pokémon Legends: Arceus at 60 FPS** — no 60 FPS patch for this game (emulator cheats, FPSLocker) has fixed them — not new bugs of this port. Switch to 30 FPS to feed Pokémon. The frame rate is unsteady in the open world (around 40–50 fps on the test machine), with brief hitches the first time each shader is compiled.
- **Frame Generation (2x)**: shows an extra frame between every two game frames — **60 on screen with the game at 30 FPS** (its logic stays at the Switch's speed) or 120 at 60 FPS. The extra frame is a blend of its two neighbours (no motion estimation yet): smoother in slow camera pans, with ghosting on fast movement. Works on any GPU.
- **HUD Decoupling**: Isolates 2D UI elements (minimap, health gauges, reticles, text) via `hdr_snapshot` to eliminate ghosting artifacts during Frame Generation.
- **Explicit VSync Modes**: Choose between *Off (Low Latency / Mailbox)*, *On (VSync / 60 Hz - FIFO)*, or *Off (Unlimited / Immediate)* with instant swapchain recreation.
- **VSync: Off (Unlimited)**: presents without waiting for the display (lowest latency, may tear), but always capped at the chosen 30 or 60 FPS — running faster made the frame-counting systems (menus, thrown food) faster still.
- **Internal Resolution Scaling**: From 25% up to 4x native 1080p (4K UHD) with Dynamic Resolution scaling, plus a **720p (Switch handheld)** option, the default of the *Light* profile, for Steam Deck, integrated GPUs and entry-level cards.
- **CPU / GPU overlap (automatic)**: the CPU builds the next frame while the GPU draws the current one. It is turned off automatically on CPUs with 8 threads or fewer and on AMD GPUs, where it crashed in the past (override in **F1 > Advanced Video > Compatibility**, or with `LARECOMP_GPU_OVERLAP=1` / `0`).
- **High Core-Count Scaling (Intel Xeon & Multi-Core CPUs)**: Dynamic worker thread scaling (up to 8 parallel texture deswizzling threads on 16-24+ thread CPUs) eliminating texture streaming stutter.
- **Dedicated GPU Memory Unlocking (Intel Arc & High-VRAM GPUs)**: Full 3.1 GB physical pool unlocked for GPUs with dedicated VRAM (such as the 12 GB Intel Arc B580 Battlemage), preventing memory thrashing.

### Graphics & Upscaling
- **Separated Options Menu**: Dedicated **Video** (display, resolution, fps, vsync) and **Advanced Video** (XeSS, FSR, Frame Gen, shadows, AO, SSR, GI) tabs for clean customization.
- **Native In-Engine Draw Distance**: Adjustable entity and vegetation rendering radius in C code without external mods (*Standard 1x*, *High 2.5x*, *Ultra 4x*). Wild Pokémon and NPC spawn distance up to 150m, trees visible up to 800m.
- **Hisui Color Enhancement & Tonemapper**: High-fidelity Vulkan presentation post-processing filters (*Hisui Vibrant* and *Hisui Cinematic*) removing the washed-out gray haze with zero CPU overhead.
- **Intel XeSS (Xe Super Sampling)**: AI neural upscaling accelerated by hardware XMX engines on Intel Arc GPUs (Ultra Performance to Native AA).
- **AMD FidelityFX FSR 1, FSR 2 (temporal)**, and **SMAA** anti-aliasing.
- **AMD FSR 3.1 (temporal) — 🧪 Experimental**: **F1 > Advanced Video > Temporal anti-aliasing > AMD FSR 3.1**. More detail in motion and fewer trails than FSR 2. Fast-moving objects can still leave a slight trail (motion is estimated from the camera only).
- **Contrast-Adaptive Sharpening (RCAS)**: Real-time image clarity adjustment.
- **Custom Shading & Lighting**: Screen-space ambient occlusion (XeGTAO), water reflections (SSR), screen-space indirect lighting (SSGI), and high-resolution shadow mapping (up to 4x).
- **HDR Display Output**: scRGB / HDR10 support with peak brightness tuning.
- **HD Texture Pack Loader**: Direct loading from `textures/hd/`.
- **Ultrawide 21:9 / 32:9 (Hor+) — 🧪 Experimental**: with **F1 > Video > Aspect ratio > Unlocked (ultrawide)** the image fills any window shape: wider screens show more of the world at the sides, with no stretching. The window's shape is detected automatically. *Tested on a real 32:9 monitor in the open world, but battles, cutscenes, interiors and menus are not fully tested yet — bugs may occur* (objects popping in at the screen edges, effects or UI out of place). Please report them in [issue #1](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/1). *Original 16:9* keeps the game's framing with side bars.

- **HUD that follows the world in ultrawide** ([#15](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/15)): the capture reticle, the "!" over a Pokémon that saw you, speech bubbles and map markers now sit on the Pokémon or place they mark at 21:9 / 32:9; the rest of the HUD stays in its 16:9 layout.

- **Field of view slider**: **F1 > Video > Field of view** (45–90) widens the game's own camera. Scenery, particles, torch fire, Pokémon eyes and alert marks and the damage vignette all follow it, and the title screen keeps its framing.

### Cheats — 🧪 Experimental
- Atmosphere cheat files for version 1.1.1 (`AEE8F150DDA1B5A8.txt`, or a whole `atmosphere` folder) go in the `cheats` folder next to `PokeL-ARecomp.exe`. **F1 > Cheats** lists them with a switch each.
- **Cheats that change values** (money, items...) work at once. **Code cheats** (catch rate...) change the game's code: after switching one, restart and the start screen recompiles the game with it (like the first build).
- A cheat can break a save or crash the game: keep a backup (`save/backups/`) and turn cheats off before reporting a bug.

### Stability
- **Shaders prepared before the game starts**: after PLAY, the game's known shaders are compiled up front (a few seconds the first time, cached afterwards), so there are far fewer hitches the first time each effect appears.
- **Performance tip:** overlays that hook into the game (RivaTuner / MSI Afterburner, OBS game capture, Discord overlay...) cost a lot here: on the test machine closing them cut the worst camera-turn hitch from 244 ms to 82 ms ([#10](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/10)).
- **Command lists are protected from being overwritten**: the game hands its command memory out again much sooner than this port can translate it. Lists are now kept in the port's own memory until they are translated. This fixed the crashes in Jubilife's Floaro Main Street ([#7](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/7), [#12](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/12)).
- **Offline network service**: features that try to go online (for example *Connect to the internet* in the menu) are told the Switch is offline instead of crashing the game.
- **Diagnostic log**: `logs/lasrecomp.log` shows game speed vs real time (`[speed]`, every 10 s) and every setting changed in game (`[settings] changed`), which helps with bug reports.

### Saves & Safety
- **Automatic Save Backup Rotation**: Creates timestamped backups in `save/backups/main_YYYYMMDD_HHMMSS.bak` whenever the game commits save data, retaining the 10 most recent copies.

### Controls & Input
- Any modern gamepad via SDL3: Xbox, DualShock 4, DualSense, Nintendo Switch Pro Controller, Steam Deck.
- **Button icons that follow your bindings**: Xbox and PlayStation icons show the button you actually press, whether your controller is mapped by position (Switch layout) or by letter (`bind_pad_a = south`: Xbox A / cross confirms). Changes apply from the next start.
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

1. Download the latest release from the [Releases](https://github.com/fabioschmidt01/PokeL-ARecomp/releases/latest) page (`PokeL-ARecomp-windows-x64.zip` for Windows, or `.AppImage` for Linux).
2. Extract the archive into a writable folder (e.g. `C:\Games\PokeL-ARecomp`).
3. Prepare your game files:
   - Base game (`.nsp` or `.xci`), the **1.1.1 update** (`.nsp`), and your `prod.keys` dumped from your console.
4. Launch `PokeL-ARecomp.exe` (or `./plarecomp.AppImage`).
5. In the start screen, choose your game folder and click **Set up the game**.
6. The initial static recompilation takes 10 to 25 minutes depending on your CPU core count (this occurs **only once**).
7. Click **Play** and enjoy exploring Hisui!

---

## Reporting a Bug

PokeL-ARecomp now has a **basic built-in bug report** (🆕 v0.27):

- **If the game crashes:** the crash window has a **Report on GitHub** button. One click gathers your `larecomp.ini`, the logs and your PC's details (version, OS, CPU, RAM, GPU and driver) into `logs/report-<date>/`, opens that folder and opens a new GitHub issue already filled in. **Drag the files from that folder into the issue** and add what you were doing. Nothing is sent by itself: you see everything before posting.
- **Any other bug** (graphics, sound, controls...): open a [new issue](https://github.com/fabioschmidt01/PokeL-ARecomp/issues) and attach `larecomp.ini` and the newest `lasrecomp*.log` from the **`logs`** folder next to `PokeL-ARecomp.exe`. A screenshot (**F12**) helps a lot.
- Turn cheats off and say which mods you use: many bugs come from them.

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

This project stands on the shoulders of giants. We express our deepest gratitude to the upstream authors and open-source pioneers whose breakthroughs made PokeL-ARecomp possible:

- **[Peiikai](https://github.com/peiikai)**: Creator of [LAS Recomp](https://github.com/peiikai/LASRecomp), who invented the static recompilation architecture, ARM64-to-C ahead-of-time code translation pipeline, fast-memory execution model, and Maxwell-to-SPIR-V shader translation.
- **[Hexveil404](https://github.com/Hexveil404)**: Creator of [PokeZ-ARecomp](https://github.com/Hexveil404/PokeZ-ARecomp), who extended LAS Recomp to modern Pokémon engine architectures, developed custom Switch OS HLE services, and proved that full Switch Pokémon titles can run natively on PC.
- **fabioschmidt01 & Antigravity**: Architecture, Intel Xeon multi-threading, Intel Arc B580 tuning, Intel XeSS integration, HUD decoupling, and the Pokémon Legends: Arceus & Violet ports.
- **[didslima](https://github.com/didslima)**: Ultrawide (21:9 / 32:9) support ([#6](https://github.com/fabioschmidt01/PokeL-ARecomp/pull/6)): the Hor+ / Vert+ camera fix in the game's own projection builder, the HUD kept in its 16:9 layout, the automated projection tests, and validation on a real 32:9 monitor.
- **[Thealexbarney](https://github.com/Thealexbarney)**: Creator of [LibHac](https://github.com/Thealexbarney/LibHac), used by the start screen to read `.nsp` / `.xci` dumps and merge the update (BKTR).
- **[Ryujinx Project](https://github.com/Ryujinx/Ryujinx)**: Exceptional behavioral reference and documentation for Nintendo Switch OS kernel and Maxwell NVN graphics behavior.
- **Open-source libraries**: SDL3, FFmpeg, Dear ImGui, Vulkan Memory Allocator, Shaderc, XeGTAO, AMD FidelityFX, Intel XeSS.

---

## Legal Notice

*PokeL-ARecomp* is a fan-created research and preservation project. It is **not** endorsed, affiliated with, or sponsored by Nintendo, Game Freak, Creatures Inc., or The Pokémon Company. *Pokémon* and *Pokémon Legends: Arceus* are registered trademarks of their respective owners. No proprietary Nintendo code, game binaries, textures, models, or font files are distributed in this repository.
