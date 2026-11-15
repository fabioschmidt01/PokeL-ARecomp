// Data folder isolation (runtime/paths.c): prints the data root the real rt_data_root_early / rt_paths_init pick, for
// tools/test_data_dir.py, which runs it under scratch HOME / XDG_DATA_HOME values and checks that it is PokeL-ARecomp's
// own folder and that the other recomps' folders next to it (pokev-recomp/, zarecomp/, lasrecomp/) are never touched.
//   test_data_dir early   -> rt_data_root_early's answer ("none": no session log file - dev mode, or refused)
//   test_data_dir init    -> rt_paths_init (exits 1 with a message when it refuses), then rt_data_root() and the cwd
//   test_data_dir remove  -> rt_paths_init, then rt_game_files_owner / rt_remove_game_files on <data root>/extracted
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "paths.h"
#include "platform.h"

// (the platform file's crash handling, never reached here)
void rt_crash_explain(char *out, size_t n, const char *log_path) { (void)log_path; if (n) out[0] = 0; }
bool rt_fault_watch(uint64_t a, uint64_t pc) { (void)a, (void)pc; return false; }
uint64_t g_fault_lr;
void rt_fault_report(const char *what, uint64_t a, uint64_t pc, uint64_t lr) {
    (void)a, (void)pc, (void)lr;
    fprintf(stderr, "fault: %s\n", what);
    abort();
}
const char *rt_host_fn_name(uint64_t pc, uint64_t *offset) { (void)pc, (void)offset; return NULL; }

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s early|init|remove\n", argv[0]); return 2; }
    if (!strcmp(argv[1], "early")) {
        char root[1100];
        if (!rt_data_root_early(root, sizeof root)) { printf("none\n"); return 0; }  // (dev mode, or refused)
        printf("%s\n", root);
        return 0;
    }
    rt_paths_init();
    if (!strcmp(argv[1], "init")) {
        char cwd[1100];
        if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
        printf("%s\n%s\n", rt_data_root(), cwd);
        return 0;
    }
    if (!strcmp(argv[1], "remove")) {
        char what[160] = "";
        RtGameOwner o = rt_game_files_owner("extracted", what, sizeof what);
        bool removed = rt_remove_game_files("extracted");
        printf("%s\nowner=%d %s\nremoved=%d\n", rt_data_root(), (int)o, what, removed);
        return 0;
    }
    return 2;
}
