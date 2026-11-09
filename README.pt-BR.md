<div align="center">

<img src="docs/icon/icon_1024.png" alt="Ícone PokeL-ARecomp" width="140">

# PokeL-ARecomp (PLA Recomp)

**Port nativo para PC de *Pokémon Legends: Arceus*, construído por recompilação estática.**

[English](README.md) · **Português (Brasil)**

[Relatar um problema](https://github.com/fabioschmidt01/PokeL-ARecomp-privado-teste/issues) ·
[Downloads (Releases)](https://github.com/fabioschmidt01/PokeL-ARecomp-privado-teste/releases)

</div>

---

O **PokeL-ARecomp** (PLA Recomp) transforma o código do Nintendo Switch de *Pokémon Legends: Arceus* em um programa nativo de 64-bit para Windows e Linux.
O código de máquina ARM64 do jogo é traduzido antecipadamente (AOT) para C e compilado nativamente para arquitetura x86-64. O kernel do Switch, sistema de arquivos, áudio e gráficos NVN/Maxwell são reimplementados diretamente sobre Vulkan.

**Nenhum emulador roda enquanto você joga.** O jogo executa com máxima fluidez e eficiência direto na sua CPU e GPU.

> [!IMPORTANT]
> **Você precisa da sua própria cópia do jogo.** O PokeL-ARecomp não inclui nenhum código proprietário, ROMs, modelos, texturas ou chaves criptográficas da Nintendo. Você deve fornecer sua cópia legalmente adquirida (`.nsp` / `.xci` + atualização 1.1.1) e as chaves `prod.keys` extraídas do seu próprio console.

---

## Recursos e Destaques

### Desempenho e Motor
- **60 FPS Nativos** na velocidade normal do jogo, ou 30 FPS idêntico ao console original.
- **Escala de Resolução Interna**: De 25% até 4x 1080p nativo (4K UHD) com resolução dinâmica.
- **Escalonamento Multi-Core Agressivo (Intel Xeon e CPUs com Muitos Núcleos)**: Gerenciamento inteligente de threads paralelas de descompactação de texturas (até 8 threads simultâneas em CPUs de 16 a 24+ threads), eliminando engasgos (*stutters*) na exploração de Hisui.
- **Memória Desbloqueada para GPUs Dedicadas (Intel Arc e GPUs de Alta VRAM)**: Pool físico de 3.1 GB liberado para GPUs com VRAM dedicada (como a **Intel Arc B580 de 12 GB**), evitando o teto de 1 GB das iGPUs.

### Gráficos e Upscaling
- **Intel XeSS (Xe Super Sampling)**: Upscaling neural de alta fidelidade acelerado por inteligência artificial nos motores XMX de hardware da Intel Arc.
- **AMD FidelityFX FSR 1, FSR 2 (temporal)** e anti-aliasing **SMAA**.
- **Iluminação Avançada**: Oclusão de ambiente (XeGTAO), reflexos de água em espaço de tela e mapas de sombra de alta resolução.
- **Suporte a HDR**: Saída scRGB / HDR10.
- **Suporte a Pacotes de Texturas HD**: Carregamento direto da pasta `textures/hd/`.

### Controles e Interface
- Qualquer controle moderno via SDL3: Xbox, DualShock 4, DualSense, Switch Pro Controller, Steam Deck.
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

1. Baixe o pacote mais recente na aba [Releases](https://github.com/fabioschmidt01/PokeL-ARecomp-privado-teste/releases) (`PokeL-ARecomp-windows-x64.zip` para Windows, ou `.AppImage` para Linux).
2. Extraia o conteúdo para uma pasta com permissão de escrita (exemplo: `C:\Jogos\PokeL-ARecomp`).
3. Prepare seus arquivos do jogo:
   - Jogo base (`.nsp` ou `.xci`), a **atualização 1.1.1** (`.nsp`) e o arquivo `prod.keys` do seu console.
4. Execute `lasrecomp.exe` (ou `./plarecomp.AppImage` no Linux).
5. Na tela inicial, selecione a pasta do jogo e clique em **Configurar o jogo**.
6. A recompilação inicial do código leva de 10 a 25 minutos dependendo da sua CPU (isso ocorre **apenas uma vez**).
7. Clique em **Jogar** e aproveite sua aventura por Hisui!

---

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

- **Peiikai**: Criador do [LAS Recomp](https://github.com/peiikai/LASRecomp), da arquitetura de recompilação estática e tradutor de shaders Maxwell.
- **Hexveil404**: Criador do [PokeZ-ARecomp](https://github.com/Hexveil404/PokeZ-ARecomp), pioneiro na emulação HLE do Switch para jogos Pokémon.
- **Projeto Ryujinx**: Referência comportamental das chamadas do kernel e comandos NVN do Switch.
- **Bibliotecas**: SDL3, FFmpeg, Dear ImGui, Vulkan Memory Allocator, Shaderc, XeGTAO, LibHac, Intel XeSS SDK.

---

## Aviso Legal

O *PokeL-ARecomp* é um projeto feito por fãs sem fins lucrativos. **Não** é afiliado, patrocinado ou associado à Nintendo, Game Freak, Creatures Inc. ou The Pokémon Company. *Pokémon* e *Pokémon Legends: Arceus* são marcas registradas de seus respectivos proprietários. Nenhum arquivo binário, textura, modelo ou código protegido é distribuído neste repositório.
