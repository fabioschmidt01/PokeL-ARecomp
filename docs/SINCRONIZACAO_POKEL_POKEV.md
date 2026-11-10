# Guia de Sincronização Cruzada: PokeL-ARecomp (Arceus) ⟷ PokeV-Recomp (Violet/Scarlet)

Documento de referência rápida para portabilidade e sincronização entre as duas vertentes do recompilador mantido por **@fabioschmidt01**.

---

## 1. O que vem do PokeV (Violet) para o PokeL (Arceus)

### Correção de TLS / Thread Pointer (`tpidr_el0`)
* **Motivo**: Previne o crash `c0000005 (Access Violation) accessing 0x30` no alocador `malloc` e threads do jogo.
* **Onde aplicar no PokeL**:
  1. [`runtime/kernel.c`](file:///D:/Rons/Pokemon%20Legends%20Arceus/PokeZ-ARecomp/runtime/kernel.c) em `thread_new`:
     ```c
     pthread_mutex_lock(&g_klock);
     t->id = next_thread_id++;
     t->tlr = tls_next;
     tls_next += 0x1000;  // 4KB por thread
     memset((void *)(uintptr_t)t->tlr, 0, 0x1000);
     t->cpu.tpidrro = t->tlr;
     t->cpu.tpidr = t->tlr + 0x200;  // Thread Pointer
     *(uint64_t *)(uintptr_t)(t->tlr + 0x1f0) = t->cpu.tpidr;  // Slot 0x1f0
     t->anext = g_threads;
     g_threads = t;
     pthread_mutex_unlock(&g_klock);
     ```
  2. [`runtime/rt.h`](file:///D:/Rons/Pokemon%20Legends%20Arceus/PokeZ-ARecomp/runtime/rt.h):
     ```c
     #define TLS_SIZE 0x0000400000ull  // 4 MB
     ```

---

## 2. O que vai do PokeL (Arceus) para o PokeV (Violet)

### A. Trava de Estado de Compilação (`recomp_state.ini`)
* **Motivo**: No Violet a compilação demora 27 minutos (1.730 objetos). A trava garante que após compilar uma vez, o jogo entre direto no menu "JOGAR" sem recompilar.
* **Onde aplicar no PokeV**:
  - `runtime/build_cache.cpp` (gravação em `build_cache_publish`)
  - `runtime/gamelib.c` (`state_matches` validando `recomp_state.ini`)

### B. Correção do `setjmp`/`longjmp` da VM Lua (256 slots + poda de pilha)
* **Motivo**: Previne crash `FATAL: no function at ...` em scripts de missões e mundo aberto.
* **Onde aplicar no PokeV**:
  - `runtime/dispatch.c` (`#define NJMP 256` + reciclagem de slots `jmps[i].sp < c->sp`)
  - `tools/gen.py` (inclusão de `__setjmp` no `_import_stubs`)

### C. Suporte a Shaders com Atômicos de 64 bits (`atom size 2`)
* **Motivo**: Previne falha de tradução em shaders de computação (`[shader] FS translate failed: atom size 2`).
* **Onde aplicar no PokeV**:
  - `runtime/nvn/maxwell/translate.c` (`size == 2` em `emit_atom`)

### D. Delta-Time Dinâmico (60 FPS Sem Velocidade 2x e Sem Slow-Motion)
* **Motivo**: O jogo roda a 60 FPS com física em velocidade normal 1.0x e ajusta o relógio se o FPS oscilar.
* **Onde aplicar no PokeV**:
  - `runtime/hle/framepace.c` (`rt_pla_frame_dt_ns` adaptado para o SV)
  - `runtime/nvn/objects.c` (`WIN->interval = 1` fixado em 60 FPS)

### E. Otimizações de CPU Xeon / Ryzen e GPU Intel Arc B580 / RTX
* **Motivo**: Streaming suave de mundo aberto sem engasgos de descompactação de texturas.
* **Onde aplicar no PokeV**:
  - `runtime/nvn/vulkan/textures.c` (`unpack_threads` escalado para até 10 threads)
  - `runtime/nvn/vulkan/device.c` (desbloqueio do pool de VRAM para GPUs discretas)
