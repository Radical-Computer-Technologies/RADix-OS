#ifndef RAD_COMPOSITOR_TIER_H
#define RAD_COMPOSITOR_TIER_H

// Display capability tiers for the RADPx-OS graphical shell. A single freestanding
// policy shared by the kernel SlintShell and the hosted rad_os_shell self-test so the
// tier decision is identical everywhere. No std/allocation — links into the -nostdlib
// kernel image unchanged.
//
//   FULL     capable framebuffer + ample RAM (x86, capable boards): all effects.
//   LEAN     small framebuffer or low RAM (Pi Zero 2 W, 512 MB): reduced richness.
//   HEADLESS no usable framebuffer (serial-only boards, e.g. zuboard): do not start
//            the window manager; boot proceeds clean.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rad_compositor_tier {
    RAD_COMPOSITOR_TIER_HEADLESS = 0,
    RAD_COMPOSITOR_TIER_LEAN = 1,
    RAD_COMPOSITOR_TIER_FULL = 2,
} rad_compositor_tier_t;

typedef struct rad_compositor_caps {
    int has_framebuffer;        // non-zero if a usable primary framebuffer exists
    uint32_t width;             // framebuffer width in pixels (0 if none)
    uint32_t height;            // framebuffer height in pixels (0 if none)
    uint32_t depth_bits;        // bits per pixel (0 if unknown)
    uint64_t ram_budget_bytes;  // usable RAM the shell may assume (0 if unknown)
    rad_compositor_tier_t tier; // resolved tier
} rad_compositor_caps_t;

// Pure policy: resolve the tier from raw capabilities. Deterministic, no side effects.
rad_compositor_tier_t rad_compositor_select_tier(int has_framebuffer,
                                                 uint32_t width, uint32_t height,
                                                 uint32_t depth_bits,
                                                 uint64_t ram_budget_bytes);

// Fill a caps struct (including the resolved tier) from raw capabilities.
void rad_compositor_probe_caps(int has_framebuffer,
                               uint32_t width, uint32_t height, uint32_t depth_bits,
                               uint64_t ram_budget_bytes,
                               rad_compositor_caps_t *out);

// Stable marker string for a tier (e.g. "RAD_COMPOSITOR_TIER_FULL_OK").
const char *rad_compositor_tier_marker(rad_compositor_tier_t tier);

// Short human label for a tier (e.g. "full").
const char *rad_compositor_tier_label(rad_compositor_tier_t tier);

#ifdef __cplusplus
}
#endif

#endif // RAD_COMPOSITOR_TIER_H
