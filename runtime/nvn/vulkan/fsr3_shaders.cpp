// AMD FSR 3.1's upscaler shaders (third_party/fidelityfx-fsr3: FidelityFX SDK v1.1.4, MIT) for the SDK's Vulkan backend:
// the replacement for its shared/blob_accessors (headers of permutations precompiled by AMD's own shader compiler at
// its build). As with FSR 2 (fsr2_shaders.cpp): the GLSL sources are built in (fsr3_shaders.inc), each permutation a
// pass asks for is compiled once with shaderc and kept, and what the backend binds by - the names, bindings and array
// sizes of the pass's constant buffers, sampled / storage images and storage buffers - is read from the SPIR-V itself.
#include <shaderc/shaderc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include <FidelityFX/host/ffx_interface.h>
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <ffx_shader_blobs.h>
#include "ffx_fsr3upscaler_private.h"

static void LOG(const char *fmt, ...) {  // (as runtime/rt.h rt_log: one write per line)
    char b[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n > 0) fwrite(b, 1, (size_t)n < sizeof b ? (size_t)n : sizeof b - 1, stderr);
}

static const struct { const char *name, *text; } k_sources[] = {
#include "fsr3_shaders.inc"
};

// AMD's includes name a file with or without its folder ("fsr3upscaler/ffx_fsr3upscaler_common.h", "ffx_core.h"): every
// name is unique, so the last path part is enough
static shaderc_include_result *include_resolve(void *, const char *requested, int, const char *, size_t) {
    shaderc_include_result *r = (shaderc_include_result *)calloc(1, sizeof *r);
    const char *base = strrchr(requested, '/');
    base = base ? base + 1 : requested;
    for (const auto &s : k_sources)
        if (!strcmp(s.name, base)) {
            r->source_name = s.name, r->source_name_length = strlen(s.name);
            r->content = s.text, r->content_length = strlen(s.text);
            return r;
        }
    static const char missing[] = "fsr3_shaders: no such include";
    r->content = missing, r->content_length = sizeof missing - 1;
    return r;
}
static void include_release(void *, shaderc_include_result *r) { free(r); }

enum { K_CB, K_SRV_TEX, K_UAV_TEX, K_SRV_BUF, K_UAV_BUF, K_N };
struct Blob {
    uint32_t pass, options;
    std::vector<uint32_t> spv;
    std::vector<std::string> names[K_N];
    std::vector<const char *> cnames[K_N];
    std::vector<uint32_t> bindings[K_N], counts[K_N], spaces[K_N];
};

