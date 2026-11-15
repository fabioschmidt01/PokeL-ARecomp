// Where PokeL-ARecomp keeps its files (runtime/paths.c).
//
// Data root: everything the app writes or the user brings - extracted/ (game files), rando/ (seeds), save/,
// cache/ (shaders / pipelines), fonts/ (imported), larecomp.ini. rt_paths_init() picks it and makes it the
// working directory, so the rest of the runtime opens those by their relative names. In order:
//   1. LARECOMP_DATA_ROOT (tests, scratch installs)
//   2. dev / portable mode: the starting directory already has extracted/ (a source checkout: `./build/lasrecomp
//      extracted`, the test scripts) - used as is
//   3. the folder named in data_root.txt in the platform's folder (4), chosen on the start screen (never inside the
//      app); asks to try again / use the default when it's missing (a drive not connected)
//   4. macOS: ~/Library/Application Support/com.peiikai.lasrecomp (the .app bundle itself is never written to)
//      Windows: the folder of the .exe (portable, no installer); refuses to start, with a message, if not writable
//      Linux: $XDG_DATA_HOME/plarecomp (~/.local/share/plarecomp when XDG_DATA_HOME is unset or relative;
//      RT_LINUX_DATA_DIR): PokeL-ARecomp's own - not LAS Recomp's lasrecomp/, ZA Recomp's zarecomp/ or PokeV-Recomp's
//      pokev-recomp/ (their saves, larecomp.ini and extracted/ would be shared)
//      LARECOMP_DATA_HOME (tests) stands in for it.
//
// Resources: files that ship with the app (helper programs, the window icon) - next to the executable (the
// Windows folder, the .app's Contents/MacOS), in the .app's Contents/Resources, or in the source tree.
#pragma once
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RT_BUNDLE_ID "com.peiikai.lasrecomp"
#define RT_LINUX_DATA_DIR "plarecomp"  // (under $XDG_DATA_HOME, else ~/.local/share; every release so far used it)

void rt_paths_init(void);         // first thing in main(), after plat_init
const char *rt_data_root(void);   // absolute
const char *rt_exe_dir(void);     // the program's folder (the start screen's updater replaces its files)
bool rt_dev_mode(void);           // dev / portable mode (rule 2 above): e.g. a source checkout
// Before rt_paths_init (the session log): the data root by the rules above; false in dev mode
bool rt_data_root_early(char *out, size_t n);

// Another app's data folder (LAS Recomp's, ZA Recomp's, PokeV-Recomp's): its folder name (lasrecomp, zarecomp,
// pokev-recomp, or the move's "LAS Recomp" / "ZA Recomp" / "PokeV-Recomp" folders) anywhere in its path, another
// game's build in its extracted/, or its settings / log naming LAS Recomp or PokeV-Recomp. Never used as the data root:
// rt_paths_init refuses it before anything is written. why: for messages. (ZA Recomp's settings header can't tell it
// apart: PokeL-ARecomp's own za-default.ini and v0.0.21 and older wrote "# ZA Recomp settings" too.)
bool rt_data_root_foreign(const char *root, char *why, size_t n);

// Choosing another data folder (the start screen). NULL = it can be changed; else why not (rules 1 / 2).
const char *rt_data_root_fixed(void);
typedef struct {
    char path[1100];               // where the data would go: the folder picked, or a "PokeL-ARecomp" folder in it
                                   // when it holds other things
    unsigned long long bytes;      // the data now (saves, seeds, settings, fonts, caches, game files)
    unsigned long long need, free; // space a move needs there (little on the same drive: hard links), space there
    bool has_data;                 // PokeL-ARecomp data already there: used as is, not moved into
    char refuse[160];              // why it can't be the data folder ("" = it can)
} RtDataTarget;
void rt_data_target(const char *picked, RtDataTarget *t);
// The data (rt_data_target's) copied there, every file checked against the original, the switch made (from the next
// start), then the old copy removed. Nothing is removed before the switch: interrupted, the next start uses the old
// folder and removes the unfinished copy (or, after the switch, finishes removing the old one). progress: step 0
// copying, 1 checking, 2 removing the old copy. false: err says why; the data stays where it was.
bool rt_data_move(const char *to, void (*progress)(int step, double frac), char *err, size_t n);
// Only the switch: from the next start the data folder is `to` (created if needed), as it is. The old one is kept.
bool rt_data_use(const char *to, char *err, size_t n);
// Start again (the same command line, from the starting folder): after a switch. Returns only on failure.
void rt_paths_restart(void);

