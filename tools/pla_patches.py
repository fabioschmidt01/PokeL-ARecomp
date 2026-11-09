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


def pla_patches(build):
    if build.lower() == 'aee8f150dda1b5a8':
        return {
            0x7002aa2780: _pla_60fps,
            0x7002a8d734: _pla_crash_guard_store1,
            0x7002a8d750: _pla_crash_guard_null_check,
            0x7002a8d754: _pla_crash_guard_store2,
        }
    return {}
