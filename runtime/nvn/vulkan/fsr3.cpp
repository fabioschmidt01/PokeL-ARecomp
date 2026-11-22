// AMD FSR 3.1's upscaler (third_party/fidelityfx-fsr3: FidelityFX SDK v1.1.4, MIT) for exec.c's temporal anti-aliasing,
// next to FSR 2 (fsr2.cpp) and taking the same frame (VkFsr2Frame: the game's jittered picture, its depth, our motion
// vectors and reactive mask). FSR 3.1 keeps more detail in motion and ghosts less than FSR 2.2 (AMD's shading-change and
// luma-instability passes). Its context is made again when the sizes change; one dispatch per frame is recorded into a
// command buffer of the caller's (the SDK backend makes its vkCmd* calls itself). The picture is tone-mapped already (LDR
// input, no exposure texture); its depth is reversed with an infinite far plane.
#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include "vk_fsr2.h"
#include "ffx_fsr3upscaler_private.h"  // (the context's backend interface and effect id: the shared resources below)

static void LOG(const char *fmt, ...) {  // (as runtime/rt.h rt_log: one write per line)
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n > 0) fwrite(b, 1, (size_t)n < sizeof b ? (size_t)n : sizeof b - 1, stderr);
}

static FfxFsr3UpscalerContext ctx;
static bool have;
static void *scratch;
static uint32_t cur_w, cur_h, cur_ow, cur_oh;
static bool failed;  // (the context couldn't be made: not tried again until the sizes change)
static VkDeviceContext devctx;
// FSR 3.1's shared resources (dilated depth, dilated motion vectors, the reconstructed previous nearest depth): the
// caller allocates them as ffxFsr3UpscalerGetSharedResourceDescriptions says and hands them to every dispatch - FSR 2
// made its own. Left out, the backend took the empty resources for buffers and cleared them: a crash in the Intel
// driver (igvk64.dll), 2026-11-21.
static FfxResourceInternal shared_int[3];
static FfxResource shared_res[3];
static bool have_shared;
static FfxInterface *ctx_iface(void) { return &((FfxFsr3UpscalerContext_Private *)&ctx)->contextDescription.backendInterface; }
static uint32_t ctx_effect(void) { return ((FfxFsr3UpscalerContext_Private *)&ctx)->effectContextId; }
static bool shared_make(void) {
    FfxFsr3UpscalerSharedResourceDescriptions d = {};
    if (ffxFsr3UpscalerGetSharedResourceDescriptions(&ctx, &d) != FFX_OK) return false;
    const FfxCreateResourceDescription *c[3] = {&d.dilatedDepth, &d.dilatedMotionVectors, &d.reconstructedPrevNearestDepth};
    FfxInterface *i = ctx_iface();
    for (int k = 0; k < 3; k++) {
        if (i->fpCreateResource(i, c[k], ctx_effect(), &shared_int[k]) != FFX_OK) return false;
        shared_res[k] = i->fpGetResource(i, shared_int[k]);
    }
    return have_shared = true;
}
static void shared_free(void) {
    if (!have_shared) return;
    FfxInterface *i = ctx_iface();
    for (int k = 0; k < 3; k++) i->fpDestroyResource(i, shared_int[k], ctx_effect());
    have_shared = false;
}

static void message(FfxMsgType type, const wchar_t *m) {
    char b[512];
    size_t i = 0;
    for (; m && m[i] && i < sizeof b - 1; i++) b[i] = (char)m[i];
    b[i] = 0;
    static int n;
    if (n++ < 50) LOG("[fsr3] %s: %s\n", type == FFX_MESSAGE_TYPE_ERROR ? "error" : "warning", b);
}

extern "C" void vk_fsr3_destroy(VkDevice dev) {
    if (!have) return;
    vkDeviceWaitIdle(dev);
    shared_free();
    ffxFsr3UpscalerContextDestroy(&ctx);
    have = false;
}

extern "C" void vk_fsr3_jitter(uint32_t index, uint32_t render_w, uint32_t out_w, float *jx, float *jy) {
    int32_t phases = ffxFsr3UpscalerGetJitterPhaseCount((int32_t)render_w, (int32_t)out_w);
    if (phases < 1) phases = 1;
    *jx = *jy = 0;
    ffxFsr3UpscalerGetJitterOffset(jx, jy, (int32_t)(index % (uint32_t)phases), phases);
}

