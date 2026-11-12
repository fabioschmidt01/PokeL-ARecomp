### Highlights of Version v0.26.5

- **🎮 Native Controller Button Prompts (DualSense / PS4 & Xbox)**:
  - Added native dynamic button prompt switching for PlayStation (DualSense / DualShock 4) and Xbox controllers.
  - Automatic gamepad detection via SDL with optional manual override in **F1 > Game / Controls** or `larecomp.ini` (`prompts = auto | nintendo | playstation | xbox`).
  - Seamless virtual RomFS overlay integration: prompts are injected at runtime without altering game files or requiring external mod managers.
  - Cleaned controller schematics in the in-game control guide screen (removed third-party handheld graphics).

- **📷 3D Field of View (FOV) & Native Ultrawide (Hor+) Support**:
  - Universal Vulkan camera projection matrix scaling (`runtime/nvn/vulkan/exec.c`): properly intercepts 3D scene camera matrices in Pokémon Legends: Arceus.
  - Dynamic interactive 3D FOV slider from 45° to 90° in **F1 > Advanced Video** and **F1 > Game**.
  - Native Ultra-wide (21:9 & 32:9) rendering with zero horizontal stretching: expands horizontal field of view (Hor+) across the display without pillarbox black bars or distorted geometry.

- **✨ Driver Compatibility & Stability**:
  - Enhanced compatibility with AMD RDNA3 / Mesa RADV Vulkan drivers.
