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
    // Per-frame engine averages (over 0.5s window, matching TotK Recomp)
    float replay_ms, gpu_ms, pipe_wait_ms, xlate_ms, tex_uploads;
    // Picture / presentation info
    uint32_t win_w, win_h, image_w, image_h, scene_w, scene_h;
    int scaler; // 0 none (1:1), 1 bilinear, 2 fsr1, 3 xess, 4 downscale
    // Machine stats (sampled twice per second in full mode)
    char cpu_name[64];
    float cpu_total_pct, cpu_game_pct, cpu_temp_c;
    float ram_used_mb, ram_total_mb, ram_game_mb;
    char gpu_name[96];
    float gpu_pct, gpu_temp_c, gpu_power_w, gpu_clock_mhz, vram_used_mb, vram_total_mb;
} PerfStats;

void perfmon_mode(int mode);
int perfmon_get_mode(void);
void perfmon_cycle_mode(void);
void perfmon_set_gpu(const char *name, const uint8_t *luid, float vram_total_mb);
void perfmon_set_vram_used(float vram_used_mb);
void perfmon_set_picture(uint32_t win_w, uint32_t win_h, uint32_t img_w, uint32_t img_h,
                         uint32_t scene_w, uint32_t scene_h, int scaler);
void perfmon_frame_ex(uint64_t interval_ns, double replay_ms, double gpu_ms,
                      double pipe_ms, double xlate_ms, double tex_uploads);
void perfmon_frame(uint64_t interval_ns);
void perfmon_snapshot(PerfStats *out);

#ifdef __cplusplus
}
#endif
