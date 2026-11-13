### Highlights of Version v0.26.6

> [!WARNING]
> 🧪 **Ultrawide support is experimental.** It was tested on a real 32:9 monitor in the open world and on simulated 32:9 / 21:9 windows, but **battles, cutscenes, interiors, menus and the map are not fully tested yet**, so bugs may occur (objects popping in at the screen edges, effects or UI out of place). Please report anything odd in [issue #1](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/1) with your `lasrecomp.log` and `larecomp.ini`.
>
> 🇧🇷 **O suporte a ultrawide é experimental.** Foi testado num monitor 32:9 real no mundo aberto e em janelas 32:9 / 21:9 simuladas, mas **batalhas, cutscenes, interiores, menus e o mapa ainda não foram 100% testados**, então podem ocorrer bugs (objetos aparecendo/sumindo nas bordas, efeitos ou interface fora do lugar). Reporte o que encontrar na [issue #1](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/1) com o seu `lasrecomp.log` e `larecomp.ini`.

- **🖥️ Ultrawide 21:9 / 32:9 fixed (Hor+, experimental)** — [#1](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/1), camera and HUD fix by [@didslima](https://github.com/didslima) in [#6](https://github.com/fabioschmidt01/PokeL-ARecomp/pull/6) — thank you!
  - The picture is no longer cropped to a stretched 16:9 window in the middle of the screen: v0.26.5 still cut every frame down to the centre of the wide image (a Link's Awakening-only rule that was always on in Arceus).
  - The game's own camera now takes the window's shape where its projection is built (one generator patch in the projection builder): every perspective camera — field, battles, cutscenes — is Hor+ on wider screens (same vertical view, more on the sides) and Vert+ on taller ones, and what the game derives from the camera sees the wider view too.
  - The HUD keeps the game's 16:9 layout and shape, centred (no more 2x-wide Poké Balls); full-screen fades and backdrops still fill the window.
  - Choose it in **F1 > Video > Aspect ratio > Unlocked (ultrawide)** (also asked on the first PLAY); *Original 16:9* keeps the game's framing with side bars. The window's shape is detected automatically.
  - ⚙️ **The start screen rebuilds the game code once** after updating (the camera fix is part of it).

- **📷 FOV slider fixed**: **F1 > Video > Field of view** now changes the real scene camera (v0.26.5 scaled the per-object matrices by mistake, so the slider had no visible effect on the view). It works together with ultrawide.

- **🎮 Button icons follow your bindings (Xbox & PlayStation)**:
  - The icon overlays were each made for one layout (Xbox by position: the Switch's A shown as **B**; PlayStation: A as cross), so players who mapped by letter (`bind_pad_a = south`) were told to press **B** while **A** was the button.
  - Each A / B / X / Y icon — HUD, menus, battle, and the title's "Press …" in every language — is now redrawn with the symbol of the controller button actually bound to it, for any remapping.
  - Bindings changed in the Controls tab apply from the next start.

- **🔧 Diagnostics** (for bug reports): `LARECOMP_NO_WIDESCREEN=1` gives the game's own 16:9 camera; `LARECOMP_WIDE_UI_BOX=0` the old (stretched) HUD; `LARECOMP_PLA_BOX=1` the old 16:9 crop; `LARECOMP_LOG_CAM=1` logs the camera the FOV slider scales.
