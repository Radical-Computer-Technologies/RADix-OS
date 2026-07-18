#ifndef RAD_COMPOSITOR_CORE_H
#define RAD_COMPOSITOR_CORE_H

#include <radkernel/radkernel.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RAD_COMPOSITOR_MAX_SURFACES
#define RAD_COMPOSITOR_MAX_SURFACES 16u
#endif

#ifndef RAD_COMPOSITOR_MAX_DAMAGE
#define RAD_COMPOSITOR_MAX_DAMAGE 64u
#endif

#define RAD_COMPOSITOR_SURFACE_ALPHA 1u

typedef struct rad_compositor_rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} rad_compositor_rect_t;

typedef struct rad_compositor_surface_config {
    uint32_t size;
    const char *app_id;
    const char *title;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t z;
    uint32_t flags;
    uint32_t *pixels;
    uint32_t stride_pixels;
} rad_compositor_surface_config_t;

typedef struct rad_compositor_surface_info {
    uint32_t id;
    char app_id[32];
    char title[64];
    rad_compositor_rect_t bounds;
    int32_t z;
    uint32_t flags;
    int visible;
    int focused;
    int dirty;
    uint32_t *pixels;
    uint32_t stride_pixels;
} rad_compositor_surface_info_t;

typedef struct rad_compositor_input_result {
    uint32_t surface_id;
    int32_t local_x;
    int32_t local_y;
    int hit;
} rad_compositor_input_result_t;

typedef struct rad_compositor_damage {
    uint32_t surface_id;
    rad_compositor_rect_t rect;
    uint32_t flags;
} rad_compositor_damage_t;

typedef struct rad_compositor {
    uint32_t *framebuffer;
    uint32_t *frontbuffer;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_stride_pixels;
    uint32_t frontbuffer_stride_pixels;
    uint32_t clear_color;
    uint32_t next_surface_id;
    uint32_t damage_head;
    uint32_t damage_count;
    uint32_t last_present_rect_count;
    rad_compositor_surface_info_t surfaces[RAD_COMPOSITOR_MAX_SURFACES];
    rad_compositor_damage_t damage[RAD_COMPOSITOR_MAX_DAMAGE];
    rad_compositor_rect_t last_present_rects[RAD_COMPOSITOR_MAX_DAMAGE];
} rad_compositor_t;

rad_status_t rad_compositor_init(rad_compositor_t *compositor, uint32_t *framebuffer, uint32_t width, uint32_t height, uint32_t stride_pixels);
rad_status_t rad_compositor_set_framebuffers(rad_compositor_t *compositor, uint32_t *frontbuffer, uint32_t front_stride_pixels, uint32_t *backbuffer, uint32_t back_stride_pixels);
rad_status_t rad_compositor_create_surface(rad_compositor_t *compositor, const rad_compositor_surface_config_t *config, uint32_t *surface_id);
rad_status_t rad_compositor_destroy_surface(rad_compositor_t *compositor, uint32_t surface_id);
rad_status_t rad_compositor_set_surface_bounds(rad_compositor_t *compositor, uint32_t surface_id, const rad_compositor_rect_t *bounds);
rad_status_t rad_compositor_focus_surface(rad_compositor_t *compositor, uint32_t surface_id);
rad_status_t rad_compositor_set_surface_visible(rad_compositor_t *compositor, uint32_t surface_id, int visible);
rad_status_t rad_compositor_mark_surface_dirty(rad_compositor_t *compositor, uint32_t surface_id);
rad_status_t rad_compositor_queue_damage(rad_compositor_t *compositor, uint32_t surface_id, const rad_compositor_rect_t *rect, uint32_t flags);
rad_status_t rad_compositor_submit_surface_pixels(rad_compositor_t *compositor, uint32_t surface_id, uint32_t *pixels, uint32_t stride_pixels);
rad_status_t rad_compositor_compose_frame(rad_compositor_t *compositor);
rad_status_t rad_compositor_hit_test(const rad_compositor_t *compositor, int32_t global_x, int32_t global_y, rad_compositor_input_result_t *result);
rad_status_t rad_compositor_dispatch_input(const rad_compositor_t *compositor, const rad_input_event_t *event, rad_compositor_input_result_t *result);
size_t rad_compositor_list_surfaces(const rad_compositor_t *compositor, rad_compositor_surface_info_t *surfaces, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
