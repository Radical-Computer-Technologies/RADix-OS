#pragma once

#include <radkernel/radkernel.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

rad_status_t rad_slint_shell_start(rad_framebuffer_t framebuffer, const char *terminal_text);
void rad_slint_shell_set_terminal_text(const char *terminal_text);
void rad_slint_shell_poll(void);
int rad_slint_shell_ready(void);

#ifdef __cplusplus
}
#endif
