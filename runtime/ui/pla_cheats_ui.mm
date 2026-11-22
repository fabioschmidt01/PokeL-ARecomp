// F1 > Cheats (Pokémon Legends: Arceus): the cheats found in the cheats folder (runtime/pla_cheats.c), one toggle each.
// Runtime cheats work at once. Code cheats change the game's code: they are built in by recompiling the game, which
// the start screen does by itself at the next start (the library's id names the set of code cheats).
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "imgui.h"
#include "option_rows.h"

extern "C" {
#include "../settings.h"
int pla_cheats_count(void);
const char *pla_cheats_name(int i);
bool pla_cheats_is_code(int i);
bool pla_cheats_is_master(int i);
bool pla_cheats_is_bad(int i);
bool pla_cheats_on(int i);
void pla_cheats_set(int i, bool on);
void pla_cheats_load(void);
bool pla_cheats_code_active(void);
void pla_cheats_id_suffix(char *out, size_t n);
}

static bool has_nocase(const char *s, const char *w) {  // (no strcasestr on Windows)
    for (; *s; s++) {
        size_t i = 0;
        while (w[i] && s[i] && (s[i] | 32) == (w[i] | 32)) i++;
        if (!w[i]) return true;
    }
    return false;
}

extern "C" void pla_cheats_draw(float scale) {
    pla_cheats_load();
    static bool started;  // (the code cheats this game was built with: compared below)
    static char start_suffix[32];
    if (!started) { pla_cheats_id_suffix(start_suffix, sizeof start_suffix); started = true; }
    opt_begin("##placheats", scale);
    opt_section(UI_TR("Experimental", "Experimental"));
    opt_button(UI_TR("About cheats", "Sobre os cheats"),
               UI_TR("Cheats are experimental (new in 0.27). A cheat can break your save or crash the game: back up your "
                     "save (save/backups keeps the last 10) and turn cheats off before reporting a bug.",
                     "Os cheats são experimentais (novos na 0.27). Um cheat pode estragar o save ou travar o jogo: faça backup "
                     "do save (save/backups guarda os 10 últimos) e desligue os cheats antes de reportar um bug."));
    int n = pla_cheats_count();
    if (!n) {
        opt_section(UI_TR("No cheats found", "Nenhum cheat encontrado"));
        opt_button(UI_TR("How to add cheats", "Como adicionar cheats"),
                   UI_TR("Put an Atmosphere cheat file for version 1.1.1 (AEE8F150DDA1B5A8.txt), or the whole atmosphere "
                         "folder, inside the \"cheats\" folder next to PokeL-ARecomp.exe, then open this tab again after "
                         "restarting the game.",
                         "Coloque um arquivo de cheats do Atmosphere da versão 1.1.1 (AEE8F150DDA1B5A8.txt), ou a pasta "
                         "atmosphere inteira, dentro da pasta \"cheats\" ao lado do PokeL-ARecomp.exe, e abra esta aba de "
                         "novo depois de reiniciar o jogo."));
        opt_end();
        return;
    }
    char now_suffix[32];
    pla_cheats_id_suffix(now_suffix, sizeof now_suffix);
    if (strcmp(now_suffix, start_suffix)) {
        opt_section(UI_TR("Restart needed", "Reinício necessário"));
        opt_button(UI_TR("Code cheats changed", "Cheats de código alterados"),
                   UI_TR("Some cheats you turned on or off change the game's code. Close the game and start it again: the "
                         "start screen recompiles the game with them (it takes a while, as the first build did).",
                         "Alguns cheats que você ligou ou desligou mudam o código do jogo. Feche o jogo e abra de novo: a tela "
                         "inicial recompila o jogo com eles (demora, como a primeira compilação)."));
    }
    for (int pass = 0; pass < 2; pass++) {  // runtime cheats first (on at once), then the ones built into the code
        opt_section(pass == 0 ? UI_TR("Cheats (work at once)", "Cheats (funcionam na hora)")
                              : UI_TR("Code cheats (recompile the game)", "Cheats de código (recompilam o jogo)"));
        for (int i = 0; i < n; i++) {
            if (pla_cheats_is_code(i) != (pass == 1) || pla_cheats_is_master(i)) continue;
            {   // ("X (off)" / "X Off" / "X - OFF": Atmosphere's way to undo X, writing the original values back. Here
                // turning X off undoes it - the code cheats are rebuilt without it - so these aren't listed)
                const char *nm = pla_cheats_name(i);
                size_t l = strlen(nm);
                bool off = has_nocase(nm, "(off)") || has_nocase(nm, "- off") || (l > 4 && has_nocase(nm + l - 4, " off"));
                if (off && !pla_cheats_on(i)) continue;
            }
            char label[128];
            snprintf(label, sizeof label, "%s##c%d", pla_cheats_name(i), i);
            bool on = pla_cheats_on(i), bad = pla_cheats_is_bad(i);
            if (bad) {
                opt_button(label, UI_TR("This cheat uses instructions PokeL-ARecomp's cheat engine doesn't run yet.",
                                        "Este cheat usa instruções que o motor de cheats do PokeL-ARecomp ainda não roda."));
                continue;
            }
            const char *desc = pass == 0
                ? UI_TR("Turns on or off at once. Its master code (if the file has one) turns on with it.",
                        "Liga ou desliga na hora. O master code do arquivo (se houver) liga junto.")
                : UI_TR("Changes the game's code: after turning it on or off, restart the game and the start screen "
                        "recompiles it with this cheat.",
                        "Muda o código do jogo: depois de ligar ou desligar, reinicie o jogo e a tela inicial recompila "
                        "com este cheat.");
            if (opt_toggle(label, &on, false, desc)) pla_cheats_set(i, on);
        }
    }
    opt_end();
}
