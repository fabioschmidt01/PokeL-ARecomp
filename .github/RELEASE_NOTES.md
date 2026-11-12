### What's new

- **Windowed Launcher Mode**: PokeL-ARecomp now always opens the start screen / launcher in windowed mode first (never fullscreen). Fullscreen activates automatically upon starting game play if enabled in settings.
- **"Extrair ROM (PokeL-Extractor)" Button**: Added a direct 1-click button on the setup screen to launch the standalone PokeL-Extractor utility.
- **Native In-Engine Draw Distance & Live Memory Patching** (F1 > Advanced Video & Game > Draw distance): *Standard (1x)*, *High (2.5x - Default)* or *Ultra (4x)*.
  - High: wild Pokémon and NPCs load at 100 m (Switch: 40 m / 60 m), trees visible up to 500 m (Switch: 200 m).
  - Ultra: Pokémon and NPCs load at 150 m, trees visible up to 800 m.
  - Real-time live memory patching: changing the setting in the F1 menu applies immediately in game without needing an area transition or restart. Settings save immediately.
- **Hisui Color Enhancement & Tonemapper** (F1 > Advanced Video > Color enhancement): *Hisui Vibrant* (+20% saturation, gentle contrast curve) and *Hisui Cinematic* (stronger contrast and atmosphere). Zero CPU overhead in Vulkan present shader.
- **Automatic Save Backup Rotation**: Creates timestamped backups in `save/backups/main_YYYYMMDD_HHMMSS.bak` whenever the game commits save data, retaining the 10 most recent copies.