// The descriptor set 0 variables of a SPIR-V module (bindings 1000+ are the backend's own static samplers)
static void spv_reflect(Blob &b) {
    const std::vector<uint32_t> &w = b.spv;
    size_t n = w.size();
    if (n < 5) return;
    uint32_t bound = w[3];
    std::vector<std::string> name(bound);
    std::vector<int32_t> binding(bound, -1), set(bound, -1);
    std::vector<uint32_t> op(bound), a1(bound), a2(bound), a3(bound), cval(bound);
    std::vector<bool> block(bound), buffer_block(bound), nonwritable(bound);
    struct Var { uint32_t id, type, sc; };
    std::vector<Var> vars;
    // (SPIR-V 1.4+, as the Vulkan 1.2 target makes: the entry point lists every global it uses. Declared but unused
    // resources - "rw_output_autocomposition" of a deprecated pass - are left out, as AMD's optimized build drops them:
    // the backend refuses a name it has no slot for)
    std::vector<bool> used(bound);
    for (size_t i = 5; i < n;) {
        uint32_t len = w[i] >> 16, code = w[i] & 0xFFFF;
        if (!len || i + len > n) break;
        const uint32_t *o = w.data() + i + 1;
        switch (code) {
        case 15: {  // OpEntryPoint: execution model, id, name (a string), interface ids
            uint32_t k = 2;
            while (k < len - 1 && !(((const uint8_t *)&o[k])[0] == 0 || ((const uint8_t *)&o[k])[1] == 0 ||
                                    ((const uint8_t *)&o[k])[2] == 0 || ((const uint8_t *)&o[k])[3] == 0)) k++;
            for (k++; k < len - 1; k++) if (o[k] < bound) used[o[k]] = true;
            break;
        }
        case 5:  // OpName
            if (o[0] < bound) name[o[0]] = (const char *)&o[1];
            break;
        case 43:  // OpConstant: result type, result, value
            if (o[1] < bound) cval[o[1]] = o[2];
            break;
        case 71:  // OpDecorate: target, decoration, operands
            if (o[0] >= bound) break;
            if (o[1] == 33) binding[o[0]] = (int32_t)o[2];
            if (o[1] == 34) set[o[0]] = (int32_t)o[2];
            if (o[1] == 2) block[o[0]] = true;         // Block
            if (o[1] == 3) buffer_block[o[0]] = true;  // BufferBlock
            if (o[1] == 24) nonwritable[o[0]] = true;  // NonWritable
            break;
        case 25:  // OpTypeImage: result, sampled type, dim, depth, arrayed, ms, sampled (1 sampled, 2 storage), format
            if (o[0] < bound) op[o[0]] = 25, a1[o[0]] = o[6];
            break;
        case 28:  // OpTypeArray: result, element type, length (a constant)
            if (o[0] < bound) op[o[0]] = 28, a1[o[0]] = o[1], a2[o[0]] = o[2];
            break;
        case 29:  // OpTypeRuntimeArray
            if (o[0] < bound) op[o[0]] = 29, a1[o[0]] = o[1];
            break;
        case 30:  // OpTypeStruct
            if (o[0] < bound) op[o[0]] = 30;
            break;
        case 32:  // OpTypePointer: result, storage class, type
            if (o[0] < bound) op[o[0]] = 32, a2[o[0]] = o[1], a3[o[0]] = o[2];
            break;
        case 59:  // OpVariable: result type, result, storage class
            vars.push_back({o[1], o[0], o[2]});
            break;
        }
        i += len;
    }
    for (const Var &v : vars) {
        if (v.id >= bound || !used[v.id] || set[v.id] != 0 || binding[v.id] < 0 || binding[v.id] >= 1000 || v.type >= bound ||
            op[v.type] != 32)
            continue;
        uint32_t t = a3[v.type], count = 1;
        if (t < bound && op[t] == 28) count = a2[t] < bound ? cval[a2[t]] : 1, t = a1[t];
        int kind = -1;
        if (t < bound && op[t] == 25) kind = a1[t] == 2 ? K_UAV_TEX : K_SRV_TEX;
        else if (t < bound && op[t] == 30) {
            bool storage = v.sc == 12 /* StorageBuffer */ || buffer_block[t];
            kind = !storage ? K_CB : nonwritable[v.id] ? K_SRV_BUF : K_UAV_BUF;
        }
        if (kind < 0) continue;
        b.names[kind].push_back(name[v.id]);
        b.bindings[kind].push_back((uint32_t)binding[v.id]);
        b.counts[kind].push_back(count ? count : 1);
        b.spaces[kind].push_back(0);
    }
    // (the reactive-mask generator's second output has no slot in the SDK's table - its own build leaves it out - and
    // the backend refuses a name it can't place: dropped. That pass is never dispatched here: our mask is made in exec.c)
    for (int k = 0; k < K_N; k++)
        for (size_t i = 0; i < b.names[k].size(); i++)
            if (b.names[k][i] == "rw_output_autocomposition") {
                b.names[k].erase(b.names[k].begin() + (long)i), b.bindings[k].erase(b.bindings[k].begin() + (long)i);
                b.counts[k].erase(b.counts[k].begin() + (long)i), b.spaces[k].erase(b.spaces[k].begin() + (long)i);
                i--;
            }
    for (int k = 0; k < K_N; k++)
        for (const std::string &s : b.names[k]) b.cnames[k].push_back(s.c_str());
}

