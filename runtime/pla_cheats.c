// Cheats for Pokémon Legends: Arceus, in Atmosphere's format (atmosphere/contents/01001F5010DFA000/cheats/<build ID>.txt:
// "{Master code}" / "[Name]" headers, then dmnt cheat VM opcodes as hex words). The player drops the file (or the whole
// atmosphere folder) into the cheats folder next to the program and turns cheats on in F1 > Cheats.
//
// A recompiled game doesn't run its ARM64 code from memory, so the cheats split in two (cheat_classify):
//  - code cheats: static writes (opcode 0) into the main module's code (text segment) - new instructions, a code cave
//    and the branch to it (the "Mastercode"), or the flags those read. They are built into the game code: the start
//    screen recompiles the game with them (pla_cheats_patch_nso gives the generator a copy of exefs/main with the writes
//    applied; pla_cheats_id_suffix makes the library's id name the set, so a changed set asks for a rebuild). The same
//    writes go into the module's image in memory at boot (pla_cheats_boot), for code that reads data next to it.
//  - runtime cheats: everything else - values in the game's memory through pointer chains, button conditions - run by
//    the cheat VM below every frame (pla_cheats_frame), on and off at once.
// The enabled set: cheats/enabled.txt (one name per line). LARECOMP_CHEATS=0: none run or are built in.
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "hle.h"
#include "rt.h"
#include "platform.h"

#define MAX_CHEATS 256
#define MAX_OPS 1024
#define CHEATS_DIR "cheats"
#define ENABLED_FILE CHEATS_DIR "/enabled.txt"
#define GUEST_MAIN 0x7000000000ull

typedef struct {
    char name[96];
    bool master, code, on, bad;  // bad: an opcode the VM doesn't run (logged, the cheat is skipped)
    uint32_t ops[MAX_OPS];
    int nops;
} Cheat;

static Cheat *g_cheats;
static int g_n;
static bool g_loaded;
static uint32_t g_text_size;  // the main module's code size (its NSO's text segment)
// Where the code region ends: the read-only segment's start. Master codes put their code cave in the page padding after
// the text segment (1.1.1: text ends at 0x32A5690, the cave starts there, read-only data at 0x32A6000), so writes below
// this are code cheats, and the generator's copy gets a text segment long enough to hold them (pla_cheats_patch_nso).
static uint32_t g_code_end;

int pla_cheats_count(void) { return g_n; }
const char *pla_cheats_name(int i) { return i >= 0 && i < g_n ? g_cheats[i].name : ""; }
bool pla_cheats_is_code(int i) { return i >= 0 && i < g_n && g_cheats[i].code; }
bool pla_cheats_is_master(int i) { return i >= 0 && i < g_n && g_cheats[i].master; }
bool pla_cheats_is_bad(int i) { return i >= 0 && i < g_n && g_cheats[i].bad; }
bool pla_cheats_on(int i) { return i >= 0 && i < g_n && g_cheats[i].on; }

static bool cheats_off(void) { const char *e = getenv("LARECOMP_CHEATS"); return e && !atoi(e); }

// The main module's text size, from exefs/main's NSO header (+0x18: the text segment's decompressed size)
static void text_size_read(void) {
    FILE *f = fopen("extracted/exefs/main", "rb");
    uint8_t h[0x20];
    if (f && fread(h, 1, sizeof h, f) == sizeof h && !memcmp(h, "NSO0", 4)) {
        memcpy(&g_text_size, h + 0x18, 4);
        memcpy(&g_code_end, h + 0x14 + 0x10, 4);  // (the read-only segment's memory offset)
        if (g_code_end < g_text_size) g_code_end = g_text_size;
    }
    if (f) fclose(f);
}

