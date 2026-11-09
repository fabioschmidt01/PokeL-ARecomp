#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <wctype.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <cpuid.h>
#endif

#include "perfmon.h"
#include "settings.h"

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv = PTHREAD_COND_INITIALIZER;
static PerfStats g_stats;
static int g_mode = 0;   // 0 off, 1 fps, 2 full

// ---- Frametime history & lows
static float g_ring[PERF_HISTORY];
static uint64_t g_ring_at[PERF_HISTORY];
static int g_ring_n, g_ring_pos;

#define LOW_BINS 1024
#define LOW_SLICES 10
static uint16_t g_low_hist[LOW_SLICES][LOW_BINS];
static int g_low_cur;
static uint64_t g_low_t0;

// ---- Per-frame engine counter window (0.5s average, matching TotK Recomp)
static struct {
    uint64_t t0, frames;
    double replay, gpu, pipe, xlate, tex_up;
} g_win;

static char g_gpu_name_cfg[96];
static uint8_t g_gpu_luid_cfg[8];
static int g_gpu_luid_ok;
static float g_vram_total_cfg = -1;
static float g_vram_used_vk = -1;

static uint64_t now_ns(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER cnt;
    QueryPerformanceCounter(&cnt);
    return (uint64_t)((double)cnt.QuadPart * 1e9 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

void perfmon_set_gpu(const char *name, const uint8_t *luid, float vram_total_mb) {
    pthread_mutex_lock(&g_lock);
    if (name) snprintf(g_gpu_name_cfg, sizeof g_gpu_name_cfg, "%s", name);
    g_gpu_luid_ok = (luid != NULL);
    if (luid) memcpy(g_gpu_luid_cfg, luid, 8);
    g_vram_total_cfg = vram_total_mb > 0 ? vram_total_mb : -1;
    pthread_mutex_unlock(&g_lock);
}

void perfmon_set_vram_used(float vram_used_mb) {
    pthread_mutex_lock(&g_lock);
    g_vram_used_vk = vram_used_mb > 0 ? vram_used_mb : -1;
    if (g_vram_used_vk > 0 && g_stats.vram_used_mb <= 0)
        g_stats.vram_used_mb = g_vram_used_vk;
    pthread_mutex_unlock(&g_lock);
}

void perfmon_set_picture(uint32_t win_w, uint32_t win_h, uint32_t img_w, uint32_t img_h,
                         uint32_t scene_w, uint32_t scene_h, int scaler) {
    pthread_mutex_lock(&g_lock);
    g_stats.win_w = win_w;
    g_stats.win_h = win_h;
    g_stats.image_w = img_w;
    g_stats.image_h = img_h;
    g_stats.scene_w = scene_w;
    g_stats.scene_h = scene_h;
    g_stats.scaler = scaler;
    pthread_mutex_unlock(&g_lock);
}

void perfmon_frame_ex(uint64_t interval_ns, double replay_ms, double gpu_ms,
                      double pipe_ms, double xlate_ms, double tex_uploads) {
    if (!g_mode && !g_settings.show_fps) return;
    uint64_t now = now_ns();
    pthread_mutex_lock(&g_lock);
    if (interval_ns > 0) {
        float ms = (float)(interval_ns / 1e6);
        g_ring[g_ring_pos] = ms;
        g_ring_at[g_ring_pos] = now;
        g_ring_pos = (g_ring_pos + 1) % PERF_HISTORY;
        if (g_ring_n < PERF_HISTORY) g_ring_n++;

        if (now - g_low_t0 >= 1000000000ull) {
            g_low_cur = (g_low_cur + 1) % LOW_SLICES;
            memset(g_low_hist[g_low_cur], 0, sizeof g_low_hist[0]);
            g_low_t0 = now;
        }
        uint64_t bin = interval_ns / 250000ull;
        uint16_t *h = &g_low_hist[g_low_cur][bin < LOW_BINS ? bin : LOW_BINS - 1];
        if (*h < 65535) (*h)++;
    }
    if (!g_win.t0) g_win.t0 = now;
    g_win.frames++;
    if (replay_ms >= 0) g_win.replay += replay_ms;
    if (gpu_ms >= 0) g_win.gpu += gpu_ms;
    if (pipe_ms >= 0) g_win.pipe += pipe_ms;
    if (xlate_ms >= 0) g_win.xlate += xlate_ms;
    if (tex_uploads >= 0) g_win.tex_up += tex_uploads;
    pthread_mutex_unlock(&g_lock);
}

void perfmon_frame(uint64_t interval_ns) {
    perfmon_frame_ex(interval_ns, -1, -1, -1, -1, -1);
}

static void frames_summary(PerfStats *s, uint64_t now) {
    double sum = 0, mx = 0;
    int n = 0;
    for (int i = 0; i < g_ring_n; i++) {
        int k = (g_ring_pos - 1 - i + PERF_HISTORY) % PERF_HISTORY;
        if (now - g_ring_at[k] > 1000000000ull) break;  // last 1 second
        sum += g_ring[k];
        if (g_ring[k] > mx) mx = g_ring[k];
        n++;
    }
    s->fps = n && sum > 0 ? (float)(n * 1000.0 / sum) : 0;
    s->frame_ms_avg = n ? (float)(sum / n) : 0;
    s->frame_ms_max = (float)mx;
    for (int i = 0; i < PERF_HISTORY; i++) {
        int age = PERF_HISTORY - 1 - i;
        s->frame_ms[i] = age < g_ring_n ? g_ring[(g_ring_pos - 1 - age + 2 * PERF_HISTORY) % PERF_HISTORY] : 0;
    }
}

static void lows_summary(PerfStats *s) {
    uint32_t total = 0;
    for (int k = 0; k < LOW_SLICES; k++)
        for (int b = 0; b < LOW_BINS; b++) total += g_low_hist[k][b];
    s->low1_fps = s->low01_fps = -1;
    if (total < 60) return;
    uint32_t cut1 = (total + 99) / 100, cut01 = (total + 999) / 1000, acc = 0;
    for (int b = LOW_BINS - 1; b >= 0 && s->low1_fps < 0; b--) {
        for (int k = 0; k < LOW_SLICES; k++) acc += g_low_hist[k][b];
        float fps = 1000.0f / ((b + 1) * 0.25f);
        if (s->low01_fps < 0 && acc >= cut01) s->low01_fps = fps;
        if (acc >= cut1) s->low1_fps = fps;
    }
}

static void counters_summary(PerfStats *s, uint64_t now) {
    if (now - g_win.t0 < 500000000ull) return;
    lows_summary(s);
    if (g_win.t0 && g_win.frames) {
        double n = (double)g_win.frames;
        s->replay_ms = (float)(g_win.replay / n);
        s->gpu_ms = g_win.gpu > 0 ? (float)(g_win.gpu / n) : -1;
        s->pipe_wait_ms = (float)(g_win.pipe / n);
        s->xlate_ms = (float)(g_win.xlate / n);
        s->tex_uploads = (float)(g_win.tex_up / n);
    }
    g_win.t0 = now;
    g_win.frames = 0;
    g_win.replay = g_win.gpu = g_win.pipe = g_win.xlate = g_win.tex_up = 0;
}

#ifdef _WIN32
// ---- NVML (NVIDIA Dynamic Loader)
typedef struct { unsigned gpu, memory; } NvmlUtil;
typedef struct { unsigned long long total, free, used; } NvmlMem;
static struct {
    int tried, ok;
    HMODULE lib;
    void *dev;
    int (*util)(void *, NvmlUtil *);
    int (*temp)(void *, int, unsigned *);
    int (*mem)(void *, NvmlMem *);
    int (*power)(void *, unsigned *);
    int (*clock)(void *, int, unsigned *);
} nv;

static void nvml_open(const char *hint) {
    nv.tried = 1;
    nv.lib = LoadLibraryA("nvml.dll");
    if (!nv.lib) nv.lib = LoadLibraryA("C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
    if (!nv.lib) return;

    int (*init)(void) = (int (*)(void))GetProcAddress(nv.lib, "nvmlInit_v2");
    int (*count)(unsigned *) = (int (*)(unsigned *))GetProcAddress(nv.lib, "nvmlDeviceGetCount_v2");
    int (*handle)(unsigned, void **) = (int (*)(unsigned, void **))GetProcAddress(nv.lib, "nvmlDeviceGetHandleByIndex_v2");
    int (*name)(void *, char *, unsigned) = (int (*)(void *, char *, unsigned))GetProcAddress(nv.lib, "nvmlDeviceGetName");
    nv.util = (int (*)(void *, NvmlUtil *))GetProcAddress(nv.lib, "nvmlDeviceGetUtilizationRates");
    nv.temp = (int (*)(void *, int, unsigned *))GetProcAddress(nv.lib, "nvmlDeviceGetTemperature");
    nv.mem = (int (*)(void *, NvmlMem *))GetProcAddress(nv.lib, "nvmlDeviceGetMemoryInfo");
    nv.power = (int (*)(void *, unsigned *))GetProcAddress(nv.lib, "nvmlDeviceGetPowerUsage");
    nv.clock = (int (*)(void *, int, unsigned *))GetProcAddress(nv.lib, "nvmlDeviceGetClockInfo");

    if (!init || !count || !handle || init() != 0) return;
    unsigned n = 0;
    if (count(&n) != 0 || !n) return;
    for (unsigned i = 0; i < n && !nv.dev; i++) {
        void *d = NULL;
        char nm[96] = "";
        if (handle(i, &d) != 0 || !name || name(d, nm, sizeof nm) != 0 || !nm[0]) continue;
        if (hint && hint[0] && (strstr(hint, nm) || strstr(nm, hint))) nv.dev = d;
    }
    nv.ok = nv.dev != NULL;
}

static void nvml_sample(PerfStats *s) {
    if (!nv.ok) return;
    NvmlUtil u;
    NvmlMem m;
    unsigned v;
    if (s->gpu_pct < 0 && nv.util && nv.util(nv.dev, &u) == 0) s->gpu_pct = (float)u.gpu;
    if (nv.mem && nv.mem(nv.dev, &m) == 0) {
        if (s->vram_used_mb < 0) s->vram_used_mb = (float)(m.used >> 20);
        s->vram_total_mb = (float)(m.total >> 20);
    }
    if (nv.temp && nv.temp(nv.dev, 0, &v) == 0) s->gpu_temp_c = (float)v;
    if (nv.power && nv.power(nv.dev, &v) == 0) s->gpu_power_w = v / 1000.0f;
    if (nv.clock && nv.clock(nv.dev, 0, &v) == 0) s->gpu_clock_mhz = (float)v;
}

// ---- PDH Array GPU & Thermal Query (Exact TotK Recomp implementation for Intel Arc / AMD / NVIDIA)
typedef PDH_STATUS(WINAPI *PdhOpenQueryW_t)(LPCWSTR, DWORD_PTR, PDH_HQUERY *);
typedef PDH_STATUS(WINAPI *PdhAddEnglishCounterW_t)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
typedef PDH_STATUS(WINAPI *PdhCollectQueryData_t)(PDH_HQUERY);
typedef PDH_STATUS(WINAPI *PdhGetFormattedCounterArrayW_t)(PDH_HCOUNTER, DWORD, LPDWORD, LPDWORD, PPDH_FMT_COUNTERVALUE_ITEM_W);
typedef PDH_STATUS(WINAPI *PdhCloseQuery_t)(PDH_HQUERY);

static struct {
    int tried;
    HMODULE lib;
    PdhOpenQueryW_t open;
    PdhAddEnglishCounterW_t add;
    PdhCollectQueryData_t collect;
    PdhGetFormattedCounterArrayW_t get;
    PdhCloseQuery_t close;
    PDH_HQUERY q_gpu, q_thermal;
    PDH_HCOUNTER c_gpu_util, c_gpu_mem, c_thermal;
    wchar_t luid[40];
} pdh;

static void pdh_init(const uint8_t *gpu_luid) {
    pdh.tried = 1;
    pdh.lib = LoadLibraryA("pdh.dll");
    if (!pdh.lib) return;
    pdh.open = (PdhOpenQueryW_t)(void *)GetProcAddress(pdh.lib, "PdhOpenQueryW");
    pdh.add = (PdhAddEnglishCounterW_t)(void *)GetProcAddress(pdh.lib, "PdhAddEnglishCounterW");
    pdh.collect = (PdhCollectQueryData_t)(void *)GetProcAddress(pdh.lib, "PdhCollectQueryData");
    pdh.get = (PdhGetFormattedCounterArrayW_t)(void *)GetProcAddress(pdh.lib, "PdhGetFormattedCounterArrayW");
    pdh.close = (PdhCloseQuery_t)(void *)GetProcAddress(pdh.lib, "PdhCloseQuery");
    if (!pdh.open || !pdh.add || !pdh.collect || !pdh.get || !pdh.close) return;

    pdh.luid[0] = 0;
    if (gpu_luid) {
        uint32_t lo, hi;
        memcpy(&lo, gpu_luid, 4);
        memcpy(&hi, gpu_luid + 4, 4);
        swprintf(pdh.luid, sizeof pdh.luid / sizeof *pdh.luid, L"luid_0x%08x_0x%08x", hi, lo);
    }
    if (pdh.open(NULL, 0, &pdh.q_gpu) == ERROR_SUCCESS) {
        if (pdh.add(pdh.q_gpu, L"\\GPU Engine(*)\\Utilization Percentage", 0, &pdh.c_gpu_util) != ERROR_SUCCESS)
            pdh.c_gpu_util = NULL;
        if (pdh.add(pdh.q_gpu, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &pdh.c_gpu_mem) != ERROR_SUCCESS)
            pdh.c_gpu_mem = NULL;
        pdh.collect(pdh.q_gpu);
    }
    if (pdh.open(NULL, 0, &pdh.q_thermal) == ERROR_SUCCESS) {
        if (pdh.add(pdh.q_thermal, L"\\Thermal Zone Information(*)\\Temperature", 0, &pdh.c_thermal) != ERROR_SUCCESS)
            pdh.c_thermal = NULL;
        pdh.collect(pdh.q_thermal);
    }
}

static PDH_FMT_COUNTERVALUE_ITEM_W *pdh_items(PDH_HCOUNTER c, DWORD *count) {
    DWORD size = 0;
    *count = 0;
    if (!c || pdh.get(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, count, NULL) != PDH_MORE_DATA || !size) return NULL;
    PDH_FMT_COUNTERVALUE_ITEM_W *items = (PDH_FMT_COUNTERVALUE_ITEM_W *)malloc(size);
    if (items && pdh.get(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, count, items) == ERROR_SUCCESS) return items;
    free(items);
    *count = 0;
    return NULL;
}

static void inst_luid(const wchar_t *name, wchar_t *out) {
    const wchar_t *p = wcsstr(name, L"luid_");
    size_t n = 0;
    while (p && p[n] && n < 26 && n < 39) out[n] = (wchar_t)towlower(p[n]), n++;
    out[n] = 0;
}

static void pdh_sample_gpu(float *load_pct, float *vram_used_mb) {
    *load_pct = *vram_used_mb = -1;
    if (!pdh.q_gpu || pdh.collect(pdh.q_gpu) != ERROR_SUCCESS) return;
    enum { TYPES = 32 };
    struct { wchar_t luid[40], type[32]; double sum; } t[TYPES];
    int nt = 0;
    DWORD n = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W *it = pdh_items(pdh.c_gpu_util, &n);
    for (DWORD i = 0; it && i < n; i++) {
        if (it[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA) continue;
        wchar_t lu[40];
        inst_luid(it[i].szName, lu);
        if (pdh.luid[0] && wcscmp(lu, pdh.luid)) continue;
        const wchar_t *ty = wcsstr(it[i].szName, L"engtype_");
        ty = ty ? ty + 8 : L"";
        int k = 0;
        while (k < nt && (wcscmp(t[k].luid, lu) || wcsncmp(t[k].type, ty, 31))) k++;
        if (k == nt) {
            if (nt == TYPES) continue;
            wcscpy(t[k].luid, lu);
            wcsncpy(t[k].type, ty, 31);
            t[k].type[31] = 0;
            t[k].sum = 0;
            nt++;
        }
        t[k].sum += it[i].FmtValue.doubleValue;
    }
    free(it);
    for (int k = 0; k < nt; k++)
        if (t[k].sum > *load_pct) *load_pct = (float)(t[k].sum > 100 ? 100 : t[k].sum);

    it = pdh_items(pdh.c_gpu_mem, &n);
    double sum = 0, best = -1;
    for (DWORD i = 0; it && i < n; i++) {
        if (it[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA) continue;
        wchar_t lu[40];
        inst_luid(it[i].szName, lu);
        if (pdh.luid[0] ? !wcscmp(lu, pdh.luid) : 0) sum += it[i].FmtValue.doubleValue;
        if (it[i].FmtValue.doubleValue > best) best = it[i].FmtValue.doubleValue;
    }
    if (it && (pdh.luid[0] ? sum > 0 : best >= 0))
        *vram_used_mb = (float)((pdh.luid[0] ? sum : best) / 1048576.0);
    free(it);
}

static void pdh_sample_thermal(float *temp_c) {
    *temp_c = -1;
    DWORD n = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W *it;
    if (pdh.q_thermal && pdh.collect(pdh.q_thermal) == ERROR_SUCCESS && (it = pdh_items(pdh.c_thermal, &n))) {
        for (DWORD i = 0; i < n; i++) {
            double c = it[i].FmtValue.doubleValue - 273.15;
            if (it[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA && c > *temp_c && c < 150) *temp_c = (float)c;
        }
        free(it);
    }
}

static uint64_t ft64(FILETIME ft) { return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime; }
#endif

static struct {
    int want, alive;
    pthread_t thread;
} g_smp;

static void machine_clear(PerfStats *s) {
    s->cpu_total_pct = s->cpu_game_pct = s->cpu_temp_c = -1;
    s->ram_used_mb = s->ram_total_mb = s->ram_game_mb = -1;
    s->gpu_pct = s->gpu_temp_c = s->gpu_power_w = s->gpu_clock_mhz = s->vram_used_mb = s->vram_total_mb = -1;
}

static void *sampler_thread(void *arg) {
    (void)arg;
#ifdef _WIN32
    uint64_t last_idle = 0, last_kern = 0, last_user = 0, last_proc = 0;
    pthread_mutex_lock(&g_lock);
    char gpu_name[96];
    snprintf(gpu_name, sizeof gpu_name, "%s", g_gpu_name_cfg);
    uint8_t luid[8];
    int have_luid = g_gpu_luid_ok;
    memcpy(luid, g_gpu_luid_cfg, 8);
    pthread_mutex_unlock(&g_lock);

    if (!nv.tried) nvml_open(gpu_name);
    if (!pdh.tried) pdh_init(have_luid ? luid : NULL);

    char cpu_brand[64] = "CPU";
    unsigned r[12], top = __get_cpuid_max(0x80000000, NULL);
    if (top >= 0x80000004) {
        for (unsigned i = 0; i < 3; i++) __get_cpuid(0x80000002 + i, &r[4 * i], &r[4 * i + 1], &r[4 * i + 2], &r[4 * i + 3]);
        char brand[49] = {0};
        memcpy(brand, r, 48);
        char *p = brand;
        while (*p == ' ') p++;
        snprintf(cpu_brand, sizeof cpu_brand, "%s", p);
    }
#endif

    while (1) {
        pthread_mutex_lock(&g_lock);
        if (!g_smp.want) {
            g_smp.alive = 0;
            pthread_mutex_unlock(&g_lock);
            break;
        }
        pthread_mutex_unlock(&g_lock);

#ifdef _WIN32
        PerfStats cur;
        memset(&cur, 0, sizeof cur);
        machine_clear(&cur);
        snprintf(cur.cpu_name, sizeof cur.cpu_name, "%s", cpu_brand);

        FILETIME idle, kern, user, cr, ex, pk, pu;
        if (GetSystemTimes(&idle, &kern, &user) && GetProcessTimes(GetCurrentProcess(), &cr, &ex, &pk, &pu)) {
            uint64_t i = ft64(idle), k = ft64(kern), u = ft64(user), p = ft64(pk) + ft64(pu);
            uint64_t total = (k - last_kern) + (u - last_user);
            if (last_kern && total) {
                cur.cpu_total_pct = 100.0f * (float)(total - (i - last_idle)) / (float)total;
                cur.cpu_game_pct = 100.0f * (float)(p - last_proc) / (float)total;
                if (cur.cpu_total_pct > 100.0f) cur.cpu_total_pct = 100.0f;
                if (cur.cpu_game_pct > 100.0f) cur.cpu_game_pct = 100.0f;
            }
            last_idle = i; last_kern = k; last_user = u; last_proc = p;
        }
        pdh_sample_thermal(&cur.cpu_temp_c);

        MEMORYSTATUSEX ms = {.dwLength = sizeof ms};
        if (GlobalMemoryStatusEx(&ms)) {
            cur.ram_total_mb = (float)(ms.ullTotalPhys >> 20);
            cur.ram_used_mb = (float)((ms.ullTotalPhys - ms.ullAvailPhys) >> 20);
        }
        PROCESS_MEMORY_COUNTERS pmc;
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) {
            cur.ram_game_mb = (float)(pmc.WorkingSetSize >> 20);
        }

        pdh_sample_gpu(&cur.gpu_pct, &cur.vram_used_mb);
        cur.vram_total_mb = g_vram_total_cfg;
        snprintf(cur.gpu_name, sizeof cur.gpu_name, "%s", g_gpu_name_cfg);
        nvml_sample(&cur);

        pthread_mutex_lock(&g_lock);
        if (cur.vram_used_mb <= 0 && g_vram_used_vk > 0)
            cur.vram_used_mb = g_vram_used_vk;
        memcpy(g_stats.cpu_name, cur.cpu_name, sizeof g_stats.cpu_name);
        memcpy(g_stats.gpu_name, cur.gpu_name, sizeof g_stats.gpu_name);
        g_stats.cpu_total_pct = cur.cpu_total_pct;
        g_stats.cpu_game_pct = cur.cpu_game_pct;
        g_stats.cpu_temp_c = cur.cpu_temp_c;
        g_stats.ram_used_mb = cur.ram_used_mb;
        g_stats.ram_total_mb = cur.ram_total_mb;
        g_stats.ram_game_mb = cur.ram_game_mb;
        g_stats.gpu_pct = cur.gpu_pct;
        g_stats.gpu_temp_c = cur.gpu_temp_c;
        g_stats.gpu_power_w = cur.gpu_power_w;
        g_stats.gpu_clock_mhz = cur.gpu_clock_mhz;
        g_stats.vram_used_mb = cur.vram_used_mb;
        g_stats.vram_total_mb = cur.vram_total_mb;
        pthread_mutex_unlock(&g_lock);
#endif

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 500000000L;  // 500ms
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }
        pthread_mutex_lock(&g_lock);
        if (g_smp.want) pthread_cond_timedwait(&g_cv, &g_lock, &ts);
        pthread_mutex_unlock(&g_lock);
    }
    return NULL;
}

void perfmon_mode(int mode) {
    pthread_mutex_lock(&g_lock);
    g_mode = mode;
    if (mode == 2) {
        g_smp.want = 1;
        if (!g_smp.alive) {
            machine_clear(&g_stats);
            g_smp.alive = (pthread_create(&g_smp.thread, NULL, sampler_thread, NULL) == 0);
            if (g_smp.alive) pthread_detach(g_smp.thread);
        }
    } else {
        g_smp.want = 0;
        pthread_cond_signal(&g_cv);
    }
    if (!mode) {
        g_ring_n = g_ring_pos = 0;
        memset(g_low_hist, 0, sizeof g_low_hist);
        g_low_t0 = 0;
        memset(&g_win, 0, sizeof g_win);
    }
    pthread_mutex_unlock(&g_lock);
}

int perfmon_get_mode(void) {
    return g_mode;
}

void perfmon_cycle_mode(void) {
    int next = (g_mode + 1) % 3;
    perfmon_mode(next);
}

void perfmon_snapshot(PerfStats *out) {
    uint64_t now = now_ns();
    pthread_mutex_lock(&g_lock);
    frames_summary(&g_stats, now);
    counters_summary(&g_stats, now);
    *out = g_stats;
    pthread_mutex_unlock(&g_lock);
}