// A shipped file: `name` next to the executable or in Contents/Resources, else `dev_rel` in the source tree (the
// starting directory or the executable's parent; NULL = shipped builds only). Absolute path in out; false = none.
bool rt_resource(const char *name, const char *dev_rel, char *out, size_t n);

// Whose game files a folder (an extracted/, or the setup's staging folder) holds, from their contents only: every
// NSO "main" in the game-file entries (exefs/main, and leftover nso_raw/, update_pfs/... folders; not romfs/ or the
// build caches) is read (runtime/game_identity.h). No marker file or folder name makes files ours.
typedef enum {
    RT_GAME_NONE,     // no game-file entries there (or no such folder)
    RT_GAME_OURS,     // only Pokémon Legends: Arceus 1.1.1: this app's to set up over, compile and delete
    RT_GAME_FOREIGN,  // a build of another game the related recomps run (Violet, Z-A, Link's Awakening)
    RT_GAME_UNKNOWN,  // game files that can't be identified: an unknown build, an unreadable or linked exefs/main, or no
                      // exefs/main at all. Never compiled, never deleted automatically.
} RtGameOwner;
// what: for messages, e.g. "Pokémon Violet 4.0.0" or "an unknown build (0123456789abcdef)"
RtGameOwner rt_game_files_owner(const char *game_dir, char *what, size_t n);

// The only ways the app deletes game files. game_dir must be <data root>/extracted itself (symlinks resolved; a
// symlinked extracted/ is refused) and hold RT_GAME_OURS or RT_GAME_NONE files; an entry is removed only if it is
// one the setup creates (exefs, romfs, leftover folders - romfs.bin from older versions) and not a link. The folder
// the user picked never gets here. true = removed or not there.
bool rt_remove_game_file(const char *game_dir, const char *name);
// All the game files set up there (exefs last, so a cut-off delete still reads as ours), then the folder if empty.
// Saves, seeds, settings stay.
bool rt_remove_game_files(const char *game_dir);

// Setting the game files up, as one transaction (runtime/ui/launcher.mm): files are copied or imported into a staging
// folder only this app writes (<data root>/.plarecomp-import), checked there, and only then replace extracted/.
//  - begin: game_dir must be <data root>/extracted (or not exist yet) and hold RT_GAME_OURS / RT_GAME_NONE files;
//    takes the setup lock (one setup at a time, also across processes), clears an earlier setup's leftover staging
//    folder and makes a fresh, empty one (its path in stage).
//  - commit: the staged files must be RT_GAME_OURS with their game data (romfs.bin or romfs/); extracted/ is checked
//    again, its own files removed, and the staging folder renamed into its place. Releases the lock.
//  - abort: the staging folder removed (only it: never extracted/), the lock released.
// false: err says why (for the player); nothing in extracted/ was changed unless commit got as far as replacing it.
bool rt_game_stage_begin(const char *game_dir, char *stage, size_t n, char *err, size_t errn);
bool rt_game_stage_commit(const char *game_dir, const char *stage, char *err, size_t errn);
void rt_game_stage_abort(void);
// A setup cut off (crash, power loss, killed) leaves its staging folder: removed at the next start (rt_paths_init),
// unless another running copy of the app holds the setup lock. Only that folder: extracted/ is never touched.
void rt_game_stage_recover(void);
// Bytes taken by the extracted game files in game_dir (links not followed).
unsigned long long rt_game_files_size(const char *game_dir);
// Bytes taken by everything under path (links not followed).
unsigned long long rt_tree_size(const char *path);

typedef struct SDL_Window SDL_Window;
void rt_window_icon(SDL_Window *w);  // the shipped icon.png, if any

#ifdef __cplusplus
}
#endif