// Words an opcode takes (0: unknown - the cheat is marked bad)
static int op_len(const uint32_t *o, int left) {
    uint32_t w = o[0], t = w >> 28;
    int width = (w >> 24) & 0xF;
    switch (t) {
    case 0x0: return width == 8 ? 4 : 3;                       // store static: 0TMR00AA AAAAAAAA V(V)
    case 0x1: return width == 8 ? 4 : 3;                       // begin conditional: 1TMC00AA AAAAAAAA V(V)
    case 0x2: return 1;                                        // end conditional / else
    case 0x3: return ((w >> 24) & 0xF) == 0 ? 2 : 1;           // loop start (count) / loop end
    case 0x4: return 3;                                        // load register static: 400R0000 V V
    case 0x5: return 2;                                        // load register from memory
    case 0x6: return 3;                                        // store static to register address
    case 0x7: return 2;                                        // legacy arithmetic
    case 0x8: return 1;                                        // key press conditional
    default: (void)left; return 0;
    }
}

static bool is_code_write(const uint32_t *o) {  // opcode 0 into the main module (M = 0) below the code's end
    uint32_t w = o[0];
    if (w >> 28 != 0 || ((w >> 20) & 0xF) != 0) return false;
    uint64_t addr = ((uint64_t)(w & 0xFF) << 32) | o[1];
    return g_code_end && addr < g_code_end;
}

static void cheat_classify(Cheat *c) {
    for (int i = 0; i < c->nops;) {
        int n = op_len(&c->ops[i], c->nops - i);
        if (!n || i + n > c->nops) { c->bad = true; return; }
        if (is_code_write(&c->ops[i])) c->code = true;
        i += n;
    }
}

static void parse_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    Cheat *cur = NULL;
    while (fgets(line, sizeof line, f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '[' || *s == '{') {
            char close = *s == '[' ? ']' : '}', *e = strchr(s + 1, close);
            if (!e || g_n >= MAX_CHEATS) { cur = NULL; continue; }
            cur = &g_cheats[g_n++];
            memset(cur, 0, sizeof *cur);
            snprintf(cur->name, sizeof cur->name, "%.*s", (int)(e - s - 1), s + 1);
            cur->master = *s == '{';
            continue;
        }
        for (char *p = s; cur && *p;) {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
            if (!*p) break;
            char *end;
            unsigned long v = strtoul(p, &end, 16);
            if (end == p) break;
            if (cur->nops < MAX_OPS) cur->ops[cur->nops++] = (uint32_t)v;
            p = end;
        }
    }
    fclose(f);
}

static void scan(const char *dir, int depth) {
    DIR *d = depth < 8 ? opendir(dir) : NULL;
    if (!d) return;
    for (struct dirent *e; (e = readdir(d));) {
        if (e->d_name[0] == '.') continue;
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        size_t l = strlen(e->d_name);
        struct stat st;
        if (stat(p, &st)) continue;
        if (S_ISDIR(st.st_mode)) scan(p, depth + 1);
        else if (l > 4 && !strcmp(e->d_name + l - 4, ".txt") && strcmp(e->d_name, "enabled.txt")) parse_file(p);
    }
    closedir(d);
}

static void enabled_load(void) {
    FILE *f = fopen(ENABLED_FILE, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        for (int i = 0; i < g_n; i++) if (!strcmp(g_cheats[i].name, line)) g_cheats[i].on = true;
    }
    fclose(f);
}

static void enabled_save(void) {
    mkdir(CHEATS_DIR, 0755);
    FILE *f = fopen(ENABLED_FILE, "w");
    if (!f) return;
    for (int i = 0; i < g_n; i++) if (g_cheats[i].on) fprintf(f, "%s\n", g_cheats[i].name);
    fclose(f);
}

void pla_cheats_load(void) {
    if (g_loaded) return;
    g_loaded = true;
    g_cheats = calloc(MAX_CHEATS, sizeof *g_cheats);
    if (!g_cheats) return;
    mkdir(CHEATS_DIR, 0755);
    text_size_read();
    scan(CHEATS_DIR, 0);
    for (int i = 0; i < g_n; i++) cheat_classify(&g_cheats[i]);
    enabled_load();
    int code = 0, bad = 0;
    for (int i = 0; i < g_n; i++) code += g_cheats[i].code, bad += g_cheats[i].bad;
    if (g_n) LOG("[cheats] %d cheat(s) in " CHEATS_DIR "/ (%d change the game code, %d not supported)\n", g_n, code, bad);
}

