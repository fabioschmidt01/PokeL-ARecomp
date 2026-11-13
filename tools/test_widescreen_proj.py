"""Pokémon Legends: Arceus's widescreen projection (issue #1): the math (tools/test_widescreen_proj.c, against
runtime/nvn/output_scale.c and the game's builder written out) and the generator patch (tools/pla_patches.py
_pla_proj_aspect): the C it emits, compiled against a stand-in CPU context, reads the camera's aspect and fovy / 2 and
writes s8 / s0 as the instructions it replaces would (upper bits cleared); a different instruction word is refused.
PLA_MAIN=<exefs/main of 1.1.1>: also checks the patch site's words in the real executable (needs lz4).
CC: the compiler (default clang; runtime/simd*.h need clang's builtins)."""
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import pla_patches  # noqa: E402

CC = shlex.split(os.environ.get('CC', 'clang'))
SITE = 0x7002994e2c
BASE = 0x7000000000
WORDS = {SITE - 12: 0xbd423e60, SITE - 8: 0x1e2c1001, SITE - 4: 0x1e210800, SITE: 0xbd424268}
failed = 0


class FakeNso:
    def __init__(self, words):
        self.image = bytearray(SITE - BASE + 0x10)
        for a, w in words.items():
            struct.pack_into('<I', self.image, a - BASE, w)


class FakeGen:
    base = BASE

    def __init__(self, words):
        self.nso = FakeNso(words)


def check(ok, what):
    global failed
    print(('ok   ' if ok else 'FAIL ') + what)
    failed += not ok


with tempfile.TemporaryDirectory() as d:
    d = Path(d)
    # 1. the math
    exe = d / 'widescreen-proj-test'
    subprocess.run(CC + ['-std=gnu11', '-O1', '-Iruntime', 'runtime/nvn/output_scale.c', 'tools/test_widescreen_proj.c',
                         '-lm', '-o', str(exe)], cwd=ROOT, check=True)
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    print(r.stdout, end='')
    check(r.returncode == 0, 'projection math (tools/test_widescreen_proj.c)')

    # 2. the generator patch: in the table for 1.1.1, emitted C behaves like the instruction
    table = pla_patches.pla_patches('aee8f150dda1b5a8')
    check(table.get(SITE) is pla_patches._pla_proj_aspect, f'{SITE:#x} patched for build aee8f150dda1b5a8')
    snippet = pla_patches._pla_proj_aspect(FakeGen(WORDS), SITE)
    src = d / 'patch_test.c'
    src.write_text('''#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t x[32]; __uint128_t v[32]; } Cpu;
static float seen_aspect, seen_half;
float rt_pla_proj_aspect(float a, float *h) { seen_aspect = a; seen_half = *h; *h = 0.75f; return a * 2.0f; }
static void site(Cpu *c) { ''' + snippet + ''' }
int main(void) {
    static float cam[0x300 / 4];
    cam[0x240 / 4] = 16.0f / 9.0f;
    Cpu c;
    memset(&c, 0xa5, sizeof c);  /* garbage everywhere: the patch must not depend on it */
    c.x[19] = (uintptr_t)cam;
    float h = 0.4363323f;  /* fovy 50 / 2, as fmul s0 left it (upper bits 0) */
    c.v[0] = 0; memcpy(&c.v[0], &h, 4);
    site(&c);
    float s8, s0;
    memcpy(&s8, &c.v[8], 4); memcpy(&s0, &c.v[0], 4);
    int ok = seen_aspect == 16.0f / 9.0f && seen_half == h && s8 == 32.0f / 9.0f && s0 == 0.75f &&
             (c.v[8] >> 32) == 0 && (c.v[0] >> 32) == 0 && c.x[19] == (uintptr_t)cam && cam[0x240 / 4] == 16.0f / 9.0f;
    printf("aspect in %f, fovy/2 in %f -> s8 %f, s0 %f\\n", seen_aspect, seen_half, s8, s0);
    return !ok;
}
''')
    pexe = d / 'patch-test'
    subprocess.run(CC + ['-std=gnu11', '-O1', str(src), '-o', str(pexe)], check=True)
    r = subprocess.run([str(pexe)], capture_output=True, text=True)
    print('   ', r.stdout.strip())
    check(r.returncode == 0, 'emitted C: reads [x19+0x240] and s0, writes s8 / s0 (upper bits 0), camera field kept')
    for at in WORDS:
        bad = dict(WORDS)
        bad[at] ^= 1
        try:
            pla_patches._pla_proj_aspect(FakeGen(bad), SITE)
            check(False, f'a different word at {at:#x} is refused')
        except AssertionError:
            check(True, f'a different word at {at:#x} is refused')

    # 3. optionally, the real executable
    main = os.environ.get('PLA_MAIN')
    if main:
        from nso import Nso
        n = Nso(main)
        check(n.build_id[:16] == 'aee8f150dda1b5a8', f'PLA_MAIN is 1.1.1 ({n.build_id[:16]})')

        class G:
            nso, base = n, BASE
        try:
            pla_patches._pla_proj_aspect(G, SITE)
            check(True, f'{SITE:#x} words match the real executable')
        except AssertionError as e:
            check(False, f'{SITE:#x} words in the real executable: {e}')

print('all widescreen projection checks passed' if not failed else f'{failed} widescreen projection check(s) FAILED')
sys.exit(1 if failed else 0)
