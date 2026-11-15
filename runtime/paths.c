// Data root and shipped resources (runtime/paths.h).
#include <SDL3/SDL.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <sys/utime.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/statvfs.h>
#include <utime.h>
#endif

#include "gamelib.h"
#include "game_identity.h"
#include "paths.h"
#include "platform.h"

static char g_root[1024], g_start[1024], g_exe_dir[1024];

#ifdef _WIN32
#define SEP "\\"
static bool is_link(const char *p) {  // symlink or junction
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT);
}
// The final path, junctions and symlinks followed (GetFinalPathNameByHandle); _fullpath (lexical, as before) only
// where the file system can't say
static bool resolve(const char *p, char *out, size_t n) {
    wchar_t w[1100], f[1100];
    if (MultiByteToWideChar(CP_UTF8, 0, p, -1, w, 1100) > 0) {
        HANDLE h = CreateFileW(w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS, NULL);  // (BACKUP_SEMANTICS: folders too)
        if (h != INVALID_HANDLE_VALUE) {
            DWORD k = GetFinalPathNameByHandleW(h, f, 1100, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            CloseHandle(h);
            if (k && k < 1100) {
                const wchar_t *s = f;
                if (!wcsncmp(f, L"\\\\?\\UNC\\", 8)) s = f + 6, f[6] = L'\\';  // \\?\UNC\srv\share -> \\srv\share
                else if (!wcsncmp(f, L"\\\\?\\", 4)) s = f + 4;                    // \\?\C:\... -> C:\...
                return WideCharToMultiByte(CP_UTF8, 0, s, -1, out, (int)n, NULL, NULL) > 0;
            }
        }
    }
    return _fullpath(out, p, n) != NULL;
}
#define path_eq(a, b) (_stricmp(a, b) == 0)
#define path_prefix(a, b, n) (_strnicmp(a, b, n) == 0)
#else
#define SEP "/"
static bool is_link(const char *p) {
    struct stat st;
    return lstat(p, &st) == 0 && S_ISLNK(st.st_mode);
}
static bool resolve(const char *p, char *out, size_t n) {
    char buf[PATH_MAX];
    if (!realpath(p, buf)) return false;
    snprintf(out, n, "%s", buf);
    return true;
}
#define path_eq(a, b) (strcmp(a, b) == 0)
#define path_prefix(a, b, n) (strncmp(a, b, n) == 0)
#endif

static bool is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static void mkdirs(const char *path) {
    char p[1024];
    snprintf(p, sizeof p, "%s", path);
    for (char *c = p + 1; *c; c++)
        if (*c == '/' || *c == '\\') { char s = *c; *c = 0; mkdir(p, 0755); *c = s; }
    mkdir(p, 0755);
}

static bool writable(const char *dir) {
    char t[1100];
    snprintf(t, sizeof t, "%s" SEP ".write_test", dir);
    FILE *f = fopen(t, "wb");
    if (!f) return false;
    fclose(f);
    remove(t);
    return true;
}

static void fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PLA Recomp", msg, NULL);
    exit(1);
}

static bool g_dev;  // the data root is the starting folder because it has extracted/ (a source checkout)
bool rt_dev_mode(void) { return g_dev; }

// ---- where the data folder is: the platform's folder, or another one a pointer file there names
#define POINTER_FILE "data_root.txt"            // in the platform's folder: the data folder's path (one line)
#define NOTE_TO ".lasrecomp_move_to"            // in a move's source until it switched: where the copy went
#define NOTE_FROM ".lasrecomp_move_from"        // in a move's destination: the old folder, still to be emptied
#define MARK_PARTIAL ".lasrecomp_incomplete"    // in a move's destination until its copy was checked
// What a data folder holds (and a move takes along). Logs stay where they are.
static const char *const DATA_ENTRIES[] = {"save", "rando", "larecomp.ini", "fonts", "gamecontrollerdb.txt", "cache",
                                           "extracted"};
#define N_ENTRIES (sizeof DATA_ENTRIES / sizeof *DATA_ENTRIES)
static char g_home[1024];  // the platform's folder (where POINTER_FILE lives)

static void exe_dir(char *out, size_t n) {
    out[0] = 0;
    if (!plat_exe_path(out, n)) return;
    char *s = strrchr(out, '/'), *b = strrchr(out, '\\');
    if (b > s) s = b;
    if (s) *s = 0;
}

// The platform's data folder. LARECOMP_DATA_HOME (tests) stands in for it.
static void default_home(char *root, size_t n) {
    const char *t = getenv("LARECOMP_DATA_HOME");
    if (t && *t) { snprintf(root, n, "%s", t); return; }
#ifdef _WIN32
    exe_dir(root, n);
#elif defined(__linux__)
    // (the AppImage is read-only): $XDG_DATA_HOME/plarecomp, ~/.local/share/plarecomp when XDG_DATA_HOME is unset or
    // relative (the XDG spec). PokeL-ARecomp's own (RT_LINUX_DATA_DIR): the related recomps built from this runtime
    // keep theirs in lasrecomp/, zarecomp/ and pokev-recomp/, which are never read, moved or written from here.
    const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    if (xdg && *xdg == '/') snprintf(root, n, "%s/" RT_LINUX_DATA_DIR, xdg);
    else snprintf(root, n, "%s/.local/share/" RT_LINUX_DATA_DIR, home && *home ? home : "/tmp");
#else
    const char *home = getenv("HOME");
    snprintf(root, n, "%s/Library/Application Support/" RT_BUNDLE_ID, home && *home ? home : "/tmp");
#endif
}

// A one-line file's line (no line end); false: no file, or empty
static bool read_line(const char *path, char *out, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t k = fread(out, 1, n - 1, f);
    fclose(f);
    out[k] = 0;
    out[strcspn(out, "\r\n")] = 0;
    return out[0] != 0;
}

// Written and flushed to the disk; replaces the file in one step (a cut-off write never leaves half a line)
static bool write_line(const char *path, const char *line) {
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fprintf(f, "%s\n", line) > 0 && fflush(f) == 0 && fsync(fileno(f)) == 0;
    ok &= fclose(f) == 0;
#ifdef _WIN32
    ok = ok && MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    ok = ok && rename(tmp, path) == 0;
#endif
    if (!ok) remove(tmp);
    return ok;
}

static void join(char *out, size_t n, const char *dir, const char *name) { snprintf(out, n, "%s" SEP "%s", dir, name); }

// a and b are the same folder, or one is inside the other (unresolvable: treated as overlapping)
static bool within(const char *child, const char *parent) {
    size_t n = strlen(parent);
    while (n > 1 && (parent[n - 1] == '/' || parent[n - 1] == '\\')) n--;
    return path_prefix(child, parent, n) && (!child[n] || child[n] == '/' || child[n] == '\\');
}
static bool overlap(const char *a, const char *b) {
    char ra[1100], rb[1100];
    if (!resolve(a, ra, sizeof ra) || !resolve(b, rb, sizeof rb)) return true;
    return within(ra, rb) || within(rb, ra);
}
static bool same_dir(const char *a, const char *b) {
    char ra[1100], rb[1100];
    return resolve(a, ra, sizeof ra) && resolve(b, rb, sizeof rb) && path_eq(ra, rb);
}