// One of our images as the SDK's resource (the backend makes its own views of it)
static FfxResource res(VkImage img, VkFormat fmt, uint32_t w, uint32_t h, bool storage, const wchar_t *name,
                       FfxResourceStates state = FFX_RESOURCE_STATE_COMPUTE_READ) {
    VkImageCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent = {w, h, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | (storage ? VK_IMAGE_USAGE_STORAGE_BIT : 0);
    return ffxGetResourceVK((void *)img, ffxGetImageResourceDescriptionVK(img, ci), name, state);
}

extern "C" bool vk_fsr3_record(VkPhysicalDevice phys, VkDevice dev, VkCommandBuffer cmd, const VkFsr2Frame *f) {
    bool sizes = f->w != cur_w || f->h != cur_h || f->ow != cur_ow || f->oh != cur_oh;
    if (failed && !sizes) return false;
    if (!have || sizes) {
        vk_fsr3_destroy(dev);
        cur_w = f->w, cur_h = f->h, cur_ow = f->ow, cur_oh = f->oh;
        failed = true;
        size_t sz = ffxGetScratchMemorySizeVK(phys, FFX_FSR3UPSCALER_CONTEXT_COUNT);
        free(scratch);
        scratch = calloc(1, sz);
        devctx = {dev, phys, vkGetDeviceProcAddr};
        FfxFsr3UpscalerContextDescription d = {};
        if (!scratch || ffxGetInterfaceVK(&d.backendInterface, ffxGetDeviceVK(&devctx), scratch, sz,
                                          FFX_FSR3UPSCALER_CONTEXT_COUNT) != FFX_OK) {
            LOG("[fsr3] no backend interface\n");
            return false;
        }
        d.flags = FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED | FFX_FSR3UPSCALER_ENABLE_DEPTH_INFINITE;
        d.maxRenderSize = {f->w, f->h};
        d.maxUpscaleSize = {f->ow, f->oh};
        d.fpMessage = message;
        FfxErrorCode e = ffxFsr3UpscalerContextCreate(&ctx, &d);
        if (e != FFX_OK) { LOG("[fsr3] context failed: %d\n", (int)e); return false; }
        have = true;
        if (!shared_make()) { LOG("[fsr3] shared resources failed\n"); vk_fsr3_destroy(dev); return false; }
        failed = false;
        LOG("[fsr3] FSR 3.1 context %ux%u -> %ux%u\n", f->w, f->h, f->ow, f->oh);
    }
    FfxFsr3UpscalerDispatchDescription d = {};
    d.commandList = ffxGetCommandListVK(cmd);
    d.color = res(f->color, f->color_format, f->w, f->h, false, L"FSR3_InputColor");
    d.depth = res(f->depth, f->depth_format, f->w, f->h, false, L"FSR3_InputDepth");
    d.motionVectors = res(f->mv, VK_FORMAT_R16G16_SFLOAT, f->w, f->h, false, L"FSR3_InputMotionVectors");
    d.reactive = res(f->reactive, VK_FORMAT_R8_UNORM, f->w, f->h, false, L"FSR3_InputReactiveMap");
    d.output = res(f->out, f->out_format, f->ow, f->oh, true, L"FSR3_OutputUpscaledColor", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    d.dilatedDepth = shared_res[0], d.dilatedMotionVectors = shared_res[1], d.reconstructedPrevNearestDepth = shared_res[2];
    d.jitterOffset = {f->jx, f->jy};
    d.motionVectorScale = {1, 1};  // (pixels: FSR divides by the render size)
    d.renderSize = {f->w, f->h};
    d.upscaleSize = {f->ow, f->oh};
    d.enableSharpening = f->sharpness > 0;
    d.sharpness = f->sharpness;
    d.frameTimeDelta = f->dt_ms;
    d.preExposure = 1;
    d.reset = f->reset;
    d.cameraNear = f->near_z;
    d.cameraFar = FLT_MAX;
    d.cameraFovAngleVertical = f->fov_y;
    d.viewSpaceToMetersFactor = 1;
    FfxErrorCode e = ffxFsr3UpscalerContextDispatch(&ctx, &d);
    if (e != FFX_OK) {
        static int n;
        if (n++ < 10) LOG("[fsr3] dispatch failed: %d\n", (int)e);
        return false;
    }
    return true;
}
