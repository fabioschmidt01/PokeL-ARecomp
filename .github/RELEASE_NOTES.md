### Highlights of Version v0.26.6

> [!WARNING]
> 🧪 **Ultrawide support is experimental.** It was tested on a simulated 32:9 window, **not yet on real 21:9 / 32:9 monitors**, so bugs may occur (objects popping in at the screen edges, effects or UI out of place). Please report anything odd in [issue #1](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/1) with your `lasrecomp.log` and `larecomp.ini`.
>
> 🇧🇷 **O suporte a ultrawide é experimental.** Foi testado numa janela 32:9 simulada, **ainda não em monitores 21:9 / 32:9 reais**, então podem ocorrer bugs (objetos aparecendo/sumindo nas bordas, efeitos ou interface fora do lugar). Reporte o que encontrar na [issue #1](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/1) com o seu `lasrecomp.log` e `larecomp.ini`.

- **🖥️ Ultrawide 21:9 / 32:9 fixed (Hor+, experimental)** — [#1](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/1):
  - The picture is no longer cropped to a stretched 16:9 window in the middle of the screen: v0.26.5 still cut every frame down to the centre of the wide image (a Link's Awakening-only rule that was always on in Arceus).
  - The 3D camera is now widened correctly: the scene camera (view / projection / view-projection, slot 5) is found and scaled, instead of the per-object matrices v0.26.5 picked by mistake, which squashed objects while the view stayed 16:9.
  - Wider screens show more of the world at the sides with correct proportions; the HUD and dialogue boxes keep their shape. Taller windows keep the horizontal view.
  - Choose it in **F1 > Video > Aspect ratio > Unlocked (ultrawide)** (also asked on the first PLAY); *Original 16:9* keeps the game's framing with side bars. The window's shape is detected automatically.
  - The FOV slider works together with it, and shadow maps / 16:9 off-screen passes are left untouched.

- **🎮 Button icons follow your bindings (Xbox & PlayStation)**:
  - The icon overlays were each made for one layout (Xbox by position: the Switch's A shown as **B**; PlayStation: A as cross), so players who mapped by letter (`bind_pad_a = south`) were told to press **B** while **A** was the button.
  - Each A / B / X / Y icon — HUD, menus, battle, and the title's "Press …" in every language — is now redrawn with the symbol of the controller button actually bound to it, for any remapping.
  - Bindings changed in the Controls tab apply from the next start.

- **🔧 Diagnostics** (for bug reports): `LARECOMP_LOG_CAM=1` logs the camera the ultrawide fix found; `LARECOMP_PLA_BOX=1` restores the old 16:9 crop; `LARECOMP_CAM_SLOT=-1` turns the camera fix off.