// A master code (the code cave the other code cheats branch to or set flags in) is on while any code cheat is: the
// runtime ones don't need it, and turning one of those on must not ask for a rebuild
void pla_cheats_set(int i, bool on) {
    if (i < 0 || i >= g_n) return;
    g_cheats[i].on = on;
    bool any = false;
    for (int k = 0; k < g_n; k++) if (!g_cheats[k].master && g_cheats[k].on && g_cheats[k].code) any = true;
    for (int k = 0; k < g_n; k++) if (g_cheats[k].master) g_cheats[k].on = any || (k == i && on);
    enabled_save();
}

// ---- code cheats: the build's id and the patched module
static uint64_t code_hash(void) {
    uint64_t h = 0;
    for (int i = 0; i < g_n; i++) {
        if (!g_cheats[i].on || !g_cheats[i].code || g_cheats[i].bad) continue;
        if (!h) h = 1469598103934665603ull;
        for (int k = 0; k < g_cheats[i].nops; k++) h = (h ^ g_cheats[i].ops[k]) * 0x100000001B3ull;
    }
    return h;
}

// What the library id gets appended for this set ("" without code cheats: the id players without cheats always had)
void pla_cheats_id_suffix(char *out, size_t n) {
    pla_cheats_load();
    uint64_t h = cheats_off() ? 0 : code_hash();
    if (h) snprintf(out, n, "-c%016llx", (unsigned long long)h);
    else if (n) out[0] = 0;
}
bool pla_cheats_code_active(void) { char s[32]; pla_cheats_id_suffix(s, sizeof s); return s[0] != 0; }

// The enabled code cheats' writes into a module image of `size` bytes (offsets from its start)
static int apply_code_writes(uint8_t *img, uint64_t size) {
    int n = 0;
    for (int i = 0; i < g_n; i++) {
        const Cheat *c = &g_cheats[i];
        if (!c->on || !c->code || c->bad) continue;
        for (int k = 0; k < c->nops;) {
            const uint32_t *o = &c->ops[k];
            int len = op_len(o, c->nops - k);
            if (is_code_write(o)) {
                int width = (o[0] >> 24) & 0xF;
                uint64_t addr = ((uint64_t)(o[0] & 0xFF) << 32) | o[1];
                uint64_t v = width == 8 ? ((uint64_t)o[2] << 32 | o[3]) : o[2];
                if ((width == 1 || width == 2 || width == 4 || width == 8) && addr + (uint64_t)width <= size)
                    memcpy(img + addr, &v, (size_t)width), n++;
            }
            k += len ? len : 1;
        }
    }
    return n;
}

size_t rt_lz4_decompress(const uint8_t *src, size_t srclen, uint8_t *dst, size_t dstlen);

