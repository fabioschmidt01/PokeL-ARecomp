# PokeL-ARecomp on Android (GameHub) and Windows on ARM

`PokeL-ARecomp-windows-arm64.zip` is a Windows ARM64 build for Android devices running Windows games through
**GameHub**, and for Windows on ARM PCs. As with the x64 release, it contains no game code or game files: the start
screen imports your own dump and recompiles the game on the device.

## Why a separate build

- **Address space.** The recomp maps the Switch's memory 1:1 at fixed addresses. The x64 build uses 0x70..0x80 GB.
  Android's ARM64 kernels usually give an app 39 bits of address space (512 GB), and Android has already placed its own
  libraries at the top of it, so that window is never free. This build moves the guest window to 0x10..0x20 GB
  (`GUEST_SHIFT=-0x6000000000`, `runtime/rt.h`). This is why the x64 build stops at start under GameHub with
  `guest address range 7000000000-8000000000 is not free`.
- **Native code.** The recompiled game code is ARM64 code here (`--simd asm`), so it runs directly instead of through
  the x64 emulator (FEX). This is the difference between about 8 and about 30 fps.
- **GPU without host memory import.** Turnip (Mesa's Adreno driver, used by GameHub) has no
  `VK_EXT_external_memory_host`. Without it, shaders that read the guest's memory directly saw zeros: the dialogue text
  and the menus were empty and the player character was invisible. A shadow buffer that is filled before each draw now
  stands in for the import (`runtime/nvn/vulkan/exec.c`, `gsh_*`). It switches on by itself only when the driver lacks
  the import; PCs keep the direct import.

## Requirements

- A Snapdragon device with an Adreno 7xx or 8xx GPU, 12 GB RAM or more.
- GameHub with the **proton11.0-arm64x** Wine and a **Turnip** driver. Tested with turnip_v26.1.0_b8.
- A Windows PC (x64) for the one-time setup: it imports your dump and compiles the game code for ARM64 (see below).
- About 8 GB free on the device for the finished folder; on the PC about 25 GB during the setup.
- Your own game dump: Pokémon Legends: Arceus **1.1.1** (base `.nsp`/`.xci` plus the 1.1.1 update, or a dump that already
  contains it) and the `prod.keys` from your console.

Tested on a RedMagic tablet (Snapdragon 8 Elite, Adreno 830): about 30 fps in the open world at 1280x800, game speed
0.95-1.0x.

## Setup

**Why on a PC:** the start screen can set the game up by itself, as on PC, but on a tablet its code generator runs
emulated (an x64 program that itself emulates ARM64 code to find jump tables): hours instead of minutes. On the PC
everything runs natively.

On the PC:

1. Unzip `PokeL-ARecomp-windows-arm64.zip`.
2. Put your dump and `prod.keys` into a folder named `gamefiles` inside it (next to `PC-Setup.bat`).
3. Double-click **`PC-Setup.bat`**. It imports the game into `extracted\` and compiles the game code for ARM64
   (`extracted\lasgame.dll`). About 10–15 minutes on a 16-thread PC, once.
4. Copy the whole `PokeL-ARecomp` folder to the device, e.g. to `Documents/PokeL-ARecomp` in internal storage. The
   `gamefiles` folder can stay behind.

On the device:

5. In GameHub: **Library → Import → Game File (.exe)**, select `PokeL-ARecomp.exe`, then **Confirm**.
6. In the new entry's settings (**More → PC Engine**): compatibility layer **proton11.0-arm64x**, the default FEX
   preset, a Turnip GPU driver, and the resolution of your screen (e.g. 1280x800 on a 16:10 tablet). The first three are
   GameHub's defaults.
7. Start it and press **Play**. A controller connected to the device works as in GameHub.

## Notes

- Settings are the same as on PC (F1 menu, or Minus+Plus on a controller). The first start uses fullscreen, 30 fps and
  1x resolution.
- Saves are in `save/` next to the exe; a PC save (`save/save_0`) can be copied over.
- Logs: `logs/play.log` next to the exe.
- The helper programs (`lasgen`, `zaprompts`, `zaimport`, `PokeL-Extractor`) are the x64 ones; `PC-Setup` runs them on
  the PC. Only the game itself and its compiler (`zig\zig.exe`; `zig\zig-x64.exe` is the PC's) are ARM64.
- Windows on ARM PCs (untested): the start screen's own setup works there in principle (native compiler, generator
  emulated by Windows); if it is slow, use `PC-Setup.bat` on an x64 PC and copy the folder over.
- In GameHub, the start screen's **Choose...** opens its folder dialog behind the window, where it can't be reached; a
  `gamefiles` folder next to the exe is picked by itself.

## Building it

`tools/za/dist-arm64.sh` on an x64 Windows PC; its header lists the toolchain (MSYS2's clangarm64 packages, llvm-mingw).
