// Intel XeSS (Xe Super Sampling) for the Vulkan backend (xess.cpp).
// Dynamically loads libxess.dll with native Intel Arc (XMX) hardware acceleration.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    XESS_QUALITY_PERFORMANCE = 1,
    XESS_QUALITY_BALANCED = 2,
    XESS_QUALITY_QUALITY = 3,
    XESS_QUALITY_ULTRA_QUALITY = 4,
    XESS_QUALITY_AA = 5
} VkXeSSQuality;

typedef struct {
    VkImage color, depth, mv, out;
    VkImageView color_view, depth_view, mv_view, out_view;
    VkFormat color_format, depth_format, out_format;
    uint32_t w, h, ow, oh;
    float jx, jy;
    float sharpness;
    bool reset;
} VkXeSSFrame;

// Checks whether Intel XeSS runtime is available on this system (libxess.dll + compatible Vulkan driver)
bool vk_xess_available(void);

// Records XeSS upscaling into command buffer
bool vk_xess_record(VkPhysicalDevice phys, VkDevice dev, VkCommandBuffer cmd, const VkXeSSFrame *f, VkXeSSQuality quality);

void vk_xess_destroy(VkDevice dev);

#ifdef __cplusplus
}
#endif
