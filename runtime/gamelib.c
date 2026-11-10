// Loads the recompiled game code library (runtime/gamelib.h).
#include "gamelib.h"

#include <SDL3/SDL_cpuinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "gamebuild.h"
#include "paths.h"
#include "rt.h"

static const GameLibTables *g_tables;
static char g_error[256];

bool gamelib_old_cpu(void) {
#if defined(__x86_64__)
    const char *e = getenv("LARECOMP_OLD_CPU");  // (testing it on a newer CPU)
    if (e && *e == '1') return true;
    __builtin_cpu_init();
    return !(__builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma") && __builtin_cpu_supports("bmi2"));
#else
    return false;
#endif
}

// The library's id file (<library>.id, written next to it by the build): its id (gamelib_full_id), read without
// loading it, plus extracted/recomp_state.ini to lock the compiled state to the Recomp version + PC hardware.
static void write_recomp_state(const char *game_dir, const char *want) {
    if (!game_dir || !*game_dir || !want || !*want) return;
    char spath[1100];
    snprintf(spath, sizeof spath, "%s/recomp_state.ini", game_dir);
    FILE *f = fopen(spath, "wb");
    if (!f) return;
    int threads = SDL_GetNumLogicalCPUCores();
    int ram_gb = (SDL_GetSystemRAM() + 512) / 1024;
    fprintf(f, "[recomp_state]\nlocked = 1\ngamelib_id = %s\ncpu_threads = %d\nram_gb = %d\ncompiled_at = %lld\n",
            want, threads, ram_gb, (long long)time(NULL));
    fclose(f);
}

static bool state_matches(const char *game_dir, const char *want) {
    if (!game_dir || !*game_dir) return true;
    const char *portable = getenv("LARECOMP_PORTABLE_STATE");
    if (portable && atoi(portable) != 0) return true;
    char spath[1100];
    snprintf(spath, sizeof spath, "%s/recomp_state.ini", game_dir);
    FILE *f = fopen(spath, "rb");
    if (!f) {
        // First launch with an already matching .id on this PC: lock and persist recomp_state.ini
        write_recomp_state(game_dir, want);
        return true;
    }
    char line[512], saved_id[128] = {0};
    int saved_threads = 0, locked = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char k[64] = {0}, v[256] = {0};
        if (sscanf(line, " %63[^= ] = %255[^\n]", k, v) == 2) {
            if (!strcmp(k, "locked")) locked = atoi(v);
            else if (!strcmp(k, "gamelib_id")) snprintf(saved_id, sizeof saved_id, "%s", v);
            else if (!strcmp(k, "cpu_threads")) saved_threads = atoi(v);
        }
    }
    fclose(f);
    int cur_threads = SDL_GetNumLogicalCPUCores();
    if (!locked || strcmp(saved_id, want) != 0) return false;
    if (saved_threads > 0 && cur_threads > 0 && saved_threads != cur_threads) return false;
    return true;
}

static bool id_matches(const char *lib, const char *want) {
    char path[1100], id[128] = {0};
    snprintf(path, sizeof path, "%s.id", lib);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(id, 1, sizeof id - 1, f);
    fclose(f);
    while (n && (id[n - 1] == '\n' || id[n - 1] == '\r')) id[--n] = 0;
    return !strcmp(id, want);
}

static bool exists(const char *p) {
    FILE *f = fopen(p, "rb");
    if (f) fclose(f);
    return f != NULL;
}

// The places to look, in order (gamelib.h). Returns how many were filled.
static int candidates(const char *game_dir, char out[3][1100]) {
    int n = 0;
    const char *env = getenv("LARECOMP_GAME_LIB");
    if (env && *env) snprintf(out[n++], 1100, "%s", env);
    char exe_side[1024];
    if (rt_resource(GAMELIB_NAME, NULL, exe_side, sizeof exe_side) && exists(exe_side))
        snprintf(out[n++], 1100, "%s", exe_side);
    if (game_dir) snprintf(out[n++], 1100, "%s/%s", game_dir, GAMELIB_NAME);
    return n;
}

bool gamelib_present(const char *game_dir) {
    if (g_tables) return true;
    char want[GAMELIB_FULL_ID_SIZE];
    if (!game_dir || !gamelib_full_id(game_dir, want, sizeof want)) return false;
    char c[3][1100];
    int n = candidates(game_dir, c);
    for (int i = 0; i < n; i++)
        if (exists(c[i]) && id_matches(c[i], want) && state_matches(game_dir, want)) return true;
    return false;
}

