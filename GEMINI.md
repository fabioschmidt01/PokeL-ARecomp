# PokeL-ARecomp: Developer & Architecture Guide (Antigravity / Gemini)

Port nativo e recompilador estático para PC de **Pokémon Legends: Arceus (1.1.1)**, adaptado e otimizado a partir do projeto PokeZ-ARecomp / LAS Recomp por **@fabioschmidt01** em colaboração com o assistente de IA **Antigravity (Google DeepMind / Gemini)**.

O código ARM64 do Nintendo Switch é traduzido antecipadamente (AOT) para C puro e compilado para x86-64 nativo; o runtime recria o microkernel Horizon do Switch e o subsistema gráfico NVN diretamente sobre a API **Vulkan**.

---

## 1. Identificação do Jogo Suportado

| Atributo | Valor Oficial |
|---|---|
| **Título** | Pokémon Legends: Arceus (PLA) |
| **Title ID Base** | `01001F5010DFA000` |
| **Title ID Update** | `01001F5010DFA800` |
| **Versão Homologada** | **1.1.1** (Update v262144) |
| **Build ID (`main`)** | `AEE8F150DDA1B5A8` |
| **Módulos Convidados** | `main`, `sdk`, `subsdk0`, `subsdk1` |

---

## 2. Hardware-Alvo e Otimizações Específicas

O projeto conta com calibrações dedicadas para a estação de trabalho do desenvolvedor:

### A. Intel Xeon Multi-Threading (>= 24 threads lógicas)
- **Localização**: [`runtime/nvn/vulkan/textures.c`](file:///D:/Rons/Pokemon%20Legends%20Arceus/PokeZ-ARecomp/runtime/nvn/vulkan/textures.c)
- **Otimização**: Detecção dinâmica de alta contagem de threads na CPU. Em processadores com 24 ou mais threads lógicas (como a linha Intel Xeon E5), o descompactador paralelo de texturas (`LARECOMP_TEX_UNPACK_THREADS`) é elevado para até 10 threads dedicadas, eliminando engasgos (stutter) durante streaming de áreas abertas de Hisui.

### B. Intel Arc B580 (Arquitetura Xe2 Battlemage, 12 GB GDDR6)
- **Localização**: [`runtime/nvn/vulkan/device.c`](file:///D:/Rons/Pokemon%20Legends%20Arceus/PokeZ-ARecomp/runtime/nvn/vulkan/device.c)
- **Otimização**: Identificação de GPUs discretas Intel (`0x8086` + `VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU`). Desbloqueio do pool de memória guest de 3.1 GB original do Switch, permitindo alocação ampla nos 12 GB de VRAM de alta velocidade.

### C. Intel XeSS (Xe Super Sampling) SDK 3.0.2
- **Localização**: [`runtime/nvn/vulkan/vk_xess.h`](file:///D:/Rons/Pokemon%20Legends%20Arceus/PokeZ-ARecomp/runtime/nvn/vulkan/vk_xess.h), [`runtime/nvn/vulkan/xess.cpp`](file:///D:/Rons/Pokemon%20Legends%20Arceus/PokeZ-ARecomp/runtime/nvn/vulkan/xess.cpp), [`third_party/xess/`](file:///D:/Rons/Pokemon%20Legends%20Arceus/PokeZ-ARecomp/third_party/xess/)
- **Otimização**: Carregador dinâmico de `libxess.dll` via Vulkan. Quando uma GPU Intel Arc com aceleradores matriciais XMX é detectada, o upscale neural por IA é habilitado diretamente no pipeline gráfico.

---

## 3. Filosofia de Desenvolvimento e CI na Nuvem

Para preservar o sistema operacional local limpo e sem necessidade de dezenas de gigabytes de compiladores globais:

- **Desenvolvimento Local Leve**: O computador local necessita apenas de [.NET SDK](https://dotnet.microsoft.com/) e [Python](https://www.python.org/) para manipulação de dados e extração do jogo.
- **Compilação 100% no GitHub Actions**: Toda compilação pesada (Clang, UCRT64, LLVM, FFmpeg, SDL3, Zig Slim e Docker Ubuntu para AppImage) roda automaticamente na infraestrutura do GitHub.
- **Ambiente de Testes e Versionamento**:
  - Versionamento granular por tags (`v0.0.1`, `v0.0.2`, `v0.0.3`, `v0.0.4`...) até atingir a versão estável `0.1.0`.
  - Quando a versão 0.1 estiver testada e validada, o repositório público oficial será publicado como fork.

---

## 4. Estrutura de Diretórios e Onde Fica Cada Coisa

```
PokeL-ARecomp/
├── .github/workflows/
│   └── build-all.yml         # CI/CD: Compila Windows (.zip) e Linux (.AppImage)
├── docs/
│   ├── icon/                 # Roda Celestial de Arceus (SVG, ICO multi-res, PNG 1024)
│   └── start_screen.jpg      # Arte panorâmica 16:9 de Hisui para o Launcher
├── extracted/                # (Ignorado no git) Arquivos do jogo extraídos
│   ├── exefs/                # main, sdk, subsdk0, subsdk1, main.npdm
│   └── romfs.bin             # RomFS de 6.42 GB gerada com patch 1.1.1
├── runtime/
│   ├── gamebuild.c           # Orquestrador de geração e compilação do jogo
│   ├── loader.c              # Carregador de NSO com suporte a subsdk0/subsdk1
│   ├── nvn/vulkan/           # Backend Vulkan, Intel Arc e XeSS 3.0.2
│   └── ui/                   # Interface gráfica (Launcher & Menu F1 com tema Arceus)
├── third_party/
│   └── xess/                 # Headers e bibliotecas de importação do Intel XeSS SDK
└── tools/
    ├── zaimport/             # Importador C# / .NET para extrair NSPs e montar RomFS
    ├── gen.py                # Tradutor estático ARM64 -> C nativo
    ├── build_deps_windows.sh # Compilador do FFmpeg LGPL para Windows UCRT64
    └── za/dist.sh            # Empacotador do release final com Zig e dependências
```

---

## 5. Comandos Úteis

### Extrair a ROM do Jogo Localmente (via `zaimport`)
```powershell
dotnet run --project tools/zaimport -- `
  --title-keys "<caminho>/title.keys" `
  "<caminho>/prod.keys" `
  "extracted" `
  "<caminho>/Pokemon Legends Arceus[01001F5010DFA000][US][v0].nsp" `
  "<caminho>/Pokemon Legends Arceus [01001F5010DFA800][v262144].nsp"
```

### Acompanhar o Status da Compilação no GitHub Actions
```powershell
gh run list
gh run watch <run-id>
```

---

## 6. Atalhos e Teclas no Jogo

- **F1**: Abre o Menu de Configurações (Resolução, VSync, Filtros, Áudio, Controles).
- **F9**: Captura o dump de passes do próximo frame renderizado.
- **F10**: Salva captura de tela em alta qualidade na pasta `screenshots/`.
