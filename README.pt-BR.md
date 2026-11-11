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
- **60 FPS Nativos** na velocidade normal do jogo com ritmo dinâmico de delta-time, ou 30 FPS idêntico ao console original.
- **Geração de Quadros (Frame Generation - 90 a 120 FPS)**: Interpolação acelerada por hardware que suaviza a movimentação para 90–120 FPS sem acelerar a lógica do jogo.
- **Desacoplamento de HUD (HUD Decoupling)**: Isola elementos 2D (minimapa, mira de captura, barras de vida, caixas de diálogo) via `hdr_snapshot` para eliminar artefatos e fantasmas visuais com a Geração de Quadros ativada.
- **Modos Explícitos de VSync**: Selecione entre *Desligado (Baixa Latência / Mailbox)*, *Ligado (VSync / 60 Hz - FIFO)* ou *Desligado (Sem Limite / Immediate)* com recriação dinâmica e imediata da swapchain Vulkan.
- **Escala de Resolução Interna**: De 25% até 4x 1080p nativo (4K UHD) com resolução dinâmica.
- **Escalonamento Multi-Core Agressivo (Intel Xeon e CPUs com Muitos Núcleos)**: Gerenciamento inteligente de threads paralelas de descompactação de texturas (até 8 threads simultâneas em CPUs de 16 a 24+ threads), eliminando engasgos (*stutters*) na exploração de Hisui.
- **Memória Desbloqueada para GPUs Dedicadas (Intel Arc e GPUs de Alta VRAM)**: Pool físico de 3.1 GB liberado para GPUs com VRAM dedicada (como a **Intel Arc B580 de 12 GB**), evitando o teto de 1 GB das iGPUs.

### Gráficos e Upscaling
- **Menu de Opções Reorganizado**: Abas dedicadas e limpas para **Vídeo** (resolução, modo de janela, fps alvo, vsync) e **Vídeo Avançado** (upscalers, geração de quadros, sombras, oclusão de ambiente, reflexos, luz indireta).
- **Intel XeSS (Xe Super Sampling)**: Upscaling neural de alta fidelidade acelerado por inteligência artificial nos motores XMX de hardware da Intel Arc (do modo Ultra Desempenho até AA Nativo).
- **AMD FidelityFX FSR 1, FSR 2 (temporal)** e anti-aliasing **SMAA**.
- **Nitidez Adaptativa por Contraste (RCAS)**: Ajuste em tempo real da nitidez visual.
- **Iluminação Avançada**: Oclusão de ambiente (XeGTAO), reflexos de água em espaço de tela (SSR), iluminação indireta (SSGI) e mapas de sombra de altíssima definição (até 4x).
- **Suporte a HDR**: Saída scRGB / HDR10 com calibração de brilho máximo.
- **Suporte a Pacotes de Texturas HD**: Carregamento direto da pasta `textures/hd/`.

### Ferramentas e Utilitários
- **PokeL-Extractor (Extrator Standalone de ROMs)**: Ferramenta executável única (GUI e CLI) baseada em LibHac. Escaneia pastas e subpastas, detecta automaticamente jogo base, atualizações (v262144) e DLCs (mesmo com arquivos renomeados), exibe os ícones oficiais do Switch e mescla partições RomFS e atualizações BKTR com velocidade em MB/s e tempo estimado (ETA).

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
4. Execute `PokeL-ARecomp.exe` (ou `./plarecomp.AppImage` no Linux).
5. Na tela inicial, selecione a pasta do jogo e clique em **Configurar o jogo**.
6. A recompilação inicial do código leva de 10 a 25 minutos dependendo da sua CPU (isso ocorre **apenas uma vez**).
7. Clique em **Jogar** e aproveite sua aventura por Hisui!

---

## Extrator Standalone de ROMs (PokeL-Extractor)

Junto com as releases é distribuído o **PokeL-Extractor** (`PokeL-Extractor.exe`), utilitário independente para inspeção e extração de ROMs do Nintendo Switch criado com LibHac e .NET 9:

- **Varredura Recursiva de Pastas**: Localiza automaticamente arquivos `.nsp`, `.nsz`, `.xci` e `.xcz` em qualquer profundidade de diretórios.
- **Identificação Automática de Title ID e CNMT**: Diferencia Jogo Base, Atualizações (Update v262144) e DLCs, mesmo que os arquivos estejam renomeados.
- **Visualização de Ícones Oficiais do Switch**: Extrai as imagens embutidas no Control NCA de cada título e as exibe na interface gráfica.
- **Extração e Junção de RomFS e ExeFS**: Aplica patches BKTR das atualizações sobre a RomFS base de forma direta e limpa, pronta para o PokeL-ARecomp.
- **Telemetria em Tempo Real**: Medidor de progresso detalhado com MB/s transferidos, tempo decorrido, previsão de término (ETA) e log completo.
- **Modos Gráfico e Linha de Comando**: Use com janela visual interativa ou integre em scripts de terminal (`PokeL-Extractor.exe --scan <pasta> --export <saida>`).

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

