### Highlights of Version v0.26.5

- **🎮 Native Controller Button Prompts (DualSense / PS4 & Xbox)**:
  - Added native dynamic button prompt switching for PlayStation (DualSense / DualShock 4) and Xbox controllers.
  - Automatic gamepad detection via SDL with optional manual override in **F1 > Game / Controls** or `larecomp.ini` (`prompts = auto | nintendo | playstation | xbox`).
  - Seamless virtual RomFS overlay integration: prompts are injected at runtime without altering game files or requiring external mod managers.
  - Cleaned controller schematics in the in-game control guide screen.
- **✨ Stability & Shader Improvements**:
  - Enhanced compatibility with AMD RDNA3 / Mesa RADV Vulkan drivers.
