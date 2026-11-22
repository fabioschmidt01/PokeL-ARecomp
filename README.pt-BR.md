<div align="center">

<img src="docs/icon/icon_1024.png" alt="Ícone PokeL-ARecomp" width="140">

# PokeL-ARecomp (PLA Recomp)

**Port nativo para PC de *Pokémon Legends: Arceus*, construído por recompilação estática.**

[English](README.md) · **Português (Brasil)**

[Relatar um problema](https://github.com/fabioschmidt01/PokeL-ARecomp/issues) ·
[![Última versão](https://img.shields.io/github/v/release/fabioschmidt01/PokeL-ARecomp?label=latest)](https://github.com/fabioschmidt01/PokeL-ARecomp/releases/latest) ·
[Download](https://github.com/fabioschmidt01/PokeL-ARecomp/releases/latest)

</div>

---

> [!CAUTION]
> ## 🔊 Problemas conhecidos nesta versão
>
> **1. Chiado no áudio.** A música de fundo toca e a maior parte dos sons sai normal na cidade, no campo e nas batalhas, mas o som do jogo sai mais baixo do que deveria. Ao aumentar o volume aparece um **chiado**, mais nítido em cenas silenciosas e na **tela de título**, onde a música suave fica escondida debaixo dele.
>
> **2. Caixas cinza nos títulos grandes.** Quando um título grande sai da tela (nomes de áreas, golpes em *Estilo Forte* / *Estilo Ágil*), **retângulos cinza aparecem** por um instante onde estava o texto e depois somem. O texto em si aparece certo.
>
> Os dois já são conhecidos e **estamos trabalhando neles**. Não é preciso reportar de novo.

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

O **PokeL-ARecomp** (PLA Recomp) transforma o código do Nintendo Switch de *Pokémon Legends: Arceus* em um programa nativo de 64-bit para Windows e Linux.
O código de máquina ARM64 do jogo é traduzido antecipadamente (AOT) para C e compilado nativamente para arquitetura x86-64. O kernel do Switch, sistema de arquivos, áudio e gráficos NVN/Maxwell são reimplementados diretamente sobre Vulkan.

**Nenhum emulador roda enquanto você joga.** O jogo executa com máxima fluidez e eficiência direto na sua CPU e GPU.

> [!IMPORTANT]
> **Você precisa da sua própria cópia do jogo.** O PokeL-ARecomp não inclui nenhum código proprietário, ROMs, modelos, texturas ou chaves criptográficas da Nintendo. Você deve fornecer sua cópia legalmente adquirida (`.nsp` / `.xci` + atualização 1.1.1) e as chaves `prod.keys` extraídas do seu próprio console.

---

## Recursos e Destaques

### Desempenho e Motor
- **30 FPS (recomendado) ou 60 FPS (⚠️ experimental)**: em 30 FPS tudo roda na velocidade original do Switch. Em 60 FPS a câmera e o movimento mantêm a velocidade pelo ritmo dinâmico de delta-time, mas partes do jogo contam quadros em vez de tempo e ficam **duas vezes mais rápidas**: a comida jogada some antes de os Pokémon selvagens comerem ([#9](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/9)), e menus, diálogos e alguns efeitos ficam acelerados. São **limites conhecidos e antigos do Pokémon Legends: Arceus a 60 FPS** — nenhum patch de 60 FPS para este jogo (cheats de emulador, FPSLocker) resolveu — e não bugs novos deste port. Volte para 30 FPS para alimentar Pokémon. A taxa de quadros é instável no mundo aberto (em torno de 40 a 50 fps na máquina de teste), com pequenos engasgos na primeira compilação de cada shader.
- **Geração de Quadros (2x)**: mostra um quadro extra entre cada dois quadros do jogo — **60 na tela com o jogo a 30 FPS** (a lógica continua na velocidade do Switch) ou 120 a 60 FPS. O quadro extra é uma mistura dos dois vizinhos (ainda sem estimativa de movimento): mais suave em giros lentos de câmera, com rastro em movimentos rápidos. Funciona em qualquer placa de vídeo.
- **Desacoplamento de HUD (HUD Decoupling)**: Isola elementos 2D (minimapa, mira de captura, barras de vida, caixas de diálogo) via `hdr_snapshot` para eliminar artefatos e fantasmas visuais com a Geração de Quadros ativada.
- **Modos Explícitos de VSync**: Selecione entre *Desligado (Baixa Latência / Mailbox)*, *Ligado (VSync / 60 Hz - FIFO)* ou *Desligado (Sem Limite / Immediate)* com recriação dinâmica e imediata da swapchain Vulkan.
- **VSync: Desligado (Ilimitado)**: apresenta sem esperar a tela (menor latência, pode ter tearing), mas sempre limitado aos 30 ou 60 FPS escolhidos — acima disso, o que o jogo conta por quadro (menus, comida jogada) ficava ainda mais acelerado.
- **Escala de Resolução Interna**: De 25% até 4x 1080p nativo (4K UHD) com resolução dinâmica, e a opção **720p (Switch portátil)**, padrão do perfil *Leve*, para Steam Deck, placas integradas e placas de entrada.
- **Sobreposição CPU / GPU (automática)**: a CPU monta o próximo quadro enquanto a GPU desenha o atual. Ela é desligada automaticamente em CPUs com 8 threads ou menos e em placas AMD, onde já causou travamentos (para mudar: **F1 > Vídeo Avançado > Compatibilidade** ou `LARECOMP_GPU_OVERLAP=1` / `0`).
- **Escalonamento Multi-Core Agressivo (Intel Xeon e CPUs com Muitos Núcleos)**: Gerenciamento inteligente de threads paralelas de descompactação de texturas (até 8 threads simultâneas em CPUs de 16 a 24+ threads), eliminando engasgos (*stutters*) na exploração de Hisui.
- **Memória Desbloqueada para GPUs Dedicadas (Intel Arc e GPUs de Alta VRAM)**: Pool físico de 3.1 GB liberado para GPUs com VRAM dedicada (como a **Intel Arc B580 de 12 GB**), evitando o teto de 1 GB das iGPUs.

### Gráficos e Upscaling
- **Menu de Opções Reorganizado**: Abas dedicadas e limpas para **Vídeo** (resolução, modo de janela, fps alvo, vsync) e **Vídeo Avançado** (upscalers, geração de quadros, sombras, oclusão de ambiente, reflexos, luz indireta).
- **Distância de Visão Nativa em C (Draw Distance)**: Ajuste dinâmico de raio de entidades e árvores em código C sem modificação de arquivos ou pastas externas (*Padrão 1x*, *Alta 2.5x*, *Ultra 4x*). Pokémon selvagens e NPCs visíveis até 150m, e árvores até 800m.
- **Realce de Cores de Hisui (Tonemapper no Vulkan)**: Filtros pós-processamento de alta fidelidade (*Hisui Vívido* e *Hisui Cinematográfico*) eliminando a névoa acinzentada original sem nenhum custo de CPU.
- **Intel XeSS (Xe Super Sampling)**: Upscaling neural de alta fidelidade acelerado por inteligência artificial nos motores XMX de hardware da Intel Arc (do modo Ultra Desempenho até AA Nativo).
- **AMD FidelityFX FSR 1, FSR 2 (temporal)** e anti-aliasing **SMAA**.
- **AMD FSR 3.1 (temporal) — 🧪 Experimental**: **F1 > Vídeo Avançado > Anti-aliasing temporal > AMD FSR 3.1**. Mais detalhe em movimento e menos rastro que o FSR 2. Objetos rápidos ainda podem deixar um leve rastro (o movimento é estimado só pela câmera).
- **Nitidez Adaptativa por Contraste (RCAS)**: Ajuste em tempo real da nitidez visual.
- **Iluminação Avançada**: Oclusão de ambiente (XeGTAO), reflexos de água em espaço de tela (SSR), iluminação indireta (SSGI) e mapas de sombra de altíssima definição (até 4x).
- **Suporte a HDR**: Saída scRGB / HDR10 com calibração de brilho máximo.
- **Suporte a Pacotes de Texturas HD**: Carregamento direto da pasta `textures/hd/`.
- **Ultrawide 21:9 / 32:9 (Hor+) — 🧪 Experimental**: em **F1 > Vídeo > Proporção > Livre (ultrawide)** a imagem ocupa a janela em qualquer formato: telas mais largas mostram mais do mundo nas laterais, sem esticar. O formato da janela é detectado automaticamente. *Testado num monitor 32:9 real no mundo aberto, mas batalhas, cutscenes, interiores e menus ainda não foram 100% testados — podem ocorrer bugs* (objetos aparecendo/sumindo nas bordas, efeitos ou interface fora do lugar). Por favor, reporte na [issue #1](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/1). *16:9 original* mantém o enquadramento do jogo com barras laterais.

- **HUD que acompanha o mundo no ultrawide** ([#15](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/15)): a mira de captura, o "!" sobre o Pokémon que te notou, balões de fala e marcadores do mapa agora ficam sobre o Pokémon ou lugar que marcam em 21:9 / 32:9; o resto do HUD continua no layout 16:9.

- **Campo de visão (FOV)**: **F1 > Vídeo > Campo de visão** (45–90) amplia a câmera do próprio jogo. Cenário, partículas, fogo das tochas, olhos e alertas dos Pokémon e a vinheta de dano acompanham, e a tela de título mantém o enquadramento.

### Estabilidade
- **Listas de comandos protegidas contra sobrescrita**: o jogo reaproveita a memória de comandos muito antes de o port terminar de traduzi-la. Agora as listas ficam na memória do próprio port até serem traduzidas. Isso corrigiu os crashes na Floaro Main Street, em Jubilife ([#7](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/7), [#12](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/12)).
- **Serviço de rede offline**: funções que tentam ficar online (por exemplo *Conectar à internet* no menu) recebem a resposta de que o Switch está sem internet, em vez de derrubar o jogo.
- **Log de diagnóstico**: o `logs/lasrecomp.log` mostra a velocidade do jogo em relação ao tempo real (`[speed]`, a cada 10 s) e cada configuração alterada no jogo (`[settings] changed`), o que ajuda nos relatórios de bug.

### Cheats — 🧪 Experimental
- Arquivos de cheats do Atmosphere da versão 1.1.1 (`AEE8F150DDA1B5A8.txt`, ou a pasta `atmosphere` inteira) vão na pasta `cheats` ao lado do `PokeL-ARecomp.exe`. **F1 > Cheats** lista cada um com um botão de liga/desliga.
- **Cheats que mudam valores** (dinheiro, itens...) funcionam na hora. **Cheats de código** (taxa de captura...) mudam o código do jogo: depois de ligar ou desligar, reinicie e a tela inicial recompila o jogo com ele (como a primeira compilação).
- Um cheat pode estragar o save ou travar o jogo: mantenha um backup (`save/backups/`) e desligue os cheats antes de reportar um bug.

### Desempenho
- **Shaders preparados antes do jogo abrir**: depois de JOGAR, os shaders conhecidos do jogo são compilados antes (alguns segundos na primeira vez, guardados depois), com bem menos engasgos na primeira vez que cada efeito aparece.
- **Dica de desempenho:** overlays que se penduram no jogo (RivaTuner / MSI Afterburner, captura de jogo do OBS, overlay do Discord...) custam caro aqui: na máquina de teste, fechá-los reduziu a pior travada ao girar a câmera de 244 ms para 82 ms ([#10](https://github.com/fabioschmidt01/PokeL-ARecomp/issues/10)).

### Saves e Segurança
- **Backup Automático Rotacional de Saves**: Gera cópias datadas em `save/backups/main_YYYYMMDD_HHMMSS.bak` a cada salvamento confirmado pelo jogo, retendo as 10 mais recentes contra falhas ou quedas de energia.

### Controles e Interface
- Qualquer controle moderno via SDL3: Xbox, DualShock 4, DualSense, Switch Pro Controller, Steam Deck.
- **Ícones de botão que seguem o seu mapeamento**: os ícones de Xbox e PlayStation mostram o botão que você realmente aperta, seja com o controle mapeado por posição (layout do Switch) ou por letra (`bind_pad_a = south`: A do Xbox / ✕ confirma). Mudanças valem a partir do próximo início.
- Suporte total a Teclado e Mouse com controle suave de câmera pelo mouse e teclas totalmente remapeáveis.
- Ícones de botões dinâmicos que se adaptam ao controle em uso.

---

## Versão Suportada do Jogo

| Jogo | Title ID | Versão Alvo | Build ID do `main` |
|---|---|---|---|
| **Pokémon Legends: Arceus** | `01001F5010DFA000` | **1.1.1** (update v262144) | `AEE8F150DDA1B5A8` |

---

## Requisitos do Sistema

| Componente | Mínimo | Recomendado |
|---|---|---|
| **Sistema Operacional** | Windows 10/11 64-bit ou Linux x86-64 (glibc 2.35+) | Windows 11 ou Linux Moderno |
| **Processador** | x86-64 com suporte a AVX2 (4 Núcleos) | Intel Xeon E5 v3/v4 / Core i5/i7/i9 ou AMD Ryzen (8+ Núcleos) |
| **Placa de Vídeo** | GPU compatível com Vulkan 1.3 (4 GB VRAM) | **Intel Arc B580 (12 GB)** / RTX 3060+ / RX 6600+ |
| **Memória RAM** | 8 GB RAM | 16 GB+ RAM |
| **Armazenamento** | ~12 GB de espaço livre | SSD / NVMe |

---

## Como Jogar (Início Rápido)

1. Baixe o pacote mais recente na página de [Releases](https://github.com/fabioschmidt01/PokeL-ARecomp/releases/latest) (`PokeL-ARecomp-windows-x64.zip` para Windows, ou `.AppImage` para Linux).
2. Extraia o conteúdo para uma pasta com permissão de escrita (exemplo: `C:\Jogos\PokeL-ARecomp`).
3. Prepare seus arquivos do jogo:
   - Jogo base (`.nsp` ou `.xci`), a **atualização 1.1.1** (`.nsp`) e o arquivo `prod.keys` do seu console.
4. Execute `PokeL-ARecomp.exe` (ou `./plarecomp.AppImage` no Linux).
5. Na tela inicial, selecione a pasta do jogo e clique em **Configurar o jogo**.
6. A recompilação inicial do código leva de 10 a 25 minutos dependendo da sua CPU (isso ocorre **apenas uma vez**).
7. Clique em **Jogar** e aproveite sua aventura por Hisui!

---

## Como Reportar um Bug

O PokeL-ARecomp agora tem um **sistema básico de envio de relatório** (🆕 v0.27):

- **Se o jogo travar (crash):** a janela de crash tem o botão **Reportar no GitHub**. Um clique junta o seu `larecomp.ini`, os logs e os dados do seu PC (versão, sistema, CPU, RAM, GPU e driver) na pasta `logs/report-<data>/`, abre essa pasta e abre uma issue nova no GitHub já preenchida. **Arraste os arquivos da pasta para a issue** e conte o que você estava fazendo. Nada é enviado sozinho: você vê tudo antes de postar.
- **Qualquer outro bug** (gráficos, som, controles...): abra uma [issue nova](https://github.com/fabioschmidt01/PokeL-ARecomp/issues) e anexe o `larecomp.ini` e o `lasrecomp*.log` mais recente da pasta **`logs`**, ao lado do `PokeL-ARecomp.exe`. Uma captura de tela (**F12**) ajuda muito.
- Desligue os cheats e diga quais mods você usa: muitos bugs vêm deles.

## Controles Básicos

| Teclado | Ação | Controle |
|---|---|---|
| W / A / S / D | Movimentação do personagem | Analógico Esquerdo |
| Movimento do Mouse | Controle de câmera | Analógico Direito |
| Botão Esquerdo do Mouse | Lançar Pokébola / Ataque (ZR) | ZR |
| Botão Direito do Mouse | Mirar (ZL) | ZL |
| Espaço / Shift | Esquiva (Rolamento) / Correr (Y / B) | Y / B |
| F1 / Esc | Menu de Configurações | + / − |
| F10 | Captura de Tela (`screenshots/`) | - |
| F11 | Alternar Tela Cheia | - |

---

## Créditos e Agradecimentos

Este projeto foi construído graças ao trabalho pioneiro da comunidade de código aberto:

- **[Peiikai](https://github.com/peiikai)**: Criador do [LAS Recomp](https://github.com/peiikai/LASRecomp), arquiteto da tecnologia de recompilação estática de ARM64 para C, modelo de memória rápida e tradutor de shaders Maxwell para SPIR-V/Vulkan.
- **[Hexveil404](https://github.com/Hexveil404)**: Criador do [PokeZ-ARecomp](https://github.com/Hexveil404/PokeZ-ARecomp), pioneiro na adaptação de jogos Pokémon para recompilação estática em PC e desenvolvimento de serviços HLE de sistema operacional.
- **fabioschmidt01 & Antigravity**: Arquitetura, multi-threading para Intel Xeon, otimizações para Intel Arc B580, integração do Intel XeSS, desacoplamento de HUD, extrator standalone e os ports de Pokémon Legends: Arceus e Violet.
- **[didslima](https://github.com/didslima)**: Suporte a ultrawide (21:9 / 32:9) ([#6](https://github.com/fabioschmidt01/PokeL-ARecomp/pull/6)): a correção da câmera Hor+ / Vert+ direto no montador de projeção do jogo, o HUD mantido no layout 16:9, os testes automáticos da projeção e a validação num monitor 32:9 real.
- **[Thealexbarney](https://github.com/Thealexbarney)**: Criador da biblioteca [LibHac](https://github.com/Thealexbarney/LibHac), usada pela tela inicial para ler os dumps `.nsp` / `.xci` e aplicar o update (BKTR).
- **[Projeto Ryujinx](https://github.com/Ryujinx/Ryujinx)**: Referência de excelência comportamental das chamadas de sistema e comandos gráficos do Nintendo Switch.
- **Bibliotecas**: SDL3, FFmpeg, Dear ImGui, Vulkan Memory Allocator, Shaderc, XeGTAO, AMD FidelityFX, Intel XeSS.

---

## Aviso Legal

O *PokeL-ARecomp* é um projeto feito por fãs sem fins lucrativos. **Não** é afiliado, patrocinado ou associado à Nintendo, Game Freak, Creatures Inc. ou The Pokémon Company. *Pokémon* e *Pokémon Legends: Arceus* são marcas registradas de seus respectivos proprietários. Nenhum arquivo binário, textura, modelo ou código protegido é distribuído neste repositório.