// Inside the app itself (the .app bundle, the mounted AppImage): never a data folder
static bool in_app(const char *p) {
    char r[1100], app[1100];
    if (!resolve(p, r, sizeof r)) return false;
#ifdef __APPLE__
    char e[1024];
    exe_dir(e, sizeof e);
    char *c = strstr(e, ".app/Contents/");
    if (!c) return false;
    c[4] = 0;
    return resolve(e, app, sizeof app) && within(r, app);
#elif defined(__linux__)
    const char *d = getenv("APPDIR");
    return d && *d && resolve(d, app, sizeof app) && within(r, app);
#else
    (void)app;
    return false;
#endif
}

// The data folder a pointer file names (false: none, or it points into the app)
static bool pointer_root(const char *home, char *out, size_t n) {
    char p[1100];
    join(p, sizeof p, home, POINTER_FILE);
    if (!read_line(p, out, n)) return false;
    if (in_app(out)) {
        fprintf(stderr, "[paths] %s points into the app (%s): ignored\n", p, out);
        return false;
    }
    return true;
}

// ---- other apps' data folders (rt_data_root_foreign). The related recomps share this runtime's file names
// (larecomp.ini, lasrecomp.log, save/, extracted/), so a folder of theirs used as PokeL-ARecomp's data root would get
// its saves, settings, logs and game files mixed or overwritten.
static const struct { const char *dir, *app; } FOREIGN_DIRS[] = {
    {"lasrecomp", "LAS Recomp"},       {"zarecomp", "ZA Recomp"},     {"pokev-recomp", "PokeV-Recomp"},  // Linux
    {"LAS Recomp", "LAS Recomp"},      {"ZA Recomp", "ZA Recomp"},    {"PokeV-Recomp", "PokeV-Recomp"},  // data moves
};
// (exact case: a checkout called LASRecomp or a folder named "las recomp notes" is not one of them)

// The first line starting with prefix in a file's first 64 KB (false: none, or no file)
static bool file_line(const char *path, const char *prefix, char *out, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    static char buf[65536];
    size_t k = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[k] = 0;
    size_t pl = strlen(prefix);
    for (char *l = buf; l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL)
        if (!strncmp(l, prefix, pl)) {
            snprintf(out, n, "%.*s", (int)strcspn(l, "\r\n"), l);
            return true;
        }
    return false;
}

bool rt_data_root_foreign(const char *root, char *why, size_t n) {
    char r[1100], line[256], p[1100], what[160];
    if (!resolve(root, r, sizeof r)) snprintf(r, sizeof r, "%s", root);  // (not there yet: as given)
    for (const char *c = r; *c;) {  // every path component
        size_t k = strcspn(c, "/\\");
        for (size_t i = 0; i < sizeof FOREIGN_DIRS / sizeof *FOREIGN_DIRS; i++)
            if (k == strlen(FOREIGN_DIRS[i].dir) && !strncmp(c, FOREIGN_DIRS[i].dir, k)) {
                if (why) snprintf(why, n, "it is %s's data folder (%s)", FOREIGN_DIRS[i].app, FOREIGN_DIRS[i].dir);
                return true;
            }
        c += k;
        while (*c == '/' || *c == '\\') c++;
    }
    join(p, sizeof p, r, "extracted");
    if (rt_game_files_owner(p, what, sizeof what) == RT_GAME_FOREIGN) {
        if (why) snprintf(why, n, "it holds %s's game files, from another app", what);
        return true;
    }
    static const char *const OTHERS[] = {"LAS Recomp", "PokeV-Recomp"};  // (not "ZA Recomp": see paths.h)
    for (int pass = 0; pass < 2; pass++) {  // larecomp.ini's "# <app> settings" line, lasrecomp.log's "[info] <app> v..."
        join(p, sizeof p, r, pass ? "lasrecomp.log" : "larecomp.ini");
        if (!file_line(p, pass ? "[info] " : "# ", line, sizeof line)) continue;
        const char *name = line + (pass ? 7 : 2);
        for (size_t i = 0; i < sizeof OTHERS / sizeof *OTHERS; i++) {
            size_t k = strlen(OTHERS[i]);
            if (!strncmp(name, OTHERS[i], k) && name[k] == ' ') {
                if (why) snprintf(why, n, "its %s is %s's", pass ? "log (lasrecomp.log)" : "settings file (larecomp.ini)",
                                  OTHERS[i]);
                return true;
            }
        }
    }
    return false;
}

// The data root's checks apply to every folder but PokeL-ARecomp's own default one (default_home, no
// LARECOMP_DATA_HOME): that one starts whatever it holds - Violet's files left there by PokeV-Recomp v0.0.24 and older
// are the setup's to refuse, not a reason not to start.
static bool own_default(const char *root) {
    const char *t = getenv("LARECOMP_DATA_HOME");
    char home[1024];
    default_home(home, sizeof home);
    return !(t && *t) && same_dir(root, home);
}

bool rt_data_root_early(char *out, size_t n) {
    const char *env = getenv("LARECOMP_DATA_ROOT");
    if (env && *env) return (size_t)snprintf(out, n, "%s", env) < n && !rt_data_root_foreign(out, NULL, 0);
    if (is_dir("extracted")) return false;  // (dev mode)
    char home[1024];
    default_home(home, sizeof home);
    if (!pointer_root(home, out, n) || !is_dir(out) || rt_data_root_foreign(out, NULL, 0)) {
        snprintf(out, n, "%s", home);
        if (!own_default(home) && rt_data_root_foreign(home, NULL, 0)) return false;  // (LARECOMP_DATA_HOME: refused)
    }
    return true;
}

// The entries of a data folder (DATA_ENTRIES) removed; links themselves, never what they point to
static bool remove_entries(const char *dir) {
    bool ok = true;
    for (size_t i = 0; i < N_ENTRIES; i++) {
        char p[1100];
        join(p, sizeof p, dir, DATA_ENTRIES[i]);
        if (!is_link(p) && !exists(p)) continue;
        if (is_dir(p) && !is_link(p)) ok &= plat_remove_tree(p);
#ifdef _WIN32
        else if (is_dir(p)) ok &= RemoveDirectoryA(p) != 0;  // (a junction)
#endif
        else ok &= remove(p) == 0;
    }
    return ok;
}

// An unfinished copy (MARK_PARTIAL) in dir removed: what a move had put there
static void remove_partial(const char *dir) {
    char p[1100];
    fprintf(stderr, "[paths] removing an unfinished copy of the data in %s\n", dir);
    remove_entries(dir);
    join(p, sizeof p, dir, NOTE_FROM);
    remove(p);
    join(p, sizeof p, dir, MARK_PARTIAL);
    remove(p);
}

