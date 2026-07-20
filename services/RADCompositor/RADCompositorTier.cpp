#include "RADCompositorTier.h"

// Threshold below which a target is treated as RAM-constrained and runs the lean
// tier. The Pi Zero 2 W has 512 MB; capable desktops/boards report far more (or
// report 0 = unknown, which we treat as ample). 768 MB cleanly separates the two.
static const uint64_t kLeanRamCeilingBytes = 768ull * 1024ull * 1024ull;

// Framebuffers shallower than 24bpp can't afford the full effect set.
static const uint32_t kFullDepthMinBits = 24u;

extern "C" rad_compositor_tier_t rad_compositor_select_tier(int has_framebuffer,
                                                            uint32_t width, uint32_t height,
                                                            uint32_t depth_bits,
                                                            uint64_t ram_budget_bytes) {
    if (!has_framebuffer || width == 0u || height == 0u) {
        return RAD_COMPOSITOR_TIER_HEADLESS;
    }
    if (ram_budget_bytes != 0ull && ram_budget_bytes < kLeanRamCeilingBytes) {
        return RAD_COMPOSITOR_TIER_LEAN;
    }
    if (depth_bits != 0u && depth_bits < kFullDepthMinBits) {
        return RAD_COMPOSITOR_TIER_LEAN;
    }
    return RAD_COMPOSITOR_TIER_FULL;
}

extern "C" void rad_compositor_probe_caps(int has_framebuffer,
                                          uint32_t width, uint32_t height, uint32_t depth_bits,
                                          uint64_t ram_budget_bytes,
                                          rad_compositor_caps_t *out) {
    if (!out) return;
    out->has_framebuffer = has_framebuffer ? 1 : 0;
    out->width = width;
    out->height = height;
    out->depth_bits = depth_bits;
    out->ram_budget_bytes = ram_budget_bytes;
    out->tier = rad_compositor_select_tier(has_framebuffer, width, height, depth_bits, ram_budget_bytes);
}

extern "C" const char *rad_compositor_tier_marker(rad_compositor_tier_t tier) {
    switch (tier) {
        case RAD_COMPOSITOR_TIER_FULL: return "RAD_COMPOSITOR_TIER_FULL_OK";
        case RAD_COMPOSITOR_TIER_LEAN: return "RAD_COMPOSITOR_TIER_LEAN_OK";
        case RAD_COMPOSITOR_TIER_HEADLESS: default: return "RAD_COMPOSITOR_TIER_HEADLESS_OK";
    }
}

extern "C" const char *rad_compositor_tier_label(rad_compositor_tier_t tier) {
    switch (tier) {
        case RAD_COMPOSITOR_TIER_FULL: return "full";
        case RAD_COMPOSITOR_TIER_LEAN: return "lean";
        case RAD_COMPOSITOR_TIER_HEADLESS: default: return "headless";
    }
}
