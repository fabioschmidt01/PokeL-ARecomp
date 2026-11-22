// "Report on GitHub" (the crash dialog's button, platform/windows.c and linux.c plat_fatal_notice): everything a bug
// report needs, gathered in one click - the player's settings (larecomp.ini), this session's log and the game's last
// two play logs, and a system.txt with the PC (version, OS, CPU, RAM, GPU and driver, the video profile: the log's
// own first lines) - copied into logs/report-<date>/, that folder shown, and a new GitHub issue opened in the browser
// with the title, the PC, what happened and the crash lines already filled in. GitHub can't take files through a link,
// so the issue text asks for the folder's files to be dragged in; nothing is sent anywhere by itself.
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include <SDL3/SDL.h>

#define REPORT_REPO "https://github.com/fabioschmidt01/PokeL-ARecomp"

static bool copy_file(const char *from, const char *to) {
    FILE *in = fopen(from, "rb");
    if (!in) return false;
    FILE *out = fopen(to, "wb");
    if (!out) { fclose(in); return false; }
    char buf[65536];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, k, out);
    fclose(in);
    fclose(out);
    return true;
}

// the whole file (at most max bytes from its end when tail, else from its start), NUL-terminated; NULL if unreadable
static char *read_part(const char *path, size_t max, bool tail) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    size_t n = (size_t)size < max ? (size_t)size : max;
    fseek(f, tail ? size - (long)n : 0, SEEK_SET);
    char *b = malloc(n + 1);
    if (b) b[fread(b, 1, n, f)] = 0;
    fclose(f);
    return b;
}

static void append(char *dst, size_t cap, const char *s, size_t len) {
    size_t have = strlen(dst);
    if (have + 1 >= cap) return;
    if (len > cap - have - 1) len = cap - have - 1;
    memcpy(dst + have, s, len);
    dst[have + len] = 0;
}

// The PC, from the log's first lines (main.c log_system and the Vulkan device setup write them): one line each
static void system_lines(const char *log_head, char *out, size_t cap) {
    static const char *const keys[] = {"[system]", "[vk] using", "[vk] gpu ", "[settings] recommended", "[settings] loaded",
                                       "[game]", "[pcore] hybrid", "[audout] opened"};
    out[0] = 0;
    for (const char *p = log_head; p && *p;) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        for (size_t k = 0; k < sizeof keys / sizeof *keys; k++)
            if (!strncmp(p, keys[k], strlen(keys[k]))) { append(out, cap, p, len); append(out, cap, "\n", 1); break; }
        p = e ? e + 1 : NULL;
    }
}

// The crash itself: from the log's last "*** exception" / "FATAL" / "[crash]" line, up to 24 lines
static void crash_lines(const char *log_tail, char *out, size_t cap) {
    out[0] = 0;
    if (!log_tail) return;
    const char *best = NULL;
    static const char *const marks[] = {"*** exception", "FATAL", "[crash]", "[vk] device lost", "abort"};
    for (size_t k = 0; k < sizeof marks / sizeof *marks; k++)
        for (const char *p = log_tail; (p = strstr(p, marks[k])); p++) {
            const char *line = p;
            while (line > log_tail && line[-1] != '\n') line--;
            if (!best || line > best) best = line;
            if (k == 0) break;  // (the first exception is the cause; later lines are the dialog's)
        }
    if (!best) return;
    const char *p = best;
    for (int i = 0; i < 24 && *p; i++) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        append(out, cap, p, len < 200 ? len : 200);
        append(out, cap, "\n", 1);
        if (!e) break;
        p = e + 1;
    }
}

static void url_encode(const char *s, char *out, size_t cap) {
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && o + 4 < cap; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || strchr("-_.~", *p))
            out[o++] = (char)*p;
        else out[o++] = '%', out[o++] = hex[*p >> 4], out[o++] = hex[*p & 15];
    }
    out[o] = 0;
}

