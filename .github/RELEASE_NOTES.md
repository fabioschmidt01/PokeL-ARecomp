### Highlights da Versão v0.26.4

- **📷 Slider de Campo de Visão 3D (FOV - Field of View)**:
  - Novo controle interativo em **F1 > Vídeo Avançado** e **F1 > Jogo**: permite afastar ou aproximar a perspectiva da câmera 3D de 45° a 90° (padrão: 60°).
  - Interceptação nativa ultra-rápida no pipeline Vulkan (`runtime/nvn/vulkan/exec.c`): recalcula os eixos X e Y da matriz de projeção em memória transitória com escala tangencial exata sem tocar no buffer de profundidade Z.
  - Zero re-compilação de shaders e zero perda de performance: troca o FOV instantaneamente no próximo quadro sem travamentos ou micro-stutters.
  - Sincronização automática com AMD FSR 2 (jitter subpixel e vetores de movimento) e GTAO (raios e profundidade de oclusão ambiente).
  - Preservação total da interface 2D (textos, menus, minimapa, botões e barras de vida continuam em escala perfeitamente nítida).
  - O zoom dinâmico de mira de Poké Bolas (botão ZR) é mantido proporcionalmente com suavidade.
  - Configuração salva automaticamente no arquivo `larecomp.ini` sob a chave `fov`.
