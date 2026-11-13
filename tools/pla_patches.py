"""Pokémon Legends: Arceus's builds (tools/gen.py), apart from Z-A's (tools/za_patches.py) and Link's Awakening's
(tools/patches.py)."""
import struct

PLA_BUILDS = {'aee8f150dda1b5a8': '1.1.1'}  # main's build ID (first 16 hex digits) -> version (update v262144)


def _pla_60fps(gen, a):
    # 0x7002aa2780: cbz w8, 0x7002aa27ac (checks vsync/throttle flag)
    # At 60fps, we bypass the 30fps throttle branch by jumping directly to 0x7002aa27ac
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0x34000168, hex(w)  # cbz w8, +0x2c
    target = a + 0x2c
    return ('{ extern _Bool rt_pla_is_60fps(void); '
            f'if (rt_pla_is_60fps() || (c->x[8] & 0xffffffff) == 0) {{ goto L_{target:x}; }} }}'
            f'  /* {a:#x}: 60fps unlocked pacing for PLA 1.1.1 */')


def _pla_frame_dt_init(gen, a):
    # 0x700277bde0: movk w9, #0x1fc, lsl #16 (completes w9 = 0x01fca056 = 33,333,334 ns = 1/30 s)
    # Initializes all 4 frame-clock slots ([x0+128], [x0+144], [x0+160], [x0+176]).
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0x72a03f89, hex(w)
    return ('{ extern uint64_t rt_pla_frame_dt_ns(uint32_t slot, uint64_t default_ns); '
            'c->x[9] = rt_pla_frame_dt_ns(0, 0x01fca056ull); }'
            f'  /* {a:#x}: PLA frame clock init delta-time (ns) */')


def _pla_frame_dt_step(gen, a):
    # 0x7002783050: ldr x2, [x8, w2, sxtw #3] (loads frame duration in ns from table at 0x700397e8d0)
    # Called every frame in f_70027812dc for slots w1 = 0..3 with w2 = 1 (0x01fca056 = 33,333,334 ns).
    # Returning true wall-clock / 60fps delta-time in ns keeps game speed at 1.0x at 60 FPS and below 60 FPS!
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xf862d902, hex(w)
    return ('{ extern uint64_t rt_pla_frame_dt_ns(uint32_t slot, uint64_t default_ns); '
            'uint64_t ea = c->x[8] + ((int64_t)(int32_t)c->x[2] << 3); '
            'c->x[2] = rt_pla_frame_dt_ns((uint32_t)c->x[1], LD64(ea)); }'
            f'  /* {a:#x}: PLA dynamic frame delta-time (ns) */')


def _pla_dynres_budget(gen, a):
    # 0x7000cc3658: movk x9, #0x4040, lsl #48 (completes double 33.333333333333336 ms = 0x4040aaaab042528b)
    # At 60 FPS, the frame budget for dynamic resolution is 16.666666666666668 ms (0x4030aaaab042528b).
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xf2e80809, hex(w)
    return ('{ extern _Bool rt_pla_is_60fps(void); '
            'c->x[9] = (c->x[9] & 0xffffffffffffull) | ((rt_pla_is_60fps() ? 0x4030ull : 0x4040ull) << 48); }'
            f'  /* {a:#x}: PLA dynamic resolution frame budget (16.67ms at 60fps, 33.33ms at 30fps) */')


def _pla_crash_guard_null_check(gen, a):
    # 0x7002a8d750: ldr x8, [x21, #0x30]
    # In Jubilife Village / telemetry callbacks, x21 can be NULL (0x0). Guard against c0000005 accessing 0x30.
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xf9401aa8, hex(w)  # ldr x8, [x21, #0x30]
    return ('c->x[8] = c->x[21] ? LD64(c->x[21] + 0x30) : 0;'
            f'  /* {a:#x}: null guard for ldr x8, [x21, #0x30] */')


def _pla_crash_guard_store1(gen, a):
    # 0x7002a8d734: stp x21, x22, [x20]
    # Guard against null x20 pointer
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xa9005a95, hex(w)  # stp x21, x22, [x20]
    return ('if (c->x[20]) { ST64(c->x[20], c->x[21]); ST64(c->x[20] + 8, c->x[22]); }'
            f'  /* {a:#x}: null guard for stp x21, x22, [x20] */')


def _pla_crash_guard_store2(gen, a):
    # 0x7002a8d754: stp x8, x19, [x20, #0x10]
    # Guard against null x20 pointer
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xa9014e88, hex(w)  # stp x8, x19, [x20, #0x10]
    return ('if (c->x[20]) { ST64(c->x[20] + 0x10, c->x[8]); ST64(c->x[20] + 0x18, c->x[19]); }'
            f'  /* {a:#x}: null guard for stp x8, x19, [x20, #0x10] */')


def _pla_proj_aspect(gen, a):
    # 0x7002994e2c: ldr s8, [x19, #0x240] in the camera projection builder f_7002994d60, perspective branch: x19 the
    # camera, [+0x23c] fovy, [+0x240] aspect (the game stores 16/9 there: six mov/movk 0x3fe38e39 sites), [+0x234] near,
    # [+0x238] far; then P00 = cot(fovy / 2) / aspect, P11 = cot(fovy / 2) -> the matrix at [+0x2b0]. The three before
    # leave s0 = fovy / 2 (the tanf argument). Through rt_pla_proj_aspect (runtime/hle/widescreen.c) a 16:9 camera
    # takes the window's shape (Hor+ wider, Vert+ taller): every perspective camera's matrix is built here, so one
    # site covers the field, battles and cutscenes alike; orthographic cameras (2D) take the other branch.
    want = {a - 12: 0xbd423e60,  # ldr s0, [x19, #0x23c]
            a - 8: 0x1e2c1001,   # fmov s1, #0.5
            a - 4: 0x1e210800,   # fmul s0, s0, s1
            a: 0xbd424268}       # ldr s8, [x19, #0x240]
    for at, word in want.items():
        w = struct.unpack_from('<I', gen.nso.image, at - gen.base)[0]
        assert w == word, (hex(at), hex(w))
    return ('{ extern float rt_pla_proj_aspect(float, float *); float a_, h_; '
            'memcpy(&a_, (const void *)(uintptr_t)(c->x[19] + 0x240), 4); memcpy(&h_, &c->v[0], 4); '
            'a_ = rt_pla_proj_aspect(a_, &h_); '
            'c->v[8] = 0; memcpy(&c->v[8], &a_, 4); c->v[0] = 0; memcpy(&c->v[0], &h_, 4); }'
            f'  /* {a:#x}: ldr s8, [x19, #0x240] (camera aspect; s0 = fovy / 2) */')


def pla_patches(build):
    if build.lower() == 'aee8f150dda1b5a8':
        return {
            0x7002aa2780: _pla_60fps,
            0x700277bde0: _pla_frame_dt_init,
            0x7002783050: _pla_frame_dt_step,
            0x7000cc3658: _pla_dynres_budget,
            0x7002a8d734: _pla_crash_guard_store1,
            0x7002a8d750: _pla_crash_guard_null_check,
            0x7002a8d754: _pla_crash_guard_store2,
            0x7002994e2c: _pla_proj_aspect,
        }
    return {}