static const char *pass_source(uint32_t pass) {
    switch ((FfxFsr3UpscalerPass)pass) {
    case FFX_FSR3UPSCALER_PASS_PREPARE_INPUTS: return "ffx_fsr3upscaler_prepare_inputs_pass.glsl";
    case FFX_FSR3UPSCALER_PASS_LUMA_PYRAMID: return "ffx_fsr3upscaler_luma_pyramid_pass.glsl";
    case FFX_FSR3UPSCALER_PASS_SHADING_CHANGE_PYRAMID: return "ffx_fsr3upscaler_shading_change_pyramid_pass.glsl";
    case FFX_FSR3UPSCALER_PASS_SHADING_CHANGE: return "ffx_fsr3upscaler_shading_change_pass.glsl";
    case FFX_FSR3UPSCALER_PASS_PREPARE_REACTIVITY: return "ffx_fsr3upscaler_prepare_reactivity_pass.glsl";
    case FFX_FSR3UPSCALER_PASS_LUMA_INSTABILITY: return "ffx_fsr3upscaler_luma_instability_pass.glsl";
    case FFX_FSR3UPSCALER_PASS_ACCUMULATE:
    case FFX_FSR3UPSCALER_PASS_ACCUMULATE_SHARPEN: return "ffx_fsr3upscaler_accumulate_pass.glsl";
    case FFX_FSR3UPSCALER_PASS_RCAS: return "ffx_fsr3upscaler_rcas_pass.glsl";
    case FFX_FSR3UPSCALER_PASS_DEBUG_VIEW: return "ffx_fsr3upscaler_debug_view_pass.glsl";
    case FFX_FSR3UPSCALER_PASS_GENERATE_REACTIVE: return "ffx_fsr3upscaler_autogen_reactive_pass.glsl";
    default: return NULL;
    }
}