## Histórico de Versões e Atualizações

### v0.0.25 (Mais Recente)
- **Divisão do Menu de Vídeo**: Separação completa entre a aba **Vídeo** (exibição, resolução, taxa de quadros e VSync) e a nova aba **Vídeo Avançado** (XeSS, FSR, Geração de Quadros, sombras, oclusão ambiental, reflexos e iluminação indireta).
- **Seletor Explícito de VSync**: Três opções diretas com recriação instantânea da swapchain Vulkan:
  - *Desligado (Baixa Latência / Mailbox)*: Taxas altas sem tearing de vblank, recomendado para Frame Gen.
  - *Ligado (VSync / 60 Hz - FIFO)*: Sincronização tradicional com a taxa de atualização do monitor.
  - *Desligado (Sem Limite / Immediate)*: Apresentação imediata desimpedida para teste de latência máxima.
- **Correção da Trava em 30 FPS na Geração de Quadros**: Evita o bloqueio duplo de vblank sob modo FIFO quando a interpolação está ativa, mantendo a cadência do loop de jogo a 60 ticks/s para alcançar 90 a 120 FPS reais.
- **Inclusão do PokeL-Extractor**: Disponibilizado nos pacotes de release e artefatos de integração contínua do GitHub.

### v0.0.24
- **Limpeza de Identidade de Release**: Padronização da pasta e executáveis para **PokeL-ARecomp** (`PokeL-ARecomp.exe`).
- **Ícone Oficial da Roda de Arceus**: Inserção de ícone com múltiplas resoluções (16x16 até 256x256) diretamente no executável PE do Windows.
- **Limpeza de Mensagens de Log Legadas**: Remoção de textos de jogos antigos e alinhamento de telemetria.
- **Otimizações para Intel Arc B580 (Xe2)**: Liberação de 3.1 GB de VRAM física dedicada sem cair na restrição de 1 GB de iGPUs.
- **Multi-threading para Intel Xeon**: Escalonamento dinâmico em até 8 threads simultâneas para streaming de texturas sem engasgos.

### v0.0.23
- **Correção de Crash de Inicialização (`0xc0000005`)**: Migração de `dcache` e `jmps` para a estrutura de CPU de cada thread, evitando violação de acesso à memória.
- **Estabilidade do Ciclo de Vida**: Inicialização robusta de áudio, sistema de arquivos e camadas NVN/Vulkan.

### v0.0.22
- **Motor Dinâmico a 60 FPS**: Implementação de delta-time dinâmico para *Pokémon Legends: Arceus*, garantindo jogabilidade em 60 FPS fluidos sem dobrar a velocidade da física.

### v0.0.21
- **Desacoplamento de HUD no Vulkan**: Isolamento da cena 3D e interface via `hdr_snapshot`, eliminando fantasmas no minimapa e na mira com Frame Generation.

### v0.0.20
- **Integração do Intel XeSS SDK**: Upscaling neural acelerado por hardware nos motores matriciais XMX da Intel Arc.

---

## Créditos e Agradecimentos

Este projeto foi construído graças ao trabalho pioneiro da comunidade de código aberto:

- **[Peiikai](https://github.com/peiikai)**: Criador do [LAS Recomp](https://github.com/peiikai/LASRecomp), arquiteto da tecnologia de recompilação estática de ARM64 para C, modelo de memória rápida e tradutor de shaders Maxwell para SPIR-V/Vulkan.
- **[Hexveil404](https://github.com/Hexveil404)**: Criador do [PokeZ-ARecomp](https://github.com/Hexveil404/PokeZ-ARecomp), pioneiro na adaptação de jogos Pokémon para recompilação estática em PC e desenvolvimento de serviços HLE de sistema operacional.
- **[Thealexbarney](https://github.com/Thealexbarney)**: Criador da biblioteca [LibHac](https://github.com/Thealexbarney/LibHac), base do nosso utilitário independente `PokeL-Extractor`.
- **[Projeto Ryujinx](https://github.com/Ryujinx/Ryujinx)**: Referência de excelência comportamental das chamadas de sistema e comandos gráficos do Nintendo Switch.
- **Bibliotecas**: SDL3, FFmpeg, Dear ImGui, Vulkan Memory Allocator, Shaderc, XeGTAO, AMD FidelityFX, Intel XeSS.

---

## Aviso Legal

O *PokeL-ARecomp* é um projeto feito por fãs sem fins lucrativos. **Não** é afiliado, patrocinado ou associado à Nintendo, Game Freak, Creatures Inc. ou The Pokémon Company. *Pokémon* e *Pokémon Legends: Arceus* são marcas registradas de seus respectivos proprietários. Nenhum arquivo binário, textura, modelo ou código protegido é distribuído neste repositório.