static const GameLibTables *open_lib(const char *path, const char *want) {
#ifdef _WIN32
    // Ensure the application directory is in the DLL search path
    char exe_dir[MAX_PATH] = "";
    if (GetModuleFileNameA(NULL, exe_dir, sizeof exe_dir)) {
        char *last_slash = strrchr(exe_dir, '\\');
        if (!last_slash) last_slash = strrchr(exe_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            SetDllDirectoryA(exe_dir);
            // Ensure lasrecomp.exe exists as a companion alias so lasgame.dll's import binds
            char lasrecomp_path[MAX_PATH];
            snprintf(lasrecomp_path, sizeof lasrecomp_path, "%s\\lasrecomp.exe", exe_dir);
            char self_path[MAX_PATH];
            GetModuleFileNameA(NULL, self_path, sizeof self_path);
            if (GetFileAttributesA(lasrecomp_path) == INVALID_FILE_ATTRIBUTES) {
                if (!CreateHardLinkA(lasrecomp_path, self_path, NULL)) {
                    CopyFileA(self_path, lasrecomp_path, FALSE);
                }
            }
        }
    }
    // If running under a renamed binary (e.g. PokeL-ARecomp.exe), load lasrecomp.exe so its exported symbols are in the PEB module list
    if (!GetModuleHandleA("lasrecomp.exe")) {
        LoadLibraryA("lasrecomp.exe");
    }
    HMODULE h = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h) h = LoadLibraryA(path);
    if (!h) { snprintf(g_error, sizeof g_error, "can't load %s (error %lu)", path, GetLastError()); return NULL; }
    GameLibEntry entry = (GameLibEntry)(void *)GetProcAddress(h, GAMELIB_ENTRY);
#else
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) { snprintf(g_error, sizeof g_error, "%s", dlerror()); return NULL; }
    GameLibEntry entry = (GameLibEntry)dlsym(h, GAMELIB_ENTRY);
#endif
    if (!entry) { snprintf(g_error, sizeof g_error, "%s: not game code", path); return NULL; }
    const GameLibTables *t = entry();
    if (!t || t->abi != GAMELIB_ABI || !t->id || strcmp(t->id, want)) {
        snprintf(g_error, sizeof g_error, "%s was built for another version", path);
        return NULL;  // (left loaded: nothing of it runs)
    }
    return t;
}

bool gamelib_load(const char *game_dir, char *why, size_t why_size) {
    static bool tried;
    if (!tried) {
        tried = true;
        snprintf(g_error, sizeof g_error, "the game code hasn't been built: set up the game files on the start screen");
        char want[GAMELIB_FULL_ID_SIZE] = "";
        if (!game_dir || !gamelib_full_id(game_dir, want, sizeof want)) {
            snprintf(g_error, sizeof g_error, "the game files' exefs/main is missing or unreadable");
            if (why) snprintf(why, why_size, "%s", g_error);
            return false;
        }
        char c[3][1100];
        int n = candidates(game_dir, c);
        const char *env = getenv("LARECOMP_GAME_LIB");
        for (int i = 0; i < n && !g_tables; i++) {
            if (!exists(c[i])) continue;
            bool given = i == 0 && env && *env;  // LARECOMP_GAME_LIB: loaded as given, the library's own id decides
            if (!given && (!id_matches(c[i], want) || !state_matches(game_dir, want))) {
                snprintf(g_error, sizeof g_error, "%s was built for another version or hardware", c[i]);
                continue;
            }
            g_tables = open_lib(c[i], want);
            if (g_tables) {
                char n[160] = "";
                for (uint32_t k = 0, o = 0; k < g_tables->nmods && o < sizeof n; k++)
                    o += snprintf(n + o, sizeof n - o, "%s%s %llu", k ? ", " : "", g_tables->mods[k].name,
                                  (unsigned long long)*g_tables->mods[k].count);
                LOG("[gamelib] %s (functions: %s)\n", c[i], n);
            }
        }
    }
    if (!g_tables && why) snprintf(why, why_size, "%s", g_error);
    return g_tables != NULL;
}

const GameLibTables *gamelib_tables(void) { return g_tables; }