// Gathers the report and opens the folder and the issue page. head: what happened, in plain words (crash_explain.c's
// first line); log_path: this session's log. Returns false when nothing could be gathered.
bool rt_crash_report(const char *head, const char *log_path) {
    char when[32], dir[256], path[512];
    time_t now = time(NULL);
    strftime(when, sizeof when, "%Y%m%d-%H%M%S", localtime(&now));
    SDL_CreateDirectory("logs");
    snprintf(dir, sizeof dir, "logs/report-%s", when);
    if (!SDL_CreateDirectory(dir)) return false;
    snprintf(path, sizeof path, "%s/larecomp.ini", dir);
    copy_file("larecomp.ini", path);
    if (log_path && *log_path) {
        const char *name = strrchr(log_path, '/'), *bs = strrchr(log_path, '\\');
        if (bs && (!name || bs > name)) name = bs;
        snprintf(path, sizeof path, "%s/%s", dir, name ? name + 1 : log_path);
        copy_file(log_path, path);
    }
    snprintf(path, sizeof path, "%s/play.log", dir);
    copy_file("logs/play.log", path);
    snprintf(path, sizeof path, "%s/play-anterior.log", dir);
    copy_file("logs/play-anterior.log", path);

    char *log_head = log_path ? read_part(log_path, 64 << 10, false) : NULL;
    char *log_tail = log_path ? read_part(log_path, 256 << 10, true) : NULL;
    static char sys[4096], crash[6144], body[12288], enc[32768], url[34000];
    system_lines(log_head, sys, sizeof sys);
    crash_lines(log_tail, crash, sizeof crash);
    free(log_head);
    free(log_tail);
    snprintf(path, sizeof path, "%s/system.txt", dir);
    FILE *f = fopen(path, "w");
    if (f) { fputs(sys, f); fclose(f); }

    char abs_dir[1100];
    const char *cwd = SDL_GetCurrentDirectory();
    snprintf(abs_dir, sizeof abs_dir, "%s%s", cwd ? cwd : "", dir);
    SDL_free((void *)cwd);
#ifdef _WIN32
    for (char *c = abs_dir; *c; c++) if (*c == '/') *c = '\\';
#endif
    snprintf(body, sizeof body,
             "**What happened:** %s\n\n"
             "**Where were you / what were you doing?** (area, battle, menu, cutscene...)\n\n\n"
             "**Files:** please drag the files from this folder into this issue (larecomp.ini, the logs, system.txt):\n`%s`\n\n"
             "### PC\n```\n%s```\n### Crash\n```\n%s```\n",
             head && *head ? head : "The game crashed", abs_dir, sys[0] ? sys : "(not in the log)\n",
             crash[0] ? crash : "(no crash lines in the log)\n");
    // (a browser takes ~8 KB of link safely: the crash lines shortened first)
    for (;;) {
        url_encode(body, enc, sizeof enc);
        if (strlen(enc) < 7000 || !crash[0]) break;
        size_t l = strlen(crash);
        char *cut = l > 400 ? crash + l - 400 : crash;
        while (cut > crash && *cut != '\n') cut--;
        *cut = 0;
        if (!*crash) strcpy(crash, "(see the attached log)\n");
        snprintf(body, sizeof body,
                 "**What happened:** %s\n\n**Where were you / what were you doing?**\n\n\n**Files:** please drag the files from "
                 "this folder into this issue:\n`%s`\n\n### PC\n```\n%s```\n### Crash\n```\n%s```\n",
                 head && *head ? head : "The game crashed", abs_dir, sys, crash);
    }
    char title[256], tenc[768];
    snprintf(title, sizeof title, "Crash: %s", head && *head ? head : "the game crashed");
    url_encode(title, tenc, sizeof tenc);
    snprintf(url, sizeof url, REPORT_REPO "/issues/new?labels=crash&title=%s&body=%s", tenc, enc);

    char folder_url[1200];
#ifdef _WIN32
    snprintf(folder_url, sizeof folder_url, "file:///%s", abs_dir);
#else
    snprintf(folder_url, sizeof folder_url, "file://%s", abs_dir);
#endif
    SDL_OpenURL(folder_url);
    SDL_OpenURL(url);
    fprintf(stderr, "[report] gathered in %s and the issue page opened\n", abs_dir);
    return true;
}

// The crash dialog itself (both platforms' plat_fatal_notice): the explanation (crash_explain.c) with three buttons -
// report it (above), show the logs folder, or close. In the start screen's language.
extern bool settings_ui_portuguese(void);
void rt_crash_dialog(const char *text, const char *log_path) {
    bool pt = settings_ui_portuguese();
    char head[200] = "";
    const char *nl = strchr(text, '\n');
    snprintf(head, sizeof head, "%.*s", nl ? (int)(nl - text) : (int)strlen(text), text);
    const SDL_MessageBoxButtonData b[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, pt ? "Reportar no GitHub" : "Report on GitHub"},
        {0, 2, pt ? "Abrir a pasta de logs" : "Open the logs folder"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, pt ? "Fechar" : "Close"}};
    char msg[2200];
    snprintf(msg, sizeof msg, "%s\n\n%s", text,
             pt ? "\"Reportar no GitHub\" junta o seu larecomp.ini, os logs e os dados do PC numa pasta e abre uma issue "
                  "já preenchida: é só arrastar os arquivos da pasta para ela."
                : "\"Report on GitHub\" puts your larecomp.ini, the logs and your PC's details in one folder and opens an "
                  "issue already filled in: just drag the folder's files into it.");
    const SDL_MessageBoxData box = {SDL_MESSAGEBOX_ERROR, NULL, "PokeL-ARecomp", msg, 3, b, NULL};
    int k = 0;
    if (!SDL_ShowMessageBox(&box, &k)) { fprintf(stderr, "%s\n", text); return; }
    if (k == 1) rt_crash_report(head, log_path);
    if (k == 2) {
        const char *cwd = SDL_GetCurrentDirectory();
        char u[1200];
#ifdef _WIN32
        snprintf(u, sizeof u, "file:///%slogs", cwd ? cwd : "");
#else
        snprintf(u, sizeof u, "file://%slogs", cwd ? cwd : "");
#endif
        SDL_free((void *)cwd);
        SDL_OpenURL(u);
    }
}
