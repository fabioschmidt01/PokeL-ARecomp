### Highlights of Version v0.27.0

Background music, AMD FSR 3.1, cheats, shaders prepared before the game starts, the ultrawide reticle fix, a one-click bug report and a set of stability fixes.

> [!CAUTION]
> ### 🔊 Known issues in this version
> 1. **Audio hiss:** music plays and most sounds are normal in towns, the field and battles, but the game's sound comes out quieter than it should. Turning the volume up brings out a **hiss**, clearest in quiet scenes and on the **title screen**, whose soft music is buried under it ([#17](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/17)).
> 2. **Grey boxes on big titles:** when a large title leaves the screen (area names, *Strong / Agile Style* moves), **grey rectangles flash** where the text was. The text itself reads correctly.
>
> Both are known and being worked on: no need to report them again.
>
> 🇧🇷 **Problemas conhecidos nesta versão**
> 1. **Chiado no áudio:** a música toca e a maior parte dos sons sai normal na cidade, no campo e nas batalhas, mas o som sai mais baixo do que deveria. Ao aumentar o volume aparece um **chiado**, mais nítido em cenas silenciosas e na **tela de título**.
> 2. **Caixas cinza nos títulos grandes:** quando um título grande sai da tela (nomes de áreas, golpes em *Estilo Forte / Ágil*), **retângulos cinza aparecem** por um instante. O texto aparece certo.
>
> Os dois já são conhecidos e estamos trabalhando neles: não é preciso reportar de novo.

