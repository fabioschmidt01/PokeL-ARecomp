#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PERF_HISTORY 120   // frame times kept for the graph

typedef struct {
    float fps, frame_ms_avg, frame_ms_max;
    float low1_fps, low01_fps;
    float frame_ms[PERF_HISTORY];
    char cpu_name[64];
    float cpu_total_pct, cpu_game_pct, cpu_temp_c;
    float ram_used_mb, ram_total_mb, ram_game_mb;
    char gpu_name[96];
    float gpu_pct, gpu_temp_c, gpu_power_w, gpu_clock_mhz, vram_used_mb, vram_total_mb;
} PerfStats;

void perfmon_mode(int mode);
int perfmon_get_mode(void);
void perfmon_cycle_mode(void);
void perfmon_set_gpu(const char *name, float vram_total_mb);
void perfmon_frame(uint64_t interval_ns);
void perfmon_snapshot(PerfStats *out);

#ifdef __cplusplus
}
#endif
