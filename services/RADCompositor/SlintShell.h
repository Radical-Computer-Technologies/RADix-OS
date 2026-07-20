#pragma once

#include <radkernel/radkernel.h>
#include "RADCompositorTier.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inform the shell how much usable RAM it may assume before rad_slint_shell_start,
// so the capability tier (FULL/LEAN/HEADLESS) is chosen correctly on constrained
// targets. 0 (the default) means "unknown/ample" and resolves to FULL. A platform
// with a known small budget (e.g. Pi Zero 2 W = 512 MB) passes its byte count here.
void rad_slint_shell_set_ram_budget(uint64_t ram_budget_bytes);

rad_status_t rad_slint_shell_start(rad_framebuffer_t framebuffer, const char *terminal_text);
void rad_slint_shell_set_terminal_text(const char *terminal_text);
void rad_slint_shell_poll(void);
int rad_slint_shell_ready(void);

// The capability tier resolved at the last rad_slint_shell_start (HEADLESS until started).
rad_compositor_tier_t rad_slint_shell_tier(void);

#ifdef __cplusplus
}
#endif