static Blob *blob_get(uint32_t pass, uint32_t options) {
    static std::vector<Blob *> cache;
    // (16-bit types: the device is made without shaderFloat16 / shaderInt16 - vulkan/device.c - so never those; wave64
    // picks another table in AMD's build, the same SPIR-V)
    options &= ~(uint32_t)(FSR3UPSCALER_SHADER_PERMUTATION_FORCE_WAVE64 | FSR3UPSCALER_SHADER_PERMUTATION_ALLOW_FP16);
    for (Blob *c : cache)
        if (c->pass == pass && c->options == options) return c;
    const char *file = pass_source(pass), *text = NULL;
    for (const auto &s : k_sources)
        if (file && !strcmp(s.name, file)) text = s.text;
    if (!text) { LOG("[fsr3] no shader for pass %u\n", pass); return NULL; }
    shaderc_compiler_t comp = shaderc_compiler_initialize();
    shaderc_compile_options_t o = shaderc_compile_options_initialize();
    shaderc_compile_options_set_target_env(o, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    // (unoptimized: shaderc's optimizer strips the names the backend binds by; the driver optimizes anyway)
    shaderc_compile_options_set_optimization_level(o, shaderc_optimization_level_zero);
    shaderc_compile_options_set_include_callbacks(o, include_resolve, include_release, NULL);
    struct { const char *d; uint32_t flag; } defs[] = {
        {"FFX_FSR3UPSCALER_OPTION_REPROJECT_USE_LANCZOS_TYPE", FSR3UPSCALER_SHADER_PERMUTATION_USE_LANCZOS_TYPE},
        {"FFX_FSR3UPSCALER_OPTION_HDR_COLOR_INPUT", FSR3UPSCALER_SHADER_PERMUTATION_HDR_COLOR_INPUT},
        {"FFX_FSR3UPSCALER_OPTION_LOW_RESOLUTION_MOTION_VECTORS", FSR3UPSCALER_SHADER_PERMUTATION_LOW_RES_MOTION_VECTORS},
        {"FFX_FSR3UPSCALER_OPTION_JITTERED_MOTION_VECTORS", FSR3UPSCALER_SHADER_PERMUTATION_JITTER_MOTION_VECTORS},
        {"FFX_FSR3UPSCALER_OPTION_INVERTED_DEPTH", FSR3UPSCALER_SHADER_PERMUTATION_DEPTH_INVERTED},
        {"FFX_FSR3UPSCALER_OPTION_APPLY_SHARPENING", FSR3UPSCALER_SHADER_PERMUTATION_ENABLE_SHARPENING},
        {"FFX_HALF", FSR3UPSCALER_SHADER_PERMUTATION_ALLOW_FP16}};
    shaderc_compile_options_add_macro_definition(o, "FFX_GLSL", 8, "1", 1);
    shaderc_compile_options_add_macro_definition(o, "FFX_GPU", 7, "1", 1);
    for (const auto &d : defs) shaderc_compile_options_add_macro_definition(o, d.d, strlen(d.d), options & d.flag ? "1" : "0", 1);
    shaderc_compilation_result_t r = shaderc_compile_into_spv(comp, text, strlen(text), shaderc_compute_shader, file, "main", o);
    Blob *b = NULL;
    if (shaderc_result_get_compilation_status(r) == shaderc_compilation_status_success) {
        b = new Blob;
        b->pass = pass, b->options = options;
        size_t bytes = shaderc_result_get_length(r);
        b->spv.assign((const uint32_t *)shaderc_result_get_bytes(r), (const uint32_t *)shaderc_result_get_bytes(r) + bytes / 4);
        spv_reflect(*b);
        cache.push_back(b);
        if (getenv("LARECOMP_FSR3_REFLECT"))
            for (int k = 0; k < K_N; k++)
                for (size_t i = 0; i < b->names[k].size(); i++)
                    LOG("[fsr3]   %s %u x%u %s\n", (const char *[]){"cb", "srv", "uav", "srvbuf", "uavbuf"}[k], b->bindings[k][i],
                        b->counts[k][i], b->names[k][i].c_str());
        LOG("[fsr3] %s (options %#x): %zu bytes, %zu buffers, %zu sampled / %zu storage images, %zu / %zu storage buffers\n",
            file, options, bytes, b->names[K_CB].size(), b->names[K_SRV_TEX].size(), b->names[K_UAV_TEX].size(),
            b->names[K_SRV_BUF].size(), b->names[K_UAV_BUF].size());
    } else {
        LOG("[fsr3] %s (options %#x) failed to compile:\n%s\n", file, options, shaderc_result_get_error_message(r));
    }
    shaderc_result_release(r);
    shaderc_compile_options_release(o);
    shaderc_compiler_release(comp);
    return b;
}

extern "C" FfxErrorCode ffxGetPermutationBlobByIndex(FfxEffect effectId, FfxPass passId, FfxBindStage, uint32_t permutationOptions,
                                                     FfxShaderBlob *outBlob) {
    if (effectId != FFX_EFFECT_FSR3UPSCALER) return FFX_ERROR_INVALID_ARGUMENT;  // (the only effect built in)
    Blob *b = blob_get((uint32_t)passId, permutationOptions);
    if (!b) return FFX_ERROR_BACKEND_API_ERROR;
    // (FfxShaderBlob's members are const: filled through a local of the same layout)
    FfxShaderBlob t = {(const uint8_t *)b->spv.data(), (uint32_t)(b->spv.size() * 4),
                       (uint32_t)b->names[K_CB].size(), (uint32_t)b->names[K_SRV_TEX].size(), (uint32_t)b->names[K_UAV_TEX].size(),
                       (uint32_t)b->names[K_SRV_BUF].size(), (uint32_t)b->names[K_UAV_BUF].size(), 0, 0,
                       b->cnames[K_CB].data(), b->bindings[K_CB].data(), b->counts[K_CB].data(), b->spaces[K_CB].data(),
                       b->cnames[K_SRV_TEX].data(), b->bindings[K_SRV_TEX].data(), b->counts[K_SRV_TEX].data(), b->spaces[K_SRV_TEX].data(),
                       b->cnames[K_UAV_TEX].data(), b->bindings[K_UAV_TEX].data(), b->counts[K_UAV_TEX].data(), b->spaces[K_UAV_TEX].data(),
                       b->cnames[K_SRV_BUF].data(), b->bindings[K_SRV_BUF].data(), b->counts[K_SRV_BUF].data(), b->spaces[K_SRV_BUF].data(),
                       b->cnames[K_UAV_BUF].data(), b->bindings[K_UAV_BUF].data(), b->counts[K_UAV_BUF].data(), b->spaces[K_UAV_BUF].data(),
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    memcpy((void *)outBlob, &t, sizeof t);
    return FFX_OK;
}

extern "C" FfxErrorCode ffxIsWave64(FfxEffect, uint32_t, bool &isWave64) {
    isWave64 = false;
    return FFX_OK;
}

// The SDK's Vulkan backend calls into its frame-interpolation swapchain (not built in: only the upscaler is used)
#include <FidelityFX/host/backends/vk/ffx_vk.h>
FfxErrorCode ffxSetFrameGenerationConfigToSwapchainVK(FfxFrameGenerationConfig const *) { return FFX_ERROR_INVALID_ARGUMENT; }
