### Highlights da Versão v0.26.3

- **🎮 Ícones Automáticos de Controle (PlayStation, Xbox, Switch Pro e Teclado)**:
  - Detecção dinâmica de controle refinada com suporte nativo a PlayStation (DualSense, DualShock 4 e PS3 via `SDL_GAMEPAD_TYPE` e mapeamento de botões South Cross), Xbox (Series, One, 360) e Nintendo Switch Pro / Joy-Cons.
  - Opção *Ícones dos botões* em **F1 > Controles** atualizada com indicador de família em tempo real (`Auto [PlayStation]`, `Auto [Xbox]`, etc.) e persistência imediata em `larecomp.ini`.
  - Chip informativo exibido na tela inicial (JOGAR) mostrando o controle conectado e sua respectiva família de ícones.

- **🇧🇷 Gerenciador Visual de Mods & Ativação 1-Clique da Tradução PT-BR**:
  - Card de destaque para a Tradução PT-BR na aba **Mods** do Launcher: detecta a pasta ou arquivo da tradução e ativa com 1 clique, posicionando-a automaticamente no topo da lista (prioridade máxima da camada RomFS sobre textos e interfaces).
  - Botões de ação em massa na aba Mods: **Ativar todos** e **Desativar todos** os mods com 1 clique.
  - Destaque visual na lista de mods com badge `[TRADUÇÃO PT-BR]` e contorno temático verde/ciano.
  - Nova seção informativa em **F1 > Jogo > Mods e Traduções** exibindo em tempo real se a tradução brasileira está ativa no LayeredFS.
