#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

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

static char g_gpu_name_cfg[96];
static float g_vram_total_cfg = -1;

void perfmon_set_gpu(const char *name, float vram_total_mb) {
    pthread_mutex_lock(&g_lock);
    if (name) snprintf(g_gpu_name_cfg, sizeof g_gpu_name_cfg, "%s", name);
    g_vram_total_cfg = vram_total_mb > 0 ? vram_total_mb : -1;
    pthread_mutex_unlock(&g_lock);
}

void perfmon_frame(uint64_t interval_ns) {
    if (!g_mode) return;
    pthread_mutex_lock(&g_lock);
    uint64_t now = 0;
#ifdef _WIN32
    static LARGE_INTEGER freq;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER cnt;
    QueryPerformanceCounter(&cnt);
    now = (uint64_t)((double)cnt.QuadPart * 1e9 / (double)freq.QuadPart);
#endif
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
    pthread_mutex_unlock(&g_lock);
}

static void frames_summary(PerfStats *s) {
    double sum = 0, mx = 0;
    int n = 0;
    for (int i = 0; i < g_ring_n; i++) {
        int k = (g_ring_pos - 1 - i + PERF_HISTORY) % PERF_HISTORY;
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

static void nvml_open(void) {
    nv.tried = 1;
    nv.lib = LoadLibraryA("nvml.dll");
    if (!nv.lib) nv.lib = LoadLibraryA("C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
    if (!nv.lib) return;

    int (*init)(void) = (int (*)(void))GetProcAddress(nv.lib, "nvmlInit_v2");
    int (*count)(unsigned *) = (int (*)(unsigned *))GetProcAddress(nv.lib, "nvmlDeviceGetCount_v2");
    int (*handle)(unsigned, void **) = (int (*)(unsigned, void **))GetProcAddress(nv.lib, "nvmlDeviceGetHandleByIndex_v2");
    nv.util = (int (*)(void *, NvmlUtil *))GetProcAddress(nv.lib, "nvmlDeviceGetUtilizationRates");
    nv.temp = (int (*)(void *, int, unsigned *))GetProcAddress(nv.lib, "nvmlDeviceGetTemperature");
    nv.mem = (int (*)(void *, NvmlMem *))GetProcAddress(nv.lib, "nvmlDeviceGetMemoryInfo");
    nv.power = (int (*)(void *, unsigned *))GetProcAddress(nv.lib, "nvmlDeviceGetPowerUsage");
    nv.clock = (int (*)(void *, int, unsigned *))GetProcAddress(nv.lib, "nvmlDeviceGetClockInfo");

    if (!init || !count || !handle || init() != 0) return;
    unsigned n = 0;
    if (count(&n) != 0 || !n) return;
    if (handle(0, &nv.dev) == 0 && nv.dev) nv.ok = 1;
}

static void nvml_sample(PerfStats *s) {
    if (!nv.ok) return;
    NvmlUtil u;
    NvmlMem m;
    unsigned v;
    if (nv.util && nv.util(nv.dev, &u) == 0) s->gpu_pct = (float)u.gpu;
    if (nv.mem && nv.mem(nv.dev, &m) == 0) {
        s->vram_used_mb = (float)(m.used >> 20);
        s->vram_total_mb = (float)(m.total >> 20);
    }
    if (nv.temp && nv.temp(nv.dev, 0, &v) == 0) s->gpu_temp_c = (float)v;
    if (nv.power && nv.power(nv.dev, &v) == 0) s->gpu_power_w = v / 1000.0f;
    if (nv.clock && nv.clock(nv.dev, 0, &v) == 0) s->gpu_clock_mhz = (float)v;
}

// PDH GPU load query
static struct {
    int tried, ok;
    HMODULE lib;
    PDH_HQUERY query;
    PDH_HCOUNTER counter_gpu, counter_vram;
    PDH_STATUS (__stdcall *PdhOpenQueryW)(LPCWSTR, DWORD_PTR, PDH_HQUERY *);
    PDH_STATUS (__stdcall *PdhAddEnglishCounterW)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
    PDH_STATUS (__stdcall *PdhCollectQueryData)(PDH_HQUERY);
    PDH_STATUS (__stdcall *PdhGetFormattedCounterValue)(PDH_HCOUNTER, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE);
    PDH_STATUS (__stdcall *PdhCloseQuery)(PDH_HQUERY);
} pdh_ctx;

static void pdh_init(void) {
    pdh_ctx.tried = 1;
    pdh_ctx.lib = LoadLibraryA("pdh.dll");
    if (!pdh_ctx.lib) return;
    pdh_ctx.PdhOpenQueryW = (void *)GetProcAddress(pdh_ctx.lib, "PdhOpenQueryW");
    pdh_ctx.PdhAddEnglishCounterW = (void *)GetProcAddress(pdh_ctx.lib, "PdhAddEnglishCounterW");
    pdh_ctx.PdhCollectQueryData = (void *)GetProcAddress(pdh_ctx.lib, "PdhCollectQueryData");
    pdh_ctx.PdhGetFormattedCounterValue = (void *)GetProcAddress(pdh_ctx.lib, "PdhGetFormattedCounterValue");
    pdh_ctx.PdhCloseQuery = (void *)GetProcAddress(pdh_ctx.lib, "PdhCloseQuery");

    if (pdh_ctx.PdhOpenQueryW && pdh_ctx.PdhOpenQueryW(NULL, 0, &pdh_ctx.query) == ERROR_SUCCESS) {
        if (pdh_ctx.PdhAddEnglishCounterW(pdh_ctx.query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &pdh_ctx.counter_gpu) == ERROR_SUCCESS) {
            pdh_ctx.PdhCollectQueryData(pdh_ctx.query);
            pdh_ctx.ok = 1;
        }
    }
}
static uint64_t ft64(FILETIME ft) { return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime; }
#endif

static struct {
    int want, alive;
    pthread_t thread;
} g_smp;

static void *sampler_thread(void *arg) {
    (void)arg;
#ifdef _WIN32
    uint64_t last_idle = 0, last_kern = 0, last_user = 0, last_proc = 0;
    if (!nv.tried) nvml_open();
    if (!pdh_ctx.tried) pdh_init();

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
        float cpu_total = -1, cpu_game = -1;
        FILETIME idle, kern, user, cr, ex, pk, pu;
        if (GetSystemTimes(&idle, &kern, &user) && GetProcessTimes(GetCurrentProcess(), &cr, &ex, &pk, &pu)) {
            uint64_t i = ft64(idle), k = ft64(kern), u = ft64(user), p = ft64(pk) + ft64(pu);
            uint64_t total = (k - last_kern) + (u - last_user);
            if (last_kern && total) {
                cpu_total = 100.0f * (float)(total - (i - last_idle)) / (float)total;
                cpu_game = 100.0f * (float)(p - last_proc) / (float)total;
                if (cpu_total > 100.0f) cpu_total = 100.0f;
                if (cpu_game > 100.0f) cpu_game = 100.0f;
            }
            last_idle = i; last_kern = k; last_user = u; last_proc = p;
        }

        float ram_used = -1, ram_total = -1, ram_game = -1;
        MEMORYSTATUSEX ms = {.dwLength = sizeof ms};
        if (GlobalMemoryStatusEx(&ms)) {
            ram_total = (float)(ms.ullTotalPhys >> 20);
            ram_used = (float)((ms.ullTotalPhys - ms.ullAvailPhys) >> 20);
        }
        PROCESS_MEMORY_COUNTERS pmc;
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) {
            ram_game = (float)(pmc.WorkingSetSize >> 20);
        }

        PerfStats cur;
        memset(&cur, 0, sizeof cur);
        snprintf(cur.cpu_name, sizeof cur.cpu_name, "%s", cpu_brand);
        cur.cpu_total_pct = cpu_total;
        cur.cpu_game_pct = cpu_game;
        cur.cpu_temp_c = -1;
        cur.ram_used_mb = ram_used;
        cur.ram_total_mb = ram_total;
        cur.ram_game_mb = ram_game;
        cur.gpu_pct = -1;
        cur.gpu_temp_c = -1;
        cur.gpu_power_w = -1;
        cur.gpu_clock_mhz = -1;
        cur.vram_used_mb = -1;
        cur.vram_total_mb = g_vram_total_cfg;
        snprintf(cur.gpu_name, sizeof cur.gpu_name, "%s", g_gpu_name_cfg);

        // Sample NVML if available
        nvml_sample(&cur);

        // Fallback GPU load via PDH if NVML didn't set it
        if (cur.gpu_pct < 0 && pdh_ctx.ok) {
            pdh_ctx.PdhCollectQueryData(pdh_ctx.query);
            PDH_FMT_COUNTERVALUE val;
            if (pdh_ctx.PdhGetFormattedCounterValue(pdh_ctx.counter_gpu, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS) {
                cur.gpu_pct = (float)val.doubleValue;
                if (cur.gpu_pct > 100.0f) cur.gpu_pct = 100.0f;
            }
        }

        pthread_mutex_lock(&g_lock);
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
        if (cur.vram_used_mb > 0) g_stats.vram_used_mb = cur.vram_used_mb;
        if (cur.vram_total_mb > 0) g_stats.vram_total_mb = cur.vram_total_mb;
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
    pthread_mutex_lock(&g_lock);
    frames_summary(&g_stats);
    lows_summary(&g_stats);
    *out = g_stats;
    pthread_mutex_unlock(&g_lock);
}
