// Game files: whose they are, deleting them, setting them up as one transaction, compiling them (runtime/paths.c
// rt_game_files_owner / rt_remove_game_file(s) / rt_game_stage_*, runtime/gamebuild.c gamebuild_run). The real
// functions, driven by tools/test_game_files.py on scratch folders with synthetic NSO headers; one mode per run, in
// the data root LARECOMP_DATA_ROOT names (rt_paths_init: the same start as the app's, staging recovery included).
//   owner <dir>            whose files dir holds: "owner=<ours|foreign|unknown|none> <what>"
//   remove                 rt_remove_game_files("extracted"): "removed=<0|1>"
//   remove1 <name>         rt_remove_game_file("extracted", name): "removed=<0|1>"
//   setup <folder>         the start screen's copy setup: stage_begin, copy folder's files into the staging folder,
//                          stage_commit (setup-fail: stage_abort instead, as a failed import; setup-crash: exit while
//                          staged, as a crash or power loss; setup-hold <secs>: stay staged that long - another copy
//                          of the app). Prints "begin=<0|1> commit=<0|1> <error>"
//   init                   only the start (a restart: rt_game_stage_recover)
//   build <dir>            gamebuild_run(dir): "built=<0|1> <why>"
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gamebuild.h"
#include "paths.h"
#include "platform.h"

// (the platform file's crash handling, never reached here; gamelib.c's CPU check)
void rt_crash_explain(char *out, size_t n, const char *log_path) { (void)log_path; if (n) out[0] = 0; }
bool rt_fault_watch(uint64_t a, uint64_t pc) { (void)a, (void)pc; return false; }
uint64_t g_fault_lr;
void rt_fault_report(const char *what, uint64_t a, uint64_t pc, uint64_t lr) {
    (void)a, (void)pc, (void)lr;
    fprintf(stderr, "fault: %s\n", what);
    abort();
}
const char *rt_host_fn_name(uint64_t pc, uint64_t *offset) { (void)pc, (void)offset; return NULL; }
bool gamelib_old_cpu(void) { return false; }

// A plain recursive copy (the start screen uses romfs_copy_tree: clones / hard links where it can - the same files)
static bool copy_tree(const char *s, const char *d, int *budget) {
    struct stat st;
    if (lstat(s, &st) != 0) return false;
    if (S_ISDIR(st.st_mode)) {
        if (mkdir(d, 0755) != 0) return false;
        DIR *dir = opendir(s);
        if (!dir) return false;
        struct dirent *e;
        bool ok = true;
        while (ok && (e = readdir(dir))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char cs[2048], cd[2048];
            snprintf(cs, sizeof cs, "%s/%s", s, e->d_name);
            snprintf(cd, sizeof cd, "%s/%s", d, e->d_name);
            ok = copy_tree(cs, cd, budget);
        }
        closedir(dir);
        return ok;
    }
    if (budget && (*budget)-- <= 0) return true;  // (setup-crash: stop part way, files missing)
    FILE *in = fopen(s, "rb"), *out = in ? fopen(d, "wb") : NULL;
    char buf[65536];
    size_t k;
    bool ok = in && out;
    while (ok && (k = fread(buf, 1, sizeof buf, in)) > 0) ok = fwrite(buf, 1, k, out) == k;
    if (in) fclose(in);
    if (out) ok &= fclose(out) == 0;
    return ok;
}

static const char *owner_name(RtGameOwner o) {
    return o == RT_GAME_OURS ? "ours" : o == RT_GAME_FOREIGN ? "foreign" : o == RT_GAME_UNKNOWN ? "unknown" : "none";
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: see the top of tools/test_game_files.c\n"); return 2; }
    const char *mode = argv[1];
    rt_paths_init();
    if (!strcmp(mode, "init")) { printf("%s\n", rt_data_root()); return 0; }
    if (!strcmp(mode, "owner") && argc == 3) {
        char what[160];
        RtGameOwner o = rt_game_files_owner(argv[2], what, sizeof what);
        printf("owner=%s %s\n", owner_name(o), what);
        return 0;
    }
    if (!strcmp(mode, "remove")) { printf("removed=%d\n", rt_remove_game_files("extracted")); return 0; }
    if (!strcmp(mode, "remove1") && argc == 3) { printf("removed=%d\n", rt_remove_game_file("extracted", argv[2])); return 0; }
    if (!strncmp(mode, "setup", 5) && argc >= 3) {
        char stage[1100], err[600], d[1200];
        if (!rt_game_stage_begin("extracted", stage, sizeof stage, err, sizeof err)) {
            printf("begin=0 commit=0 %s\n", err);
            return 0;
        }
        fflush(stdout);
        int budget = 1;  // (setup-crash: one file copied, then the process dies)
        DIR *dir = opendir(argv[2]);
        struct dirent *e;
        bool ok = dir != NULL;
        while (ok && (e = readdir(dir))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char s[1200];
            snprintf(s, sizeof s, "%s/%s", argv[2], e->d_name);
            snprintf(d, sizeof d, "%s/%s", stage, e->d_name);
            ok = copy_tree(s, d, !strcmp(mode, "setup-crash") ? &budget : NULL);
        }
        if (dir) closedir(dir);
        if (!strcmp(mode, "setup-crash")) { printf("begin=1 crashed staged\n"); fflush(stdout); _exit(9); }
        if (!strcmp(mode, "setup-hold")) {
            printf("begin=1 holding %s\n", stage);
            fflush(stdout);
            sleep(argc > 3 ? (unsigned)atoi(argv[3]) : 5);
            rt_game_stage_abort();
            return 0;
        }
        if (!ok || !strcmp(mode, "setup-fail")) {
            rt_game_stage_abort();
            printf("begin=1 commit=0 %s\n", ok ? "aborted" : "copy failed");
            return 0;
        }
        bool c = rt_game_stage_commit("extracted", stage, err, sizeof err);
        printf("begin=1 commit=%d %s\n", c, err);
        return 0;
    }
    if (!strcmp(mode, "build") && argc == 3) {
        char why[512] = "";
        GameBuildOpts o = {NULL, 1, NULL};
        bool ok = gamebuild_run(argv[2], &o, why, sizeof why);
        printf("built=%d %s\n", ok, why);
        return 0;
    }
    return 2;
}