> [!TIP]
> ### 🆕 New: built-in bug report
> If the game crashes, click **Report on GitHub** in the crash window: it gathers your settings, logs and PC details into `logs/report-<date>/`, opens that folder and opens a GitHub issue already filled in. Drag the folder's files into the issue and tell us what you were doing. Nothing is sent by itself. For other bugs, attach `larecomp.ini` and the newest log from the **`logs`** folder to a [new issue](https://github.com/fabioschmidt01/PokeL-ARecomp/issues). [How to report a bug](https://github.com/fabioschmidt01/PokeL-ARecomp#reporting-a-bug).
>
> 🇧🇷 **Novo: envio de relatório embutido.** Se o jogo travar, clique em **Reportar no GitHub** na janela de crash: ele junta as configurações, os logs e os dados do PC em `logs/report-<data>/`, abre a pasta e abre uma issue já preenchida. Arraste os arquivos para a issue e conte o que estava fazendo. Nada é enviado sozinho. Para outros bugs, anexe o `larecomp.ini` e o log mais recente da pasta **`logs`** numa [issue nova](https://github.com/fabioschmidt01/PokeL-ARecomp/issues).

#### 🎵 Audio
- **Background music plays** ([#17](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/17)): the Switch's hardware Opus decoder (`hwopus`) is implemented on FFmpeg's Opus decoder. The music, ambient sounds and voices that the game streams through it were silent before; sound effects already played.

#### 🎨 Graphics
- **AMD FSR 3.1 — 🧪 Experimental:** a new choice in **F1 > Advanced Video > Temporal anti-aliasing**. More detail in motion and fewer trails than FSR 2. Fast-moving objects can still leave a slight trail.
- **Ultrawide HUD** ([#15](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/15)): at 21:9 / 32:9 the capture reticle, the "!" over a Pokémon that saw you, speech bubbles and map markers now sit on what they point at, instead of halfway to the centre.
- **FSR in motion:** the grainy look while walking with a wide field of view is fixed (the motion vectors counted the FOV change twice).

#### 🎮 Cheats — 🧪 Experimental
- Put an Atmosphere cheat file for 1.1.1 (`AEE8F150DDA1B5A8.txt`, or the whole `atmosphere` folder) in the `cheats` folder next to `PokeL-ARecomp.exe` and switch them in **F1 > Cheats**.
- Value cheats (money, items...) work at once. Code cheats (catch rate...) are built into the game code: restart after switching one and the start screen recompiles the game (like the first build; turning them all off goes back to the normal build).
- A cheat can break a save or crash the game: keep a backup (`save/backups/`) and turn cheats off before reporting a bug.

#### ⚡ Performance
- **Shaders prepared before the game starts:** after PLAY the game's known shaders are compiled up front (about 10 s the first time on a new PC, cached afterwards): far fewer hitches the first time each effect shows up.
- **Tip:** overlays that hook into the game (RivaTuner / MSI Afterburner, OBS game capture, Discord overlay...) are expensive here. On the test machine closing them cut the worst hitch while turning the camera from 244 ms to 82 ms ([#10](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/10)).

#### 🐞 Reporting bugs
- **"Report on GitHub" in the crash window:** one click gathers your `larecomp.ini`, the logs and your PC's details (version, OS, CPU, RAM, GPU and driver) into `logs/report-<date>/`, opens that folder, and opens a new GitHub issue already filled in. Just drag the folder's files into it. Nothing is sent by itself.
- **All logs in the `logs` folder:** `lasrecomp.log` and its backups no longer clutter the program's folder (old ones are moved in on the first start).

#### 🖥️ Start screen
- **Full screen or window?** PLAY now asks how you want to play, with a "Don't ask again" box (F1 > Video changes it later). The start screen's picture no longer sits in a corner of a black full screen while the game starts.
- **`lasrecomp.exe` is gone:** the game code loads from `PokeL-ARecomp.exe` itself; the old copy is removed.
- The standalone extractor is set aside for now; the start screen's own setup reads your `.nsp` / `.xci`.
- Leftover "ZA Recomp" / "LAS Recomp" names in error windows and messages now read PokeL-ARecomp ([#23](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/23)).

#### 🛠️ Stability and fixes
- **Name entry:** typing works again after the window lost focus ([#20](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/20)).
- **Integrated GPUs with little video memory:** textures stay within its budget, the rest in system memory ([#19](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/19)).
- **Safety nets:** a draw whose indices lie outside the game's memory is skipped, and a damaged game-file index fails cleanly instead of crashing.
- **Textures:** a texture the game rewrote is no longer left stale.
- **Linux AppImage:** the Xbox / PlayStation button icons are included.

#### 🙏 Thanks
To the upstream [PokeZ-ARecomp](https://github.com/Hexveil404/PokeZ-ARecomp) project for several of the fixes above, and to everyone who sent logs and screenshots.

---

#### 🇧🇷 Resumo em português
- **AMD FSR 3.1 (🧪 experimental)** em F1 > Vídeo Avançado, e o granulado do FSR ao andar com FOV alto foi corrigido.
- **Cheats (🧪 experimental):** arquivos do Atmosphere 1.1.1 na pasta `cheats`, ligados em F1 > Cheats. Cheats de código recompilam o jogo ao reiniciar. Faça backup do save.
- **Shaders preparados antes do jogo abrir:** bem menos engasgos na primeira vez de cada efeito.
- **Ultrawide** ([#15](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/15)): mira, "!", balões e marcadores agora ficam no lugar certo em 21:9 / 32:9.
- **Dica:** feche overlays como RivaTuner/MSI Afterburner e OBS: eles causam boa parte das travadas ao girar a câmera ([#10](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/10)).
- **A música de fundo toca** ([#17](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/17)). Ainda há um chiado em cenas silenciosas (veja o aviso acima).
- **Botão "Reportar no GitHub" na janela de crash:** junta o `larecomp.ini`, os logs e os dados do PC numa pasta e abre uma issue já preenchida.
- **Logs na pasta `logs`**, pergunta **tela cheia ou janela** ao clicar em JOGAR, fim do `lasrecomp.exe` e dos nomes antigos nas mensagens.
- **Correções:** digitação do nome (#20), GPUs integradas com pouca memória (#19), texturas desatualizadas, proteções contra arquivos danificados e ícones de controle no AppImage do Linux.