// exefs/main with the code cheats applied, uncompressed (tools/nso.py reads a segment as is when its flag is clear):
// dst for the generator. False when nothing could be written (the build then uses the original).
bool pla_cheats_patch_nso(const char *src, const char *dst) {
    FILE *f = fopen(src, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *raw = n > 0x100 ? malloc((size_t)n) : NULL;
    bool ok = raw && fread(raw, 1, (size_t)n, f) == (size_t)n && !memcmp(raw, "NSO0", 4);
    fclose(f);
    if (!ok) { free(raw); return false; }
    uint32_t flags, foff[3], moff[3], size[3], csize[3];
    memcpy(&flags, raw + 0xC, 4);
    uint8_t *seg[3] = {0};
    for (int i = 0; i < 3 && ok; i++) {
        memcpy(&foff[i], raw + 0x10 + 0x10 * i, 4), memcpy(&moff[i], raw + 0x14 + 0x10 * i, 4);
        memcpy(&size[i], raw + 0x18 + 0x10 * i, 4), memcpy(&csize[i], raw + 0x60 + 4 * i, 4);
        seg[i] = malloc(size[i] ? size[i] : 1);
        ok = seg[i] && (uint64_t)foff[i] + csize[i] <= (uint64_t)n;
        if (ok && (flags & (1u << i))) ok = rt_lz4_decompress(raw + foff[i], csize[i], seg[i], size[i]) == size[i];
        else if (ok) ok = csize[i] >= size[i], memcpy(seg[i], raw + foff[i], ok ? size[i] : 0);
    }
    // (the text segment grown to the last code write - a code cave past its end - up to the read-only segment)
    uint32_t want = size[0];
    for (int i = 0; ok && i < g_n; i++) {
        const Cheat *c = &g_cheats[i];
        if (!c->on || !c->code || c->bad) continue;
        for (int k = 0; k < c->nops;) {
            const uint32_t *op = &c->ops[k];
            int len = op_len(op, c->nops - k);
            if (is_code_write(op)) {
                uint64_t end = ((((uint64_t)(op[0] & 0xFF) << 32) | op[1]) + ((op[0] >> 24) & 0xF) + 3) & ~3ull;
                if (end > want && end <= moff[1]) want = (uint32_t)end;
            }
            k += len ? len : 1;
        }
    }
    if (ok && want > size[0]) {
        uint8_t *grown = realloc(seg[0], want);
        if (grown) memset(grown + size[0], 0, want - size[0]), seg[0] = grown, size[0] = want;
    }
    int writes = ok ? apply_code_writes(seg[0], size[0]) : 0;
    FILE *o = ok ? fopen(dst, "wb") : NULL;
    if (o) {
        uint8_t hdr[0x100];
        memcpy(hdr, raw, sizeof hdr);
        uint32_t clear = 0, at = sizeof hdr;
        memcpy(hdr + 0xC, &clear, 4);  // (no compressed segments, no hash checks)
        for (int i = 0; i < 3; i++) {
            memcpy(hdr + 0x10 + 0x10 * i, &at, 4);
            memcpy(hdr + 0x60 + 4 * i, &size[i], 4);
            at += size[i];
        }
        fwrite(hdr, 1, sizeof hdr, o);
        for (int i = 0; i < 3; i++) fwrite(seg[i], 1, size[i], o);
        ok = fclose(o) == 0;
    } else ok = false;
    for (int i = 0; i < 3; i++) free(seg[i]);
    free(raw);
    if (ok) LOG("[cheats] %d code write(s) built into the game code (%s)\n", writes, dst);
    return ok;
}

// (at boot, the main module loaded) the same writes into its image in memory
void pla_cheats_boot(void) {
    pla_cheats_load();
    if (cheats_off() || !g_code_end || !pla_cheats_code_active()) return;
    uintptr_t page = (uintptr_t)GUEST_MAIN;
    size_t len = ((size_t)g_code_end + 0x3FFF) & ~(size_t)0x3FFF;
    plat_protect(page, len, PLAT_PROT_RW);  // (the guest's code pages hold data only: no host code runs there)
    int n = apply_code_writes((uint8_t *)(uintptr_t)GUEST_MAIN, g_code_end);
    // (left writable, as rando.c apply_data does: the loader's own protection of these pages isn't known here)
    LOG("[cheats] %d code write(s) in the game's image\n", n);
}

// ---- runtime cheats: Atmosphere's cheat VM (stratosphere dmnt), the opcodes the cheat files use
extern bool kernel_guest_mapped(uint64_t addr, uint64_t size);  // runtime/kernel.c
static bool guest_ok(uint64_t a, unsigned w) { return a >= GUEST_MAIN && a + w <= 0x8000000000ull && kernel_guest_mapped(a, w); }
static uint64_t rd(uint64_t a, unsigned w) {
    uint64_t v = 0;
    if (guest_ok(a, w)) memcpy(&v, (const void *)(uintptr_t)a, w);
    return v;
}
static void wr(uint64_t a, unsigned w, uint64_t v) {
    if (!guest_ok(a, w)) return;
    if (a < GUEST_MAIN + g_code_end) return;  // (the code: built in, not written while running)
    memcpy((void *)(uintptr_t)a, &v, w);
}
static uint64_t region_base(unsigned m) {
    switch (m) {
    case 0: return GUEST_MAIN;                                        // main module
    case 1: return HEAP_BASE;                                         // heap (runtime/rt.h)
    default: return 0;                                                // alias / ASLR: not used by these cheats
    }
}

extern uint64_t input_buttons_now(void);  // runtime/hle/input.c: the game's buttons (Switch npad bits)

static void run_cheat(const Cheat *c) {
    uint64_t reg[16] = {0};
    int skip = 0;  // nested conditionals that failed (their bodies are skipped)
    for (int k = 0; k < c->nops;) {
        const uint32_t *o = &c->ops[k];
        int len = op_len(o, c->nops - k);
        if (!len) return;
        uint32_t w = o[0], t = w >> 28;
        unsigned width = (w >> 24) & 0xF;
        if (skip) {  // only nesting matters while skipping
            if (t == 0x1 || t == 0x8) skip++;
            else if (t == 0x2 && ((w >> 24) & 0xF) == 0) skip--;
            k += len;
            continue;
        }
        switch (t) {
        case 0x0: {  // store static: [M base + R + address] = value
            if (is_code_write(o)) break;
            unsigned m = (w >> 20) & 0xF, r = (w >> 16) & 0xF;
            uint64_t a = region_base(m) + reg[r] + (((uint64_t)(w & 0xFF) << 32) | o[1]);
            wr(a, width, width == 8 ? ((uint64_t)o[2] << 32 | o[3]) : o[2]);
            break;
        }
        case 0x1: {  // begin conditional: [M base + address] <cond> value
            unsigned m = (w >> 20) & 0xF, cond = (w >> 16) & 0xF;
            uint64_t a = region_base(m) + (((uint64_t)(w & 0xFF) << 32) | o[1]);
            uint64_t v = width == 8 ? ((uint64_t)o[2] << 32 | o[3]) : o[2], x = rd(a, width);
            bool pass = cond == 1 ? x > v : cond == 2 ? x >= v : cond == 3 ? x < v : cond == 4 ? x <= v
                      : cond == 5 ? x == v : cond == 6 ? x != v : false;
            if (!pass) skip = 1;
            break;
        }
        case 0x2: break;  // end conditional
        case 0x4: reg[(w >> 16) & 0xF] = ((uint64_t)o[1] << 32) | o[2]; break;  // load register static
        case 0x5: {  // load register from memory: R = [M base + address] or [R + address]
            unsigned m = (w >> 20) & 0xF, r = (w >> 16) & 0xF, s = (w >> 12) & 0xF;
            uint64_t off = ((uint64_t)(w & 0xFF) << 32) | o[1];
            uint64_t a = s == 1 ? reg[r] + off : region_base(m) + off;
            reg[r] = rd(a, width);
            break;
        }
        case 0x6: {  // store static to register address: [R (+ offset register)] = value (R += width)
            unsigned r = (w >> 16) & 0xF, inc = (w >> 12) & 0xF, use_off = (w >> 8) & 0xF, ro = (w >> 4) & 0xF;
            uint64_t a = reg[r] + (use_off ? reg[ro] : 0);
            wr(a, width, ((uint64_t)o[1] << 32) | o[2]);
            if (inc) reg[r] += width;
            break;
        }
        case 0x7: {  // legacy arithmetic: R op= value
            unsigned r = (w >> 16) & 0xF, op = (w >> 12) & 0xF;
            uint64_t v = o[1];
            reg[r] = op == 0 ? reg[r] + v : op == 1 ? reg[r] - v : op == 2 ? reg[r] * v : op == 3 ? reg[r] << v
                   : op == 4 ? reg[r] >> v : reg[r];
            break;
        }
        case 0x8:  // key press conditional: every key of the mask held
            if ((input_buttons_now() & (w & 0x0FFFFFFF)) != (w & 0x0FFFFFFF)) skip = 1;
            break;
        default: break;
        }
        k += len;
    }
}

// (every presented frame, in play) the enabled runtime cheats, and the runtime parts of code cheats (their writes
// outside the code)
void pla_cheats_frame(void) {
    if (!g_loaded || cheats_off()) return;
    for (int i = 0; i < g_n; i++)
        if (g_cheats[i].on && !g_cheats[i].bad) run_cheat(&g_cheats[i]);
}