// What a move left to do, from the data folder now in use (root):
//  - it was a move's source, and the move never switched over: the copy there goes
//  - it is a move's destination: the old folder's data goes (only once the switch is made: the pointer names root)
static void finish_moves(const char *root, void (*progress)(int, double)) {
    char note[1100], other[1024], p[1100], back[1024];
    join(note, sizeof note, root, NOTE_TO);
    if (read_line(note, other, sizeof other)) {
        join(p, sizeof p, other, MARK_PARTIAL);
        bool partial = exists(p);
        join(p, sizeof p, other, NOTE_FROM);
        bool ours = partial || (read_line(p, back, sizeof back) && same_dir(back, root));
        if (ours && !overlap(root, other)) remove_partial(other);
        remove(note);
    }
    join(note, sizeof note, root, NOTE_FROM);
    if (read_line(note, other, sizeof other)) {
        if (!overlap(root, other) && is_dir(other)) {
            fprintf(stderr, "[paths] removing the old copy of the data in %s\n", other);
            join(p, sizeof p, other, NOTE_TO);
            remove(p);  // (first: that folder can never undo this move)
            for (size_t i = 0; i < N_ENTRIES; i++) {
                char e[1100];
                join(e, sizeof e, other, DATA_ENTRIES[i]);
                if (is_dir(e) && !is_link(e)) plat_remove_tree(e);
                else remove(e);
                if (progress) progress(2, (double)(i + 1) / N_ENTRIES);
            }
            if (!same_dir(other, g_home)) rmdir(other);  // (only if empty)
        }
        remove(note);
    }
}

void rt_paths_init(void) {
    if (!getcwd(g_start, sizeof g_start)) g_start[0] = 0;
    exe_dir(g_exe_dir, sizeof g_exe_dir);
    default_home(g_home, sizeof g_home);
    char root[1024];
    const char *env = getenv("LARECOMP_DATA_ROOT");
    if (env && *env) snprintf(root, sizeof root, "%s", env);
    else if (is_dir("extracted")) snprintf(root, sizeof root, "%s", g_start), g_dev = true;  // dev / portable: as before
    else {
        snprintf(root, sizeof root, "%s", g_home);
        char ptr[1024];
        while (pointer_root(g_home, ptr, sizeof ptr)) {  // another folder chosen on the start screen
            // (another app's folder: refused below, before writable() writes its test file in there)
            if (is_dir(ptr) && (rt_data_root_foreign(ptr, NULL, 0) || writable(ptr))) {
                snprintf(root, sizeof root, "%s", ptr);
                break;
            }
            char msg[1400];
            snprintf(msg, sizeof msg, "PokeL-ARecomp's data folder isn't available:\n%s\n\nIf it's on a drive that isn't "
                     "connected, connect it and try again.", ptr);
            const SDL_MessageBoxButtonData b[] = {{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Try again"},
                                                  {0, 1, "Use the default folder"},
                                                  {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Quit"}};
            const SDL_MessageBoxData box = {SDL_MESSAGEBOX_WARNING, NULL, "PokeL-ARecomp", msg, 3, b, NULL};
            int k = 2;
            if (!SDL_ShowMessageBox(&box, &k)) fprintf(stderr, "%s\n", msg);
            if (k == 2) exit(1);
            if (k == 1) {  // (the data there stays; the start screen can switch back to it)
                char p[1100];
                join(p, sizeof p, g_home, POINTER_FILE);
                remove(p);
            }
        }
    }
    char msg[1400], why[200];
    if (!own_default(root) && rt_data_root_foreign(root, why, sizeof why)) {  // (before anything is written there)
        char ptr[1100];
        join(ptr, sizeof ptr, g_home, POINTER_FILE);
        const char *t = getenv("LARECOMP_DATA_HOME");
        snprintf(msg, sizeof msg, "PokeL-ARecomp won't use this folder as its data folder:\n%s\n\nIt looks like another "
                 "app's: %s. Nothing in it was changed.\n\n%s", root, why,
                 env && *env ? "Unset LARECOMP_DATA_ROOT, or point it at another folder."
                 : g_dev ? "Start PokeL-ARecomp from another folder (it was started from inside this one)."
                 : same_dir(root, g_home) && t && *t ? "Unset LARECOMP_DATA_HOME, or point it at another folder."
                         : "Delete data_root.txt in PokeL-ARecomp's own data folder to use that folder again.");
        if (!g_dev && !(env && *env)) fprintf(stderr, "[paths] (data_root.txt: %s)\n", ptr);
        fail(msg);
    }
    mkdirs(root);
    if (!is_dir(root) || !writable(root) || chdir(root) != 0) {
#ifdef _WIN32
        snprintf(msg, sizeof msg, "PLA Recomp can't write to its folder:\n%s\n\nIt keeps game files and saves "
                 "next to the program. Move the \"PLA Recomp\" folder somewhere you can write to (e.g. Documents or "
                 "Desktop, not Program Files) and start it again.", root);
#else
        snprintf(msg, sizeof msg, "PLA Recomp can't write to its data folder:\n%s", root);
#endif
        fail(msg);
    }
    if (!getcwd(g_root, sizeof g_root)) snprintf(g_root, sizeof g_root, "%s", root);
    fprintf(stderr, "[paths] data: %s\n", g_root);
    if (!g_dev && !(env && *env)) finish_moves(g_root, NULL);
    rt_game_stage_recover();  // (a setup cut off last time: its staging folder, never extracted/)
    // Pokémon Z-A's release settings (the upscaler, the anti-aliasing, video_recommended = auto...): the app ships them
    // as za-default.ini (tools/za/dist.sh, dist-linux.sh) and a first run copies them into the data folder as the
    // player's larecomp.ini. The release zip no longer holds a larecomp.ini, so unzipping a new version over an install
    // keeps the player's settings (Windows: the zip used to replace them). Not with LARECOMP_SETTINGS (test runs); a
    // source checkout has no za-default.ini next to its executable, so nothing is copied there.
    char def[1100];
    if (!getenv("LARECOMP_SETTINGS") && !exists("larecomp.ini") && rt_resource("za-default.ini", NULL, def, sizeof def)) {
        FILE *in = fopen(def, "rb"), *out = in ? fopen("larecomp.ini", "wb") : NULL;
        char buf[4096];
        size_t k;
        while (out && (k = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, k, out);
        if (in) fclose(in);
        if (out) fclose(out), fprintf(stderr, "[paths] first run: the release's settings from %s\n", def);
    }
}

const char *rt_data_root(void) { return g_root; }

bool rt_resource(const char *name, const char *dev_rel, char *out, size_t n) {
    char c[4][1100] = {{0}};
    if (name) {
        snprintf(c[0], sizeof c[0], "%s" SEP "%s", g_exe_dir, name);
        snprintf(c[1], sizeof c[1], "%s" SEP ".." SEP "Resources" SEP "%s", g_exe_dir, name);  // .app/Contents/Resources
    }
    if (dev_rel) {
        snprintf(c[2], sizeof c[2], "%s" SEP "%s", g_start, dev_rel);
        snprintf(c[3], sizeof c[3], "%s" SEP ".." SEP "%s", g_exe_dir, dev_rel);  // build/lasrecomp -> repo root
    }
    for (int i = 0; i < 4; i++)
        if (c[i][0] && exists(c[i])) return (size_t)snprintf(out, n, "%s", c[i]) < n;  // (as is: a venv python must stay a link)
    return false;
}

// ---- game files
// What the setup (runtime/ui/launcher.mm) creates in extracted/ (aoc/ and import.log: a package import's), plus what
// older versions' extraction and manual extractions left there (never read at run time). exefs last: a delete cut
// off half way leaves exefs/main, so what is left still reads as ours (rt_game_files_owner) and can be finished.
static const char *const GAME_FILES[] = {"romfs", "romfs.bin", GAMELIB_NAME, GAMELIB_NAME ".id", ".gamebuild",
                                         ".zigcache", ".extract", "extract.log", "xci_secure", "nso_raw", "update_pfs",
                                         "aoc", "import.log", ".DS_Store", "exefs"};
#define N_GAME_FILES (sizeof GAME_FILES / sizeof *GAME_FILES)
#define STAGE_DIR ".plarecomp-import"         // in the data root: the setup's staging folder (only this app writes it)
#define STAGE_LOCK ".plarecomp-import.lock"   // in the data root: locked while a setup runs (rt_game_stage_begin)

// <data root>/extracted with links resolved (false: no data root, or it can't be resolved)
static bool game_dir_want(char *want, size_t n) {
    char root[1100];
    if (!g_root[0] || !resolve(g_root, root, sizeof root)) return false;
    return (size_t)snprintf(want, n, "%s" SEP "extracted", root) < n;
}

// game_dir must be <data root>/extracted, a real folder (a dev worktree's symlinked extracted/ is refused)
static bool game_dir_ok(const char *game_dir) {
    char want[1100], got[1100];
    if (is_link(game_dir)) {
        fprintf(stderr, "[paths] %s is a link (symlink / junction): its game files are not ours to delete\n", game_dir);
        return false;
    }
    if (!game_dir_want(want, sizeof want) || !resolve(game_dir, got, sizeof got)) return false;
    if (!path_eq(want, got)) {
        fprintf(stderr, "[paths] %s resolves to %s, not the data folder's %s\n", game_dir, got, want);
        return false;
    }
    return true;
}

// game_dir is <data root>/extracted, or would be once made: when it isn't there yet, its parent is the data root
static bool game_dir_target(const char *game_dir) {
    if (exists(game_dir) || is_link(game_dir)) return game_dir_ok(game_dir);
    char parent[1100], root[1100], got[1100];
    snprintf(parent, sizeof parent, "%s", game_dir);
    char *s = strrchr(parent, '/'), *b = strrchr(parent, '\\');
    if (b > s) s = b;
    if (strcmp(s ? s + 1 : parent, "extracted")) return false;
    if (s) *s = 0;
    else snprintf(parent, sizeof parent, ".");
    return g_root[0] && resolve(g_root, root, sizeof root) && resolve(parent[0] ? parent : SEP, got, sizeof got) &&
           path_eq(root, got);
}

// ---- whose game files (rt_game_files_owner): from what they are, never from a marker or a folder name
typedef struct {
    bool ours, foreign, unknown;
    char what[160];  // the first foreign build, else the first unknown one
} Owner;

static void owner_note(Owner *o, bool foreign, const char *what) {
    if (foreign && !o->foreign) snprintf(o->what, sizeof o->what, "%s", what);
    else if (!foreign && !o->foreign && !o->unknown) snprintf(o->what, sizeof o->what, "%s", what);
    if (foreign) o->foreign = true;
    else o->unknown = true;
}

// One "main" module. exefs/main (required) must be an NSO with a known build; another file called main counts only
// when it is an NSO (a leftover folder's copy of some game's main).
static void owner_main(Owner *o, const char *path, bool required) {
    uint8_t id[8];
    char magic[4] = {0}, what[160];
    FILE *f = fopen(path, "rb");
    bool nso = f && fread(magic, 1, 4, f) == 4 && !memcmp(magic, "NSO0", 4);
    if (f) fclose(f);
    if (!nso) {
        if (required) owner_note(o, false, "an exefs/main that isn't a game program");
        return;
    }
    if (!game_identity_read(path, id)) {
        owner_note(o, false, "an unreadable exefs/main");
        return;
    }
    const GameIdentity *g = game_identity_find(id);
    if (g && g->supported) { o->ours = true; return; }
    game_identity_describe(id, what, sizeof what);
    owner_note(o, g != NULL, what);
}

// Every NSO called main under dir (links not followed: they are never deleted through either)
static void owner_tree(Owner *o, const char *dir, int depth) {
    if (depth > 8) return;
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[2048];
        snprintf(p, sizeof p, "%s" SEP "%s", dir, e->d_name);
        if (is_link(p)) continue;
        if (is_dir(p)) owner_tree(o, p, depth + 1);
        else if (!strcmp(e->d_name, "main")) owner_main(o, p, false);
    }
    closedir(d);
}

RtGameOwner rt_game_files_owner(const char *game_dir, char *what, size_t n) {
    Owner o = {0};
    bool any = false;
    for (size_t i = 0; i < N_GAME_FILES; i++) {
        const char *name = GAME_FILES[i];
        char p[1100];
        snprintf(p, sizeof p, "%s" SEP "%s", game_dir, name);
        if (!exists(p) && !is_link(p)) continue;
        if (!strcmp(name, ".DS_Store")) continue;  // (a file manager's, not a game file)
        any = true;
        if (!strcmp(name, "exefs")) {
            char m[1100];
            snprintf(m, sizeof m, "%s" SEP "main", p);
            if (is_link(p) || is_link(m)) owner_note(&o, false, "a linked exefs/main");
            else if (!exists(m)) owner_note(&o, false, "game files without an exefs/main");
            else owner_main(&o, m, true);
        } else if (is_dir(p) && !is_link(p) && strcmp(name, "romfs") && strcmp(name, ".gamebuild") &&
                   strcmp(name, ".zigcache") && strcmp(name, "aoc"))
            owner_tree(&o, p, 1);  // (leftover folders: nso_raw/, update_pfs/, .extract/... - not the game's data)
    }
    RtGameOwner r = !any ? RT_GAME_NONE : o.foreign ? RT_GAME_FOREIGN : o.unknown ? RT_GAME_UNKNOWN
                  : o.ours ? RT_GAME_OURS : RT_GAME_UNKNOWN;
    if (r == RT_GAME_UNKNOWN && !o.unknown) snprintf(o.what, sizeof o.what, "game files without an exefs/main");
    if (what && n) snprintf(what, n, "%s", r == RT_GAME_OURS ? "Pok\xc3\xa9mon Legends: Arceus 1.1.1" : o.what);
    return r;
}

// ---- deleting game files
// One entry of game_dir (already checked: <data root>/extracted, our files): never a link, never outside game_dir
static bool remove_entry(const char *game_dir, const char *name) {
    char p[1100];
    snprintf(p, sizeof p, "%s" SEP "%s", game_dir, name);
    if (!exists(p) && !is_link(p)) return true;
    if (is_link(p)) {
        fprintf(stderr, "[paths] refusing to delete %s (a link)\n", p);
        return false;
    }
    // the entry itself must resolve inside <data root>/extracted too (belt and braces: neither it nor game_dir is a link)
    char want[1100], got[1100];
    size_t wl = game_dir_want(want, sizeof want) ? strlen(want) : 0;
    if (!wl || !resolve(p, got, sizeof got) || strlen(got) <= wl || !path_prefix(got, want, wl) ||
        (got[wl] != '/' && got[wl] != '\\')) {
        fprintf(stderr, "[paths] refusing to delete %s (resolves outside %s)\n", p, wl ? want : "the data folder");
        return false;
    }
    return is_dir(p) ? plat_remove_tree(p) : remove(p) == 0;  // (plat_remove_tree doesn't follow links inside)
}

// game_dir is ours to delete in: <data root>/extracted, holding Legends: Arceus 1.1.1's files only (or none).
// Another game's build (Violet, Z-A, Link's Awakening) or files that can't be identified are refused, whatever else
// is in there: they may be another recomp's (its folder used as this app's data root, or Violet's from PokeV-Recomp
// v0.0.24 and older, which set Violet up in plarecomp/).
static bool ours_to_delete(const char *game_dir, const char *what_for) {
    if (!game_dir_ok(game_dir)) {
        fprintf(stderr, "[paths] refusing to delete %s in %s (not <data root>/extracted)\n", what_for, game_dir);
        return false;
    }
    char what[160];
    RtGameOwner o = rt_game_files_owner(game_dir, what, sizeof what);
    if (o == RT_GAME_FOREIGN || o == RT_GAME_UNKNOWN) {
        fprintf(stderr, "[paths] refusing to delete %s in %s: the game files there are %s (%s)\n", what_for, game_dir,
                what, o == RT_GAME_FOREIGN ? "another app's" : "not identified");
        return false;
    }
    return true;
}

static bool known_game_file(const char *name) {
    for (size_t i = 0; i < N_GAME_FILES; i++)
        if (!strcmp(name, GAME_FILES[i])) return true;
    return false;
}

bool rt_remove_game_file(const char *game_dir, const char *name) {
    if (!known_game_file(name)) {
        fprintf(stderr, "[paths] refusing to delete %s/%s (not app-created game files in the data root)\n", game_dir, name);
        return false;
    }
    return ours_to_delete(game_dir, name) && remove_entry(game_dir, name);
}

bool rt_remove_game_files(const char *game_dir) {
    if (!ours_to_delete(game_dir, "the game files")) return false;
    bool ok = true;
    for (size_t i = 0; i < N_GAME_FILES; i++) ok &= remove_entry(game_dir, GAME_FILES[i]);  // (exefs last)
    rmdir(game_dir);  // only if empty: anything else in there stays
    return ok;
}

// ---- setting the game files up: staged, checked, then moved into place (paths.h)
static char g_stage[1100];  // the staging folder of the setup in progress ("" = none)
#ifdef _WIN32
static HANDLE g_stage_lock = INVALID_HANDLE_VALUE;
static bool stage_lock(void) {
    char p[1100];
    if (g_stage_lock != INVALID_HANDLE_VALUE || !g_root[0]) return false;
    join(p, sizeof p, g_root, STAGE_LOCK);
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    OVERLAPPED ov;
    memset(&ov, 0, sizeof ov);
    if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)) {
        CloseHandle(h);
        return false;
    }
    g_stage_lock = h;
    return true;
}
static void stage_unlock(void) {
    if (g_stage_lock != INVALID_HANDLE_VALUE) CloseHandle(g_stage_lock), g_stage_lock = INVALID_HANDLE_VALUE;
}
#else
static int g_stage_lock = -1;
static bool stage_lock(void) {  // (flock: released by the system when the process ends, however it ends)
    char p[1100];
    if (g_stage_lock >= 0 || !g_root[0]) return false;
    join(p, sizeof p, g_root, STAGE_LOCK);
    int fd = open(p, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (fd < 0) return false;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return false;
    }
    g_stage_lock = fd;
    return true;
}
static void stage_unlock(void) {
    if (g_stage_lock >= 0) close(g_stage_lock), g_stage_lock = -1;
}
#endif

// <data root>/.plarecomp-import (false: no data root)
static bool stage_path(char *out, size_t n) {
    char root[1100];
    return g_root[0] && resolve(g_root, root, sizeof root) && (size_t)snprintf(out, n, "%s" SEP STAGE_DIR, root) < n;
}

// The staging folder removed (with the setup lock held): a link there goes, never what it points to
static bool stage_remove(const char *stage) {
    if (is_link(stage)) {
#ifdef _WIN32
        if (is_dir(stage)) return RemoveDirectoryA(stage) != 0;  // (a junction)
#endif
        return remove(stage) == 0;
    }
    if (!exists(stage)) return true;
    fprintf(stderr, "[paths] removing the setup's staging folder %s\n", stage);
    return is_dir(stage) ? plat_remove_tree(stage) : remove(stage) == 0;
}

// game_dir holds nothing but game-file entries (and file managers' leftovers): what a setup may replace whole.
// other: the first entry that isn't one (the player's own file, say), for the message.
static bool only_game_files(const char *game_dir, char *other, size_t n) {
    DIR *d = opendir(game_dir);
    if (!d) return true;
    struct dirent *e;
    bool only = true;
    while (only && (e = readdir(d))) {
        const char *x = e->d_name;
        if (!strcmp(x, ".") || !strcmp(x, "..") || !strcmp(x, "Thumbs.db") || !strcmp(x, "desktop.ini")) continue;
        bool known = false;
        for (size_t i = 0; i < N_GAME_FILES; i++) known |= !strcmp(x, GAME_FILES[i]);
        if (!known) only = false, snprintf(other, n, "%s", x);
    }
    closedir(d);
    return only;
}

static void owner_refusal(RtGameOwner o, const char *what, char *err, size_t n) {
    if (o == RT_GAME_FOREIGN)
        snprintf(err, n, "The data folder's extracted folder holds %s's files, from another app: they are left as they "
                 "are. Move the extracted folder out of the data folder yourself, then set the game up again.", what);
    else
        snprintf(err, n, "The data folder's extracted folder holds game files PokeL-ARecomp can't identify (%s): they "
                 "are left as they are. Move the extracted folder out of the data folder yourself, then set the game up "
                 "again.", what);
}

bool rt_game_stage_begin(const char *game_dir, char *stage, size_t n, char *err, size_t errn) {
    char what[160];
    if (errn) err[0] = 0;
    if (!game_dir_target(game_dir)) {
        snprintf(err, errn, "%s isn't the data folder's extracted folder: PokeL-ARecomp only sets the game up there.",
                 game_dir);
        return false;
    }
    RtGameOwner o = rt_game_files_owner(game_dir, what, sizeof what);
    if (o == RT_GAME_FOREIGN || o == RT_GAME_UNKNOWN) {
        owner_refusal(o, what, err, errn);
        return false;
    }
    char other[256];
    if (!only_game_files(game_dir, other, sizeof other)) {
        snprintf(err, errn, "%s holds \"%s\", which isn't a game file: move it out, then set the game up again.", game_dir,
                 other);
        return false;
    }
    if (!stage_lock()) {
        snprintf(err, errn, "Another copy of PokeL-ARecomp is setting the game up in this data folder.");
        return false;
    }
    if (!stage_path(stage, n) || !stage_remove(stage) || mkdir(stage, 0755) != 0 || is_link(stage) || !is_dir(stage)) {
        snprintf(err, errn, "Couldn't make the setup's folder in the data folder (%s).", strerror(errno));
        stage_unlock();
        return false;
    }
    snprintf(g_stage, sizeof g_stage, "%s", stage);
    fprintf(stderr, "[paths] setup: staging in %s\n", stage);
    return true;
}

void rt_game_stage_abort(void) {
    if (g_stage[0]) stage_remove(g_stage);
    g_stage[0] = 0;
    stage_unlock();
}

bool rt_game_stage_commit(const char *game_dir, const char *stage, char *err, size_t errn) {
    char what[160], p[1100];
    if (errn) err[0] = 0;
    if (!g_stage[0] || !path_eq(stage, g_stage) || is_link(stage) || !is_dir(stage)) {
        snprintf(err, errn, "The setup's folder changed while it ran.");
        rt_game_stage_abort();
        return false;
    }
    RtGameOwner o = rt_game_files_owner(stage, what, sizeof what);
    if (o != RT_GAME_OURS) {  // (the start screen says which game it was before getting here; this is the last word)
        snprintf(err, errn, o == RT_GAME_NONE ? "No game files came out of the setup." : "These files are %s.", what);
        rt_game_stage_abort();
        return false;
    }
    char romfs[1100];
    join(p, sizeof p, stage, "romfs.bin");
    join(romfs, sizeof romfs, stage, "romfs");
    if (!exists(p) && !is_dir(romfs)) {
        snprintf(err, errn, "The game's data (romfs) is missing from what was set up.");
        rt_game_stage_abort();
        return false;
    }
    // extracted/ again: anything may have changed there while the files were read (another app, the player)
    if (!game_dir_target(game_dir) ||
        ((o = rt_game_files_owner(game_dir, what, sizeof what)) == RT_GAME_FOREIGN || o == RT_GAME_UNKNOWN)) {
        if (o == RT_GAME_FOREIGN || o == RT_GAME_UNKNOWN) owner_refusal(o, what, err, errn);
        else snprintf(err, errn, "%s isn't the data folder's extracted folder any more.", game_dir);
        rt_game_stage_abort();
        return false;
    }
    char other[256];
    if (!only_game_files(game_dir, other, sizeof other)) {  // (checked before anything is removed)
        snprintf(err, errn, "%s holds \"%s\", which isn't a game file: move it out, then set the game up again.", game_dir,
                 other);
        rt_game_stage_abort();
        return false;
    }
    if ((exists(game_dir) || is_link(game_dir)) && !rt_remove_game_files(game_dir)) {
        snprintf(err, errn, "Couldn't remove the previous game files from %s.", game_dir);
        rt_game_stage_abort();
        return false;
    }
    if (exists(game_dir) || is_link(game_dir)) {
        snprintf(err, errn, "%s still holds files that aren't game files: move them out, then set the game up again.",
                 game_dir);
        rt_game_stage_abort();
        return false;
    }
    if (rename(stage, game_dir) != 0) {  // (same folder, same drive: one step)
        snprintf(err, errn, "Couldn't move the new game files into %s (%s).", game_dir, strerror(errno));
        rt_game_stage_abort();
        return false;
    }
    fprintf(stderr, "[paths] setup: %s in place\n", game_dir);
    g_stage[0] = 0;
    stage_unlock();
    return true;
}

void rt_game_stage_recover(void) {
    char stage[1100];
    if (!stage_path(stage, sizeof stage) || (!exists(stage) && !is_link(stage))) return;
    if (!stage_lock()) {  // (another copy of the app is setting up right now: its folder)
        fprintf(stderr, "[paths] %s: a setup is running in another copy of PokeL-ARecomp - left as it is\n", stage);
        return;
    }
    fprintf(stderr, "[paths] a setup was cut off: its staging folder goes (extracted/ is left as it is)\n");
    stage_remove(stage);
    stage_unlock();
}

#ifdef _WIN32
// Sizes skip only real links (symlinks, junctions): OneDrive's cloud files are reparse points too, but they're files
static bool size_link(const char *p) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(p, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    return (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && IsReparseTagNameSurrogate(fd.dwReserved0);
}
#else
#define size_link is_link
#endif

static unsigned long long tree_size(const char *p) {
    struct stat st;
    if (size_link(p) || stat(p, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return (unsigned long long)st.st_size;
    unsigned long long n = 0;
    DIR *d = opendir(p);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char c[2048];
            snprintf(c, sizeof c, "%s" SEP "%s", p, e->d_name);
            n += tree_size(c);
        }
        closedir(d);
    }
    return n;
}

unsigned long long rt_tree_size(const char *path) { return tree_size(path); }

unsigned long long rt_game_files_size(const char *game_dir) {
    unsigned long long n = 0;
    char p[1100];
    for (size_t i = 0; i < N_GAME_FILES; i++) {
        snprintf(p, sizeof p, "%s" SEP "%s", game_dir, GAME_FILES[i]);
        n += tree_size(p);
    }
    return n;
}

// ---- choosing another data folder, moving the data there (the start screen: runtime/ui/launcher.mm)
const char *rt_data_root_fixed(void) {
    const char *env = getenv("LARECOMP_DATA_ROOT");
    if (env && *env) return "Set by LARECOMP_DATA_ROOT.";
    if (g_dev) return "A source checkout uses its own folder.";
    return NULL;
}

static bool has_entries(const char *dir) {
    for (size_t i = 0; i < N_ENTRIES; i++) {
        char p[1100];
        join(p, sizeof p, dir, DATA_ENTRIES[i]);
        if (exists(p) || is_link(p)) return true;
    }
    return false;
}

// nothing in it but what file managers leave behind
static bool empty_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return true;
    struct dirent *e;
    bool empty = true;
    while (empty && (e = readdir(d))) {
        const char *n = e->d_name;
        empty = !strcmp(n, ".") || !strcmp(n, "..") || !strcmp(n, ".DS_Store") || !strcmp(n, ".localized") ||
                !strcmp(n, "desktop.ini") || !strcmp(n, "Thumbs.db");
    }
    closedir(d);
    return empty;
}

static unsigned long long free_space(const char *dir) {
#ifdef _WIN32
    ULARGE_INTEGER a;
    return GetDiskFreeSpaceExA(dir, &a, NULL, NULL) ? a.QuadPart : 0;
#else
    struct statvfs v;
    return statvfs(dir, &v) == 0 ? (unsigned long long)v.f_bavail * v.f_frsize : 0;
#endif
}

static unsigned long long entries_size(const char *dir) {
    unsigned long long n = 0;
    for (size_t i = 0; i < N_ENTRIES; i++) {
        char p[1100];
        join(p, sizeof p, dir, DATA_ENTRIES[i]);
        n += tree_size(p);
    }
    return n;
}

// PokeL-ARecomp's data (an unfinished copy too), or nothing yet
static bool usable_dir(const char *dir) {
    char p[1100];
    join(p, sizeof p, dir, MARK_PARTIAL);
    return !is_dir(dir) || has_entries(dir) || exists(p) || empty_dir(dir);
}

void rt_data_target(const char *picked, RtDataTarget *t) {
    memset(t, 0, sizeof *t);
    char r[1100], root[1100], p[1100];
    snprintf(t->path, sizeof t->path, "%s", picked);
    if (!resolve(picked, r, sizeof r) || !is_dir(r) || !resolve(g_root, root, sizeof root)) {
        snprintf(t->refuse, sizeof t->refuse, "That folder can't be opened.");
        return;
    }
    // the folder itself when it's empty, PokeL-ARecomp's data, or the platform's folder; otherwise a "PokeL-ARecomp"
    // folder in it (its own name: LAS Recomp, which this runtime comes from, makes a "LAS Recomp" one)
    if (same_dir(r, g_home) || usable_dir(r)) snprintf(t->path, sizeof t->path, "%s", r);
    else join(t->path, sizeof t->path, r, "PokeL-ARecomp");
    join(p, sizeof p, t->path, MARK_PARTIAL);
    t->has_data = has_entries(t->path) && !exists(p);
    t->bytes = entries_size(g_root);
    t->free = free_space(r);
    struct stat a, b;
    bool same_fs = stat(root, &a) == 0 && stat(r, &b) == 0 && a.st_dev == b.st_dev;  // (hard links: no second copy)
    t->need = (same_fs ? 0 : t->bytes) + (64ull << 20);
    if (path_eq(t->path, root)) snprintf(t->refuse, sizeof t->refuse, "That's the data folder already.");
    else if (within(t->path, root)) snprintf(t->refuse, sizeof t->refuse, "That folder is inside the data folder.");
    else if (within(root, t->path)) snprintf(t->refuse, sizeof t->refuse, "That folder holds the data folder.");
    else if (in_app(r)) snprintf(t->refuse, sizeof t->refuse, "That folder is inside the app.");
    else if (rt_data_root_foreign(t->path, NULL, 0)) snprintf(t->refuse, sizeof t->refuse, "That folder is another app's.");
    else if (!writable(r)) snprintf(t->refuse, sizeof t->refuse, "PokeL-ARecomp can't write to that folder.");
    else if (!same_dir(t->path, g_home) && !usable_dir(t->path))
        snprintf(t->refuse, sizeof t->refuse, "Choose an empty folder.");
}

// ---- the move: copy, check the copy, switch over, then empty the old folder
typedef struct {
    void (*progress)(int step, double frac);
    unsigned long long done, total;
    int step;
    char *err;
    size_t errn;
    char *buf, *buf2;
} Move;

static void moved(Move *m, unsigned long long n) {
    m->done += n;
    if (m->progress) m->progress(m->step, m->total ? (double)m->done / (double)m->total : 1.0);
}

static bool move_fail(Move *m, const char *what, const char *p) {
    if (!m->err[0]) snprintf(m->err, m->errn, "%s %s (%s)", what, p, errno ? strerror(errno) : "differs");
    return false;
}

#ifdef _WIN32
static int lstat_(const char *p, struct stat *st) {
    if (is_link(p)) { errno = EPERM; return -1; }  // (links in the data folder: not moved on Windows)
    return stat(p, st);
}
#ifndef S_ISLNK
#define S_ISLNK(m) 0
#endif
#else
#define lstat_ lstat
#endif

#define MOVE_BUF (1u << 20)
static bool copy_file(Move *m, const char *s, const char *d, const struct stat *st) {
    remove(d);
    static const char *no_links;  // (tests: LARECOMP_MOVE_NO_LINKS=1 copies as if to another drive)
    if (!no_links) no_links = getenv("LARECOMP_MOVE_NO_LINKS") ? "1" : "";
#ifdef _WIN32
    if (!*no_links && CreateHardLinkA(d, s, NULL)) { moved(m, st->st_size); return true; }  // (same volume: a second
#else                                                                                       //  name, no copy)
    if (!*no_links && link(s, d) == 0) { moved(m, st->st_size); return true; }
#endif
    FILE *in = fopen(s, "rb"), *out = in ? fopen(d, "wb") : NULL;
    bool ok = in && out;
    errno = 0;
    while (ok) {
        size_t k = fread(m->buf, 1, MOVE_BUF, in);
        if (!k) { ok = !ferror(in); break; }
        ok = fwrite(m->buf, 1, k, out) == k;
        moved(m, k);
    }
    if (out) ok &= fflush(out) == 0 && fsync(fileno(out)) == 0, ok &= fclose(out) == 0;
    if (in) fclose(in);
    if (!ok) return move_fail(m, "Couldn't copy", s);
    struct utimbuf u = {st->st_atime, st->st_mtime};
    utime(d, &u);
    return true;
}

static bool copy_tree(Move *m, const char *s, const char *d) {
    struct stat st;
    if (lstat_(s, &st) != 0) return move_fail(m, "Couldn't read", s);
    if (S_ISLNK(st.st_mode)) {
#ifndef _WIN32
        char t[PATH_MAX];
        ssize_t k = readlink(s, t, sizeof t - 1);
        if (k < 0) return move_fail(m, "Couldn't read", s);
        t[k] = 0;
        remove(d);
        if (symlink(t, d) != 0) return move_fail(m, "Couldn't copy", s);
#endif
        return true;
    }
    if (S_ISREG(st.st_mode)) return copy_file(m, s, d, &st);
    if (!S_ISDIR(st.st_mode)) return true;  // (sockets and such: nothing of ours)
    if (mkdir(d, 0755) != 0 && !is_dir(d)) return move_fail(m, "Couldn't create", d);
    DIR *dir = opendir(s);
    if (!dir) return move_fail(m, "Couldn't read", s);
    bool ok = true;
    struct dirent *e;
    while (ok && (e = readdir(dir))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") || !strncmp(e->d_name, "._", 2)) continue;  // (._: macOS
        char cs[4096], cd[4096];                                                                         //  metadata)
        join(cs, sizeof cs, s, e->d_name);
        join(cd, sizeof cd, d, e->d_name);
        ok = copy_tree(m, cs, cd);
    }
    closedir(dir);
    return ok;
}

static bool same_bytes(Move *m, const char *a, const char *b) {
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    bool ok = fa && fb;
    while (ok) {
        size_t k = fread(m->buf, 1, MOVE_BUF, fa), j = fread(m->buf2, 1, MOVE_BUF, fb);
        ok = k == j && !memcmp(m->buf, m->buf2, k) && !ferror(fa) && !ferror(fb);
        moved(m, k);
        if (!k) break;
    }
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return ok;
}

static bool check_tree(Move *m, const char *s, const char *d) {
    struct stat a, b;
    errno = 0;
    if (lstat_(s, &a) != 0 || lstat_(d, &b) != 0) return move_fail(m, "Missing in the copy:", d);
    if ((a.st_mode & S_IFMT) != (b.st_mode & S_IFMT)) return move_fail(m, "Differs in the copy:", d);
    if (S_ISREG(a.st_mode)) {
        if (a.st_size != b.st_size) return move_fail(m, "Differs in the copy:", d);
#ifndef _WIN32
        if (a.st_dev == b.st_dev && a.st_ino == b.st_ino) { moved(m, a.st_size); return true; }  // (the same file)
#endif
        errno = 0;
        return same_bytes(m, s, d) || move_fail(m, "Differs in the copy:", d);
    }
#ifndef _WIN32
    if (S_ISLNK(a.st_mode)) {
        char ta[PATH_MAX] = {0}, tb[PATH_MAX] = {0};
        if (readlink(s, ta, sizeof ta - 1) < 0 || readlink(d, tb, sizeof tb - 1) < 0 || strcmp(ta, tb))
            return move_fail(m, "Differs in the copy:", d);
        return true;
    }
#endif
    if (!S_ISDIR(a.st_mode)) return true;
    DIR *dir = opendir(s);
    if (!dir) return move_fail(m, "Couldn't read", s);
    bool ok = true;
    struct dirent *e;
    while (ok && (e = readdir(dir))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") || !strncmp(e->d_name, "._", 2)) continue;  // (._: macOS
        char cs[4096], cd[4096];                                                                         //  metadata)
        join(cs, sizeof cs, s, e->d_name);
        join(cd, sizeof cd, d, e->d_name);
        ok = check_tree(m, cs, cd);
    }
    closedir(dir);
    return ok;
}

// The switch: the pointer file names `to` (none when it's the platform's folder)
static bool point_to(const char *to) {
    char p[1100], r[1100];
    join(p, sizeof p, g_home, POINTER_FILE);
    if (same_dir(to, g_home)) return remove(p) == 0 || !exists(p);
    if (!resolve(to, r, sizeof r)) return false;
    mkdirs(g_home);
    return write_line(p, r);
}

// A folder to use: created if needed, writable, with no unfinished copy or old move notes in it
static bool prepare_target(const char *to, char *err, size_t n) {
    char p[1100];
    mkdirs(to);
    if (!is_dir(to) || !writable(to)) {
        snprintf(err, n, "PokeL-ARecomp can't write to %s.", to);
        return false;
    }
    join(p, sizeof p, to, MARK_PARTIAL);
    if (exists(p)) remove_partial(to);
    join(p, sizeof p, to, NOTE_FROM);
    remove(p);  // (a copy that was never switched to: it mustn't empty another folder later)
    return true;
}

bool rt_data_use(const char *to, char *err, size_t n) {
    err[0] = 0;
    if (!prepare_target(to, err, n)) return false;
    if (!point_to(to)) {
        snprintf(err, n, "Couldn't save the data folder's location in %s.", g_home);
        return false;
    }
    fprintf(stderr, "[paths] data folder: %s from the next start\n", to);
    return true;
}

bool rt_data_move(const char *to, void (*progress)(int step, double frac), char *err, size_t n) {
    err[0] = 0;
    char from[1024], p[1100], q[1100], abs_to[1100];
    snprintf(from, sizeof from, "%s", g_root);
    if (!prepare_target(to, err, n)) return false;
    if (!resolve(to, abs_to, sizeof abs_to) || overlap(abs_to, from)) {
        snprintf(err, n, "That folder is inside the data folder.");
        return false;
    }
    if (has_entries(abs_to)) {
        snprintf(err, n, "That folder already has PokeL-ARecomp data.");
        return false;
    }
    Move m = {progress, 0, entries_size(from), 0, err, n, malloc(MOVE_BUF), malloc(MOVE_BUF)};
    join(p, sizeof p, abs_to, MARK_PARTIAL);
    join(q, sizeof q, from, NOTE_TO);
    bool ok = m.buf && m.buf2 && write_line(p, from) && write_line(q, abs_to);
    if (!ok) move_fail(&m, "Couldn't write to", abs_to);
    fprintf(stderr, "[paths] moving the data (%.2f GB) from %s to %s\n", m.total / 1e9, from, abs_to);
    for (int step = 0; step < 2 && ok; step++) {  // copy, then check every file of the copy
        m.step = step, m.done = 0;
        moved(&m, 0);
        for (size_t i = 0; i < N_ENTRIES && ok; i++) {
            char s[1100], d[1100];
            join(s, sizeof s, from, DATA_ENTRIES[i]);
            join(d, sizeof d, abs_to, DATA_ENTRIES[i]);
            if (!exists(s) && !is_link(s)) continue;
            ok = step ? check_tree(&m, s, d) : copy_tree(&m, s, d);
        }
    }
    free(m.buf), free(m.buf2);
    // checked: the copy names its source (emptied once it's in use), stops being unfinished, and becomes the data
    // folder. Until the switch the old folder is the one in use, and its note undoes the copy at the next start.
    if (ok) {
        join(q, sizeof q, abs_to, NOTE_FROM);
        ok = write_line(q, from) && remove(p) == 0;
        if (!ok) move_fail(&m, "Couldn't write to", abs_to);
    }
    if (ok && !point_to(abs_to)) ok = false, snprintf(err, n, "Couldn't save the data folder's location in %s.", g_home);
    if (!ok) {
        fprintf(stderr, "[paths] move failed: %s\n", err);
        remove_partial(abs_to);
        join(q, sizeof q, from, NOTE_TO);
        remove(q);
        return false;
    }
    fprintf(stderr, "[paths] data moved to %s\n", abs_to);
    finish_moves(abs_to, progress);  // (the old copy; interrupted, the next start finishes it)
    return true;
}

const char *rt_exe_dir(void) { return g_exe_dir; }

void rt_paths_restart(void) {
    fprintf(stderr, "[paths] restarting\n");
    fflush(stderr);
    if (chdir(g_start) != 0) fprintf(stderr, "[paths] can't go back to %s\n", g_start);  // (not the old data folder:
    plat_exec(NULL);                                                                     //  dev mode there)
}

// Window / Dock icon: icon.png shipped next to the exe or in the .app (none for a plain source build).
void rt_window_icon(SDL_Window *w) {
    char p[1100];
    if (!rt_resource("icon.png", NULL, p, sizeof p)) return;
    SDL_Surface *s = SDL_LoadPNG(p);
    if (s) SDL_SetWindowIcon(w, s), SDL_DestroySurface(s);
}
