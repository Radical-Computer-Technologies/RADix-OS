#include "slint_shell.h"

#include <radixkernel/rad_display.h>
#include <radixkernel/rad_input.h>
#include <radixkernel/rad_pty.h>

#include "RADCompositorCore.h"

#include <memory>
#include <span>
#include <string_view>
#include <utility>

#include <slint-platform.h>

#include "RADCompositor.h"

#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_write(const char *text);
extern "C" rad_status_t radix_shell_launch_terminal_process(void) __attribute__((weak));
extern "C" rad_status_t x86_user_spawn_process(const char *path, int32_t parent_pid, int32_t *pid_out, rad_task_t *task_out) __attribute__((weak));
extern "C" rad_status_t x86_user_spawn_process_with_stdio(const char *path, int32_t parent_pid, const char *stdio_path, int32_t *pid_out, rad_task_t *task_out) __attribute__((weak));
extern "C" int x86_user_wait_process(int32_t pid) __attribute__((weak));

namespace {

constexpr uint32_t MaxShellText = 8192u;
constexpr uint32_t TerminalWindowId = 1u;
constexpr int32_t MinWindowWidth = 180;
constexpr int32_t MinWindowHeight = 112;
constexpr uint32_t MaxSurfaceWidth = 1920u;
constexpr uint32_t MaxSurfaceHeight = 1080u;

enum class SlintSurfaceRole {
    Desktop,
    Terminal
};

enum class TerminalAppState {
    Closed = 0,
    Launching = 1,
    Running = 2
};

enum class DesktopWindowState {
    Closed = 0,
    Loading = 1,
    Running = 2
};

struct DesktopWindowBounds {
    int32_t x = 134;
    int32_t y = 110;
    int32_t width = 640;
    int32_t height = 380;
};

struct DesktopWindow {
    uint32_t id = TerminalWindowId;
    const char *title = "Terminal";
    DesktopWindowBounds bounds{};
    DesktopWindowState state = DesktopWindowState::Running;
    uint32_t z = 1;
    bool focused = true;
};

class EmbeddedDesktopShellModel {
public:
    bool applicationsMenuOpen() const {
        return applications_menu_open_;
    }

    bool terminalOpen() const {
        return terminal_window_.state != DesktopWindowState::Closed;
    }

    bool terminalLaunching() const {
        return terminal_window_.state == DesktopWindowState::Loading;
    }

    TerminalAppState terminalState() const {
        if (terminal_window_.state == DesktopWindowState::Closed) return TerminalAppState::Closed;
        if (terminal_window_.state == DesktopWindowState::Loading) return TerminalAppState::Launching;
        return TerminalAppState::Running;
    }

    const DesktopWindow *terminalWindow() const {
        return &terminal_window_;
    }

    bool focusWindow(uint32_t window_id) {
        if (window_id != terminal_window_.id || terminal_window_.state == DesktopWindowState::Closed) return false;
        focusTerminal();
        applications_menu_open_ = false;
        return true;
    }

    bool closeWindow(uint32_t window_id) {
        if (window_id != terminal_window_.id) return false;
        terminal_window_.state = DesktopWindowState::Closed;
        terminal_window_.focused = false;
        applications_menu_open_ = false;
        return true;
    }

    bool moveWindow(uint32_t window_id, int32_t dx, int32_t dy) {
        if (window_id != terminal_window_.id || terminal_window_.state == DesktopWindowState::Closed) return false;
        terminal_window_.bounds.x = clamp_min(terminal_window_.bounds.x + dx, 0);
        terminal_window_.bounds.y = clamp_min(terminal_window_.bounds.y + dy, 38);
        focusTerminal();
        return true;
    }

    bool resizeWindow(uint32_t window_id, int32_t dx, int32_t dy) {
        if (window_id != terminal_window_.id || terminal_window_.state == DesktopWindowState::Closed) return false;
        terminal_window_.bounds.width = clamp_min(terminal_window_.bounds.width + dx, MinWindowWidth);
        terminal_window_.bounds.height = clamp_min(terminal_window_.bounds.height + dy, MinWindowHeight);
        focusTerminal();
        return true;
    }

    void toggleApplicationsMenu() {
        applications_menu_open_ = !applications_menu_open_;
    }

    void beginTerminalLaunch() {
        terminal_window_.state = DesktopWindowState::Loading;
        focusTerminal();
        applications_menu_open_ = false;
    }

    void terminalReady() {
        terminal_window_.state = DesktopWindowState::Running;
        focusTerminal();
    }

    bool handleEscape() {
        if (applications_menu_open_) {
            applications_menu_open_ = false;
            return true;
        }
        if (terminal_window_.state != DesktopWindowState::Closed && terminal_window_.focused) {
            return closeWindow(terminal_window_.id);
        }
        return false;
    }

private:
    static int32_t clamp_min(int32_t value, int32_t minimum) {
        return value < minimum ? minimum : value;
    }

    void focusTerminal() {
        terminal_window_.focused = true;
        terminal_window_.z = next_z_++;
    }

    bool applications_menu_open_ = false;
    uint32_t next_z_ = 2;
    DesktopWindow terminal_window_{};
};

char g_terminal_text[MaxShellText];
int g_slint_started = 0;
int g_boot_marker_sent = 0;
int g_loading_marker_sent = 0;
int g_ready_marker_sent = 0;
int g_wm_marker_sent = 0;
int g_terminal_window_marker_sent = 0;
int g_menu_open_marker_sent = 0;
int g_menu_escape_marker_sent = 0;
int g_window_move_marker_sent = 0;
int g_window_resize_marker_sent = 0;
int g_terminal_close_marker_sent = 0;
int g_terminal_relaunch_marker_sent = 0;
int g_compositor_surface_marker_sent = 0;
int g_compositor_offscreen_marker_sent = 0;
int g_compositor_blit_marker_sent = 0;
int g_compositor_hit_marker_sent = 0;
int g_compositor_input_marker_sent = 0;
int g_compositor_z_marker_sent = 0;
int g_compositor_alpha_marker_sent = 0;
int g_compositor_damage_marker_sent = 0;
int g_compositor_copy_forward_marker_sent = 0;
int g_compositor_exposed_marker_sent = 0;
int g_compositor_empty_marker_sent = 0;
int g_compositor_ipc_marker_sent = 0;
int g_framebuffer_dirty_present_marker_sent = 0;
int g_shm_process_marker_sent = 0;
int g_shell_smoke_actions_done = 0;
rad_pty_t g_terminal_pty = nullptr;
int32_t g_terminal_pid = 0;
rad_task_t g_terminal_task = nullptr;
EmbeddedDesktopShellModel g_desktop;
uint32_t g_desktop_surface_width = MaxSurfaceWidth;
uint32_t g_desktop_surface_height = MaxSurfaceHeight;

alignas(16) uint32_t g_desktop_pixels[MaxSurfaceWidth * MaxSurfaceHeight];
alignas(16) uint32_t g_terminal_pixels[MaxSurfaceWidth * MaxSurfaceHeight];
alignas(16) uint32_t g_present_front_pixels[MaxSurfaceWidth * MaxSurfaceHeight];
alignas(16) uint32_t g_present_back_pixels[MaxSurfaceWidth * MaxSurfaceHeight];
uint32_t *g_present_front = g_present_front_pixels;
uint32_t *g_present_back = g_present_back_pixels;
rad_compositor_t g_compositor{};
uint32_t g_desktop_surface_id = 0;
uint32_t g_terminal_surface_id = 0;

slint::ComponentHandle<RadDesktopSurface> *g_desktop_shell = nullptr;
slint::ComponentHandle<RadTerminalSurface> *g_terminal_shell = nullptr;

slint::SharedString shared_string(const char *text) {
    if (!text) text = "";
    return slint::SharedString(std::string_view(text, strlen(text)));
}

void copy_terminal_text(const char *text) {
    if (!text) text = "";
    strncpy(g_terminal_text, text, sizeof(g_terminal_text) - 1u);
    g_terminal_text[sizeof(g_terminal_text) - 1u] = '\0';
}

void append_terminal_text(const char *text, size_t size) {
    if (!text || !size) return;
    const size_t current = strlen(g_terminal_text);
    if (current >= sizeof(g_terminal_text) - 1u) return;
    size_t copy = size;
    if (copy > sizeof(g_terminal_text) - 1u - current) copy = sizeof(g_terminal_text) - 1u - current;
    memcpy(g_terminal_text + current, text, copy);
    g_terminal_text[current + copy] = '\0';
}

void marker_once(int *flag, const char *marker) {
    if (!flag || *flag || !marker) return;
    *flag = 1;
    rad_debug_marker(marker);
}

uint32_t clamp_surface_extent(uint32_t value, uint32_t maximum) {
    if (value == 0) return 1u;
    return value > maximum ? maximum : value;
}

rad_status_t compositor_device_ioctl(void*, uint32_t request, void *argument) {
    if (request == RAD_DEVICE_IOCTL_COMPOSITOR_CREATE_SURFACE) {
        auto *surface = static_cast<rad_compositor_ipc_surface_t*>(argument);
        if (!surface || surface->size < sizeof(*surface) || surface->width == 0 || surface->height == 0) return RAD_STATUS_INVALID_ARGUMENT;
        uint32_t *pixels = static_cast<uint32_t*>(rad_shm_kernel_pointer(surface->shm_fd));
        if (!pixels) return RAD_STATUS_INVALID_ARGUMENT;
        rad_compositor_surface_config_t config{};
        config.size = sizeof(config);
        config.app_id = "rad.ipc";
        config.title = "IPC Surface";
        config.x = surface->x;
        config.y = surface->y;
        config.width = static_cast<int32_t>(surface->width);
        config.height = static_cast<int32_t>(surface->height);
        config.z = surface->z;
        config.flags = surface->flags;
        config.pixels = pixels;
        config.stride_pixels = surface->stride_pixels;
        const rad_status_t status = rad_compositor_create_surface(&g_compositor, &config, &surface->surface_id);
        if (status == RAD_STATUS_OK) {
            marker_once(&g_compositor_ipc_marker_sent, "RADIX_COMPOSITOR_IPC_SURFACE_OK");
        }
        return status;
    }
    if (request == RAD_DEVICE_IOCTL_COMPOSITOR_QUEUE_DAMAGE) {
        auto *damage = static_cast<rad_compositor_ipc_damage_t*>(argument);
        if (!damage || damage->size < sizeof(*damage)) return RAD_STATUS_INVALID_ARGUMENT;
        rad_compositor_rect_t rect{};
        rect.x = damage->x;
        rect.y = damage->y;
        rect.width = damage->width;
        rect.height = damage->height;
        const rad_status_t status = rad_compositor_queue_damage(&g_compositor, damage->surface_id, &rect, damage->flags);
        if (status == RAD_STATUS_OK) {
            marker_once(&g_compositor_damage_marker_sent, "RADIX_COMPOSITOR_DAMAGE_QUEUE_OK");
            if (damage->flags & RAD_COMPOSITOR_DAMAGE_EXPOSED) {
                marker_once(&g_compositor_exposed_marker_sent, "RADIX_COMPOSITOR_EXPOSED_DAMAGE_OK");
            }
        }
        return status;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

void register_compositor_device() {
    rad_device_ops_t ops{};
    ops.ioctl = compositor_device_ioctl;
    rad_device_register("/dev/compositor0", RAD_DEVICE_COMPOSITOR, &ops);
}

void drain_terminal_pty(void) {
    if (!g_terminal_pty || !g_terminal_shell) return;
    char buffer[256];
    size_t count = 0;
    int changed = 0;
    while (rad_pty_read_master(g_terminal_pty, buffer, sizeof(buffer), &count) == RAD_STATUS_OK && count > 0) {
        append_terminal_text(buffer, count);
        changed = 1;
    }
    if (changed) {
        (*g_terminal_shell)->set_terminal(shared_string(g_terminal_text));
        marker_once(&g_ready_marker_sent, "RADIX_SLINT_TERMINAL_READY_OK");
    }
}

void write_terminal_input(const char *text, size_t size) {
    const DesktopWindow *window = g_desktop.terminalWindow();
    if (!g_terminal_pty || !text || !size || !window || window->state == DesktopWindowState::Closed || !window->focused) return;
    size_t written = 0;
    rad_pty_write_master(g_terminal_pty, text, size, &written);
}

class RadixSlintWindowAdapter final : public slint::platform::WindowAdapter {
public:
    RadixSlintWindowAdapter(SlintSurfaceRole role, uint32_t surface_id, uint32_t *pixels, uint32_t width, uint32_t height, uint32_t stride_pixels)
        : role_(role),
          surface_id_(surface_id),
          pixels_(pixels),
          stride_pixels_(stride_pixels),
          renderer_(slint::platform::SoftwareRenderer::RepaintBufferType::ReusedBuffer),
          size_({ width, height }) {}

    slint::platform::AbstractRenderer& renderer() override {
        return renderer_;
    }

    slint::PhysicalSize size() override {
        return size_;
    }

    void set_size(slint::PhysicalSize) override {
        dispatch_resize();
    }

    void set_visible(bool visible) override {
        if (visible) {
            dispatch_resize();
            needs_redraw_ = true;
        }
    }

    void request_redraw() override {
        needs_redraw_ = true;
    }

    uint32_t surface_id() const {
        return surface_id_;
    }

    SlintSurfaceRole role() const {
        return role_;
    }

    void update_size(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0 || width > MaxSurfaceWidth || height > MaxSurfaceHeight) return;
        size_ = slint::PhysicalSize({ width, height });
        dispatch_resize();
        request_redraw();
    }

    rad_status_t render_if_needed(bool *rendered) {
        if (rendered) *rendered = false;
        if (!needs_redraw_) return RAD_STATUS_OK;
        needs_redraw_ = false;
        if (!pixels_ || stride_pixels_ < size_.width) return RAD_STATUS_NOT_SUPPORTED;

        renderer_.render_by_line<slint::Rgb8Pixel>(
            [&](std::size_t line, std::size_t begin, std::size_t end, auto render_line) {
                if (line >= size_.height || begin >= end || end > size_.width) return;
                const std::size_t count = end - begin;
                if (count > sizeof(line_buffer_) / sizeof(line_buffer_[0])) return;
                std::span<slint::Rgb8Pixel> span(line_buffer_, count);
                render_line(span);
                auto *dst = pixels_ + (line * stride_pixels_) + begin;
                for (std::size_t i = 0; i < count; ++i) {
                    const auto& src = line_buffer_[i];
                    dst[i] = 0xff000000u
                        | (uint32_t(src.r) << 16u)
                        | (uint32_t(src.g) << 8u)
                        | uint32_t(src.b);
                }
            });
        rad_compositor_mark_surface_dirty(&g_compositor, surface_id_);
        marker_once(&g_compositor_offscreen_marker_sent, "RADIX_COMPOSITOR_OFFSCREEN_RENDER_OK");
        if (rendered) *rendered = true;
        return RAD_STATUS_OK;
    }

    void dispatch_input_event(const rad_input_event_t& event, int32_t local_x = 0, int32_t local_y = 0) {
        switch (event.type) {
        case RAD_INPUT_EVENT_KEY:
            dispatch_key_event(event);
            break;
        case RAD_INPUT_EVENT_POINTER_MOTION:
            window().dispatch_pointer_move_event(pointer_position(event, local_x, local_y));
            request_redraw();
            break;
        case RAD_INPUT_EVENT_POINTER_BUTTON:
            if (event.pressed) {
                window().dispatch_pointer_press_event(pointer_position(event, local_x, local_y), pointer_button(event.code));
            } else {
                window().dispatch_pointer_release_event(pointer_position(event, local_x, local_y), pointer_button(event.code));
            }
            request_redraw();
            break;
        case RAD_INPUT_EVENT_POINTER_SCROLL:
            window().dispatch_pointer_scroll_event(pointer_position(event, local_x, local_y),
                static_cast<float>(event.scroll_x),
                static_cast<float>(event.scroll_y));
            request_redraw();
            break;
        default:
            break;
        }
    }

private:
    void dispatch_resize() {
        window().dispatch_resize_event(slint::LogicalSize({ float(size_.width), float(size_.height) }));
    }

    slint::SharedString key_text(const rad_input_event_t& event) {
        switch (event.code) {
        case RAD_INPUT_KEY_ESCAPE: return shared_string("\x1b");
        case RAD_INPUT_KEY_ENTER: return shared_string("\r");
        case RAD_INPUT_KEY_BACKSPACE: return shared_string("\x08");
        case RAD_INPUT_KEY_TAB: return shared_string("\t");
        default:
            break;
        }
        if (event.codepoint >= 0x20u && event.codepoint <= 0x7eu) {
            char text[2] = { static_cast<char>(event.codepoint), '\0' };
            return shared_string(text);
        }
        return shared_string("");
    }

    void dispatch_key_event(const rad_input_event_t& event) {
        const slint::SharedString text = key_text(event);
        if (text.empty()) return;
        if (event.pressed) {
            char input[2] = {};
            size_t input_size = 0;
            if (event.code == RAD_INPUT_KEY_ENTER) {
                input[0] = '\n';
                input_size = 1;
            } else if (event.code == RAD_INPUT_KEY_BACKSPACE) {
                input[0] = '\b';
                input_size = 1;
            } else if (event.code == RAD_INPUT_KEY_TAB) {
                input[0] = '\t';
                input_size = 1;
            } else if (event.codepoint >= 0x20u && event.codepoint <= 0x7eu) {
                input[0] = static_cast<char>(event.codepoint);
                input_size = 1;
            }
            write_terminal_input(input, input_size);
        }
        if (event.pressed) {
            if (event.repeat) {
                window().dispatch_key_press_repeat_event(text);
            } else {
                window().dispatch_key_press_event(text);
            }
        } else {
            window().dispatch_key_release_event(text);
        }
        request_redraw();
    }

    slint::LogicalPosition pointer_position(const rad_input_event_t&, int32_t local_x, int32_t local_y) const {
        float x = static_cast<float>(local_x);
        float y = static_cast<float>(local_y);
        if (x < 0.0f) x = 0.0f;
        if (y < 0.0f) y = 0.0f;
        if (x >= static_cast<float>(size_.width)) x = static_cast<float>(size_.width - 1u);
        if (y >= static_cast<float>(size_.height)) y = static_cast<float>(size_.height - 1u);
        return slint::LogicalPosition(slint::Point<float>{ x, y });
    }

    slint::PointerEventButton pointer_button(uint32_t button) const {
        if (button == RAD_INPUT_POINTER_BUTTON_RIGHT) return slint::PointerEventButton::Right;
        if (button == RAD_INPUT_POINTER_BUTTON_MIDDLE) return slint::PointerEventButton::Middle;
        return slint::PointerEventButton::Left;
    }

    SlintSurfaceRole role_ = SlintSurfaceRole::Desktop;
    uint32_t surface_id_ = 0;
    uint32_t *pixels_ = nullptr;
    uint32_t stride_pixels_ = 0;
    slint::platform::SoftwareRenderer renderer_;
    slint::PhysicalSize size_{};
    bool needs_redraw_ = true;
    slint::Rgb8Pixel line_buffer_[4096]{};
};

class RadixSlintPlatform final : public slint::platform::Platform {
public:
    RadixSlintPlatform(rad_framebuffer_t framebuffer, const rad_framebuffer_info_t& info)
        : framebuffer_(framebuffer), info_(info) {
        if (rad_input_open("/dev/input/event0", &keyboard_) != RAD_STATUS_OK) keyboard_ = nullptr;
        if (rad_input_open("/dev/input/event1", &pointer_) != RAD_STATUS_OK) pointer_ = nullptr;
        started_ms_ = rad_time_millis();
    }

    void set_next_role(SlintSurfaceRole role) {
        next_role_ = role;
    }

    std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter() override {
        if (next_role_ == SlintSurfaceRole::Terminal) {
            const DesktopWindow *terminal = g_desktop.terminalWindow();
            const uint32_t width = terminal && terminal->bounds.width > 0 ? static_cast<uint32_t>(terminal->bounds.width) : 640u;
            const uint32_t height = terminal && terminal->bounds.height > 0 ? static_cast<uint32_t>(terminal->bounds.height) : 380u;
            auto window = std::make_unique<RadixSlintWindowAdapter>(SlintSurfaceRole::Terminal, g_terminal_surface_id, g_terminal_pixels, width, height, MaxSurfaceWidth);
            terminal_window_ = window.get();
            next_role_ = SlintSurfaceRole::Desktop;
            return window;
        }
        auto window = std::make_unique<RadixSlintWindowAdapter>(SlintSurfaceRole::Desktop, g_desktop_surface_id, g_desktop_pixels, g_desktop_surface_width, g_desktop_surface_height, MaxSurfaceWidth);
        desktop_window_ = window.get();
        return window;
    }

    std::chrono::milliseconds duration_since_start() override {
        return std::chrono::milliseconds(rad_time_millis() - started_ms_);
    }

    void run_event_loop() override {
        for (;;) {
            tick();
            rad_sleep_ms(16);
        }
    }

    void run_in_event_loop(Task task) override {
        std::move(task).run();
    }

    rad_status_t tick() {
        slint::platform::update_timers_and_animations();
        poll_input_device(keyboard_);
        poll_input_device(pointer_);
        drain_terminal_pty();
        bool rendered = false;
        bool any_rendered = false;
        if (desktop_window_) {
            const rad_status_t status = desktop_window_->render_if_needed(&rendered);
            if (status != RAD_STATUS_OK) return status;
            any_rendered = any_rendered || rendered;
            if (desktop_window_->window().has_active_animations()) desktop_window_->request_redraw();
        }
        if (terminal_window_) {
            const rad_status_t status = terminal_window_->render_if_needed(&rendered);
            if (status != RAD_STATUS_OK) return status;
            any_rendered = any_rendered || rendered;
            if (terminal_window_->window().has_active_animations()) terminal_window_->request_redraw();
        }
        rad_compositor_set_framebuffers(&g_compositor, g_present_front, MaxSurfaceWidth, g_present_back, MaxSurfaceWidth);
        if (rad_compositor_compose_frame(&g_compositor) != RAD_STATUS_OK) return RAD_STATUS_ERROR;
        if (g_compositor.last_present_rect_count == 0) {
            marker_once(&g_compositor_empty_marker_sent, "RADIX_COMPOSITOR_EMPTY_FRAME_SKIP_OK");
        } else {
            marker_once(&g_compositor_copy_forward_marker_sent, "RADIX_COMPOSITOR_COPY_FORWARD_OK");
            marker_once(&g_compositor_blit_marker_sent, "RADIX_COMPOSITOR_BLIT_OK");
            for (uint32_t i = 0; i < g_compositor.last_present_rect_count; ++i) {
                const rad_compositor_rect_t& dirty = g_compositor.last_present_rects[i];
                rad_framebuffer_present_t present{};
                present.size = sizeof(present);
                present.pixels = g_present_back;
                present.stride_bytes = MaxSurfaceWidth * sizeof(uint32_t);
                present.rect.x = static_cast<uint32_t>(dirty.x);
                present.rect.y = static_cast<uint32_t>(dirty.y);
                present.rect.width = static_cast<uint32_t>(dirty.width);
                present.rect.height = static_cast<uint32_t>(dirty.height);
                const rad_status_t status = rad_framebuffer_present(framebuffer_, &present);
                if (status != RAD_STATUS_OK && status != RAD_STATUS_NOT_SUPPORTED) return status;
            }
            marker_once(&g_framebuffer_dirty_present_marker_sent, "RADIX_FRAMEBUFFER_DIRTY_PRESENT_OK");
            uint32_t *tmp = g_present_front;
            g_present_front = g_present_back;
            g_present_back = tmp;
        }
        if (any_rendered && !g_boot_marker_sent) {
            marker_once(&g_boot_marker_sent, "RADIX_SLINT_BOOT_SHELL_OK");
        }
        return RAD_STATUS_OK;
    }

    void sync_terminal_bounds() {
        if (!terminal_window_) return;
        const DesktopWindow *terminal = g_desktop.terminalWindow();
        if (!terminal) return;
        rad_compositor_rect_t bounds{};
        bounds.x = terminal->bounds.x;
        bounds.y = terminal->bounds.y;
        bounds.width = terminal->bounds.width;
        bounds.height = terminal->bounds.height;
        rad_compositor_set_surface_bounds(&g_compositor, g_terminal_surface_id, &bounds);
        terminal_window_->update_size(static_cast<uint32_t>(bounds.width), static_cast<uint32_t>(bounds.height));
    }

private:
    void poll_input_device(rad_device_t device) {
        if (!device) return;
        rad_input_event_t event{};
        while (rad_input_read_event(device, &event) == RAD_STATUS_OK) {
            rad_compositor_input_result_t result{};
            if (rad_compositor_dispatch_input(&g_compositor, &event, &result) != RAD_STATUS_OK || !result.hit) continue;
            RadixSlintWindowAdapter *target = adapter_for_surface(result.surface_id);
            if (!target) continue;
            marker_once(&g_compositor_hit_marker_sent, "RADIX_COMPOSITOR_HIT_TEST_OK");
            if (event.type == RAD_INPUT_EVENT_POINTER_MOTION
                || event.type == RAD_INPUT_EVENT_POINTER_BUTTON
                || event.type == RAD_INPUT_EVENT_POINTER_SCROLL) {
                marker_once(&g_compositor_input_marker_sent, "RADIX_COMPOSITOR_INPUT_TRANSLATE_OK");
            }
            target->dispatch_input_event(event, result.local_x, result.local_y);
        }
    }

    RadixSlintWindowAdapter *adapter_for_surface(uint32_t surface_id) {
        if (desktop_window_ && desktop_window_->surface_id() == surface_id) return desktop_window_;
        if (terminal_window_ && terminal_window_->surface_id() == surface_id) return terminal_window_;
        return nullptr;
    }

    rad_framebuffer_t framebuffer_ = nullptr;
    rad_framebuffer_info_t info_{};
    rad_device_t keyboard_ = nullptr;
    rad_device_t pointer_ = nullptr;
    uint64_t started_ms_ = 0;
    SlintSurfaceRole next_role_ = SlintSurfaceRole::Desktop;
    RadixSlintWindowAdapter *desktop_window_ = nullptr;
    RadixSlintWindowAdapter *terminal_window_ = nullptr;
};

RadixSlintPlatform *g_platform = nullptr;

const char *shell_status_text() {
    switch (g_desktop.terminalState()) {
    case TerminalAppState::Launching:
        return "framebuffer=primary shell=radlib state=terminal launching";
    case TerminalAppState::Running:
        return "framebuffer=primary shell=radlib state=terminal";
    case TerminalAppState::Closed:
    default:
        return g_desktop.applicationsMenuOpen()
            ? "framebuffer=primary shell=radlib state=applications menu"
            : "framebuffer=primary shell=radlib state=desktop";
    }
}

void set_shell_state(const char *status = nullptr) {
    if (!g_desktop_shell || !g_terminal_shell) return;
    const DesktopWindow *terminal = g_desktop.terminalWindow();
    (*g_desktop_shell)->set_surface_width(static_cast<float>(g_desktop_surface_width));
    (*g_desktop_shell)->set_surface_height(static_cast<float>(g_desktop_surface_height));
    (*g_desktop_shell)->set_backend(shared_string("x86_64_grub / RADix"));
    (*g_desktop_shell)->set_status(shared_string(status ? status : shell_status_text()));
    (*g_desktop_shell)->set_applications_open(g_desktop.applicationsMenuOpen());
    (*g_terminal_shell)->set_terminal(shared_string(g_terminal_text));
    (*g_terminal_shell)->set_terminal_loading(g_desktop.terminalLaunching());
    if (terminal) {
        (*g_terminal_shell)->set_terminal_window_title(shared_string(terminal->title));
        (*g_terminal_shell)->set_terminal_window_focused(terminal->focused);
        (*g_terminal_shell)->set_surface_width(static_cast<float>(terminal->bounds.width));
        (*g_terminal_shell)->set_surface_height(static_cast<float>(terminal->bounds.height));
        rad_compositor_set_surface_visible(&g_compositor, g_terminal_surface_id, terminal->state != DesktopWindowState::Closed);
        if (g_platform) g_platform->sync_terminal_bounds();
    }
}

void render_ticks(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (g_platform) g_platform->tick();
    }
}

void launch_terminal(const char *terminal_text) {
    g_desktop.beginTerminalLaunch();
    set_shell_state();
    render_ticks(2);
    marker_once(&g_loading_marker_sent, "RADIX_SLINT_TERMINAL_LOADING_OK");
    if (g_terminal_pty) {
        copy_terminal_text(terminal_text && *terminal_text ? terminal_text : g_terminal_text);
        g_desktop.terminalReady();
        set_shell_state();
        render_ticks(2);
        return;
    }
    copy_terminal_text("");

    char slave_name[64]{};
    rad_status_t status = rad_pty_open_pair("slint-terminal", &g_terminal_pty);
    if (status == RAD_STATUS_OK) {
        status = rad_pty_slave_name(g_terminal_pty, slave_name, sizeof(slave_name));
    }
    if (status == RAD_STATUS_OK) {
        size_t written = 0;
        rad_pty_write_master(g_terminal_pty, "x\n", 2, &written);
    }
    if (status == RAD_STATUS_OK && x86_user_spawn_process_with_stdio) {
        status = x86_user_spawn_process_with_stdio("/bin/radsh", rad_process_current_pid(), slave_name, &g_terminal_pid, &g_terminal_task);
        if (status == RAD_STATUS_OK) {
            rad_debug_marker("RADIX_SLINT_APP_LAUNCH_PROCESS_OK");
            rad_debug_marker("RADIX_SLINT_APP_LIVE_PTY_OK");
            append_terminal_text("RADix PTY terminal ready\n$ ", 27);
        }
    } else if (radix_shell_launch_terminal_process) {
        status = radix_shell_launch_terminal_process();
        if (status != RAD_STATUS_OK) {
            rad_debug_marker("RADIX_SLINT_APP_LAUNCH_PROCESS_FAIL");
        }
    }
    if (status != RAD_STATUS_OK) {
        rad_debug_marker("RADIX_SLINT_APP_LAUNCH_PROCESS_FAIL");
        if (g_terminal_pty) {
            rad_pty_close(g_terminal_pty);
            g_terminal_pty = nullptr;
        }
    }

    if (!g_terminal_text[0]) copy_terminal_text(terminal_text);
    g_desktop.terminalReady();
    set_shell_state();
    render_ticks(2);
    marker_once(&g_ready_marker_sent, "RADIX_SLINT_TERMINAL_READY_OK");
    marker_once(&g_wm_marker_sent, "RADIX_SLINT_WM_OK");
    marker_once(&g_terminal_window_marker_sent, "RADIX_SLINT_APP_TERMINAL_WINDOW_OK");
}

void close_terminal_model_only() {
    if (const DesktopWindow *window = g_desktop.terminalWindow()) {
        g_desktop.closeWindow(window->id);
        set_shell_state();
        marker_once(&g_terminal_close_marker_sent, "RADIX_SLINT_TERMINAL_CLOSE_OK");
    }
}

void run_compositor_input_smoke() {
    const DesktopWindow *window = g_desktop.terminalWindow();
    if (!window || window->state == DesktopWindowState::Closed) return;
    rad_input_event_t event{};
    event.size = sizeof(event);
    event.type = RAD_INPUT_EVENT_POINTER_BUTTON;
    event.code = RAD_INPUT_POINTER_BUTTON_LEFT;
    event.pressed = 1;
    event.x = window->bounds.x + 12;
    event.y = window->bounds.y + 10;
    event.buttons = RAD_INPUT_POINTER_BUTTON_LEFT;
    rad_compositor_input_result_t result{};
    if (rad_compositor_dispatch_input(&g_compositor, &event, &result) == RAD_STATUS_OK
        && result.hit
        && result.surface_id == g_terminal_surface_id
        && result.local_x == 12
        && result.local_y == 10) {
        marker_once(&g_compositor_hit_marker_sent, "RADIX_COMPOSITOR_HIT_TEST_OK");
        marker_once(&g_compositor_input_marker_sent, "RADIX_COMPOSITOR_INPUT_TRANSLATE_OK");
    }
}

void run_shell_smoke_actions() {
    if (g_shell_smoke_actions_done) return;
    g_shell_smoke_actions_done = 1;
    g_desktop.toggleApplicationsMenu();
    set_shell_state();
    marker_once(&g_menu_open_marker_sent, "RADIX_SLINT_MENU_OPEN_OK");
    if (g_desktop.handleEscape()) {
        set_shell_state();
        marker_once(&g_menu_escape_marker_sent, "RADIX_SLINT_MENU_ESCAPE_OK");
    }
    if (const DesktopWindow *window = g_desktop.terminalWindow()) {
        if (g_desktop.moveWindow(window->id, 16, 10)) {
            set_shell_state();
            marker_once(&g_window_move_marker_sent, "RADIX_SLINT_WINDOW_MOVE_OK");
        }
    }
    if (const DesktopWindow *window = g_desktop.terminalWindow()) {
        if (g_desktop.resizeWindow(window->id, 28, 18)) {
            set_shell_state();
            marker_once(&g_window_resize_marker_sent, "RADIX_SLINT_WINDOW_RESIZE_OK");
        }
    }
    close_terminal_model_only();
    launch_terminal(g_terminal_text);
    run_compositor_input_smoke();
    marker_once(&g_terminal_relaunch_marker_sent, "RADIX_SLINT_TERMINAL_RELAUNCH_OK");
}

void run_compositor_alpha_smoke() {
    uint32_t target[4]{};
    uint32_t back[4] = {0xff0000ffu, 0xff0000ffu, 0xff0000ffu, 0xff0000ffu};
    uint32_t blend[4] = {0x80ff0000u, 0x80ff0000u, 0x80ff0000u, 0x80ff0000u};
    rad_compositor_t compositor{};
    if (rad_compositor_init(&compositor, target, 2, 2, 2) != RAD_STATUS_OK) return;
    uint32_t back_id = 0;
    rad_compositor_surface_config_t back_config{};
    back_config.size = sizeof(back_config);
    back_config.width = 2;
    back_config.height = 2;
    back_config.pixels = back;
    back_config.stride_pixels = 2;
    rad_compositor_create_surface(&compositor, &back_config, &back_id);
    uint32_t blend_id = 0;
    rad_compositor_surface_config_t blend_config{};
    blend_config.size = sizeof(blend_config);
    blend_config.width = 2;
    blend_config.height = 2;
    blend_config.z = 1;
    blend_config.flags = RAD_COMPOSITOR_SURFACE_ALPHA;
    blend_config.pixels = blend;
    blend_config.stride_pixels = 2;
    if (rad_compositor_create_surface(&compositor, &blend_config, &blend_id) == RAD_STATUS_OK
        && rad_compositor_compose_frame(&compositor) == RAD_STATUS_OK
        && target[0] != back[0]
        && target[0] != blend[0]) {
        marker_once(&g_compositor_alpha_marker_sent, "RADIX_COMPOSITOR_ALPHA_OK");
    }
}

void run_ipc_surface_smoke_process() {
    if (!x86_user_spawn_process || !x86_user_wait_process) return;
    int32_t pid = 0;
    rad_task_t task = nullptr;
    if (x86_user_spawn_process("/bin/radgfx-smoke", rad_process_current_pid(), &pid, &task) != RAD_STATUS_OK) return;
    if (x86_user_wait_process(pid)) {
        marker_once(&g_shm_process_marker_sent, "RADIX_SHM_PROCESS_IPC_OK");
        render_ticks(3);
    }
}

} // namespace

extern "C" rad_status_t radix_slint_shell_start(rad_framebuffer_t framebuffer, const char *terminal_text) {
    if (g_slint_started) return RAD_STATUS_OK;
    if (!framebuffer) return RAD_STATUS_INVALID_ARGUMENT;

    rad_framebuffer_info_t info{};
    rad_status_t status = rad_framebuffer_get_info(framebuffer, &info);
    if (status != RAD_STATUS_OK) return status;
    if (!info.pixels || info.pixel_format != RAD_PIXEL_FORMAT_XRGB8888 || info.width == 0 || info.height == 0) {
        return RAD_STATUS_NOT_SUPPORTED;
    }

    memset(g_desktop_pixels, 0, sizeof(g_desktop_pixels));
    memset(g_terminal_pixels, 0, sizeof(g_terminal_pixels));
    memset(g_present_front_pixels, 0x1f, sizeof(g_present_front_pixels));
    memset(g_present_back_pixels, 0x1f, sizeof(g_present_back_pixels));
    g_present_front = g_present_front_pixels;
    g_present_back = g_present_back_pixels;
    g_desktop_surface_width = clamp_surface_extent(info.width, MaxSurfaceWidth);
    g_desktop_surface_height = clamp_surface_extent(info.height, MaxSurfaceHeight);
    status = rad_compositor_init(&g_compositor, g_present_back, g_desktop_surface_width, g_desktop_surface_height, MaxSurfaceWidth);
    if (status != RAD_STATUS_OK) return status;
    rad_compositor_set_framebuffers(&g_compositor, g_present_front, MaxSurfaceWidth, g_present_back, MaxSurfaceWidth);
    register_compositor_device();
    rad_compositor_surface_config_t desktop_config{};
    desktop_config.size = sizeof(desktop_config);
    desktop_config.app_id = "rad.desktop";
    desktop_config.title = "RAD Desktop";
    desktop_config.width = static_cast<int32_t>(g_desktop_surface_width);
    desktop_config.height = static_cast<int32_t>(g_desktop_surface_height);
    desktop_config.z = 0;
    desktop_config.pixels = g_desktop_pixels;
    desktop_config.stride_pixels = MaxSurfaceWidth;
    status = rad_compositor_create_surface(&g_compositor, &desktop_config, &g_desktop_surface_id);
    if (status != RAD_STATUS_OK) return status;
    const DesktopWindow *terminal_window = g_desktop.terminalWindow();
    rad_compositor_surface_config_t terminal_config{};
    terminal_config.size = sizeof(terminal_config);
    terminal_config.app_id = "rad.terminal";
    terminal_config.title = "Terminal";
    terminal_config.x = terminal_window ? terminal_window->bounds.x : 134;
    terminal_config.y = terminal_window ? terminal_window->bounds.y : 110;
    terminal_config.width = terminal_window ? terminal_window->bounds.width : 640;
    terminal_config.height = terminal_window ? terminal_window->bounds.height : 380;
    terminal_config.z = 10;
    terminal_config.pixels = g_terminal_pixels;
    terminal_config.stride_pixels = MaxSurfaceWidth;
    status = rad_compositor_create_surface(&g_compositor, &terminal_config, &g_terminal_surface_id);
    if (status != RAD_STATUS_OK) return status;
    rad_compositor_focus_surface(&g_compositor, g_terminal_surface_id);
    marker_once(&g_compositor_surface_marker_sent, "RADIX_COMPOSITOR_SURFACE_CREATE_OK");
    marker_once(&g_compositor_z_marker_sent, "RADIX_COMPOSITOR_Z_ORDER_OK");
    run_compositor_alpha_smoke();

    auto *platform = new RadixSlintPlatform(framebuffer, info);
    g_platform = platform;
    slint::platform::set_platform(std::unique_ptr<slint::platform::Platform>(platform));
    platform->set_next_role(SlintSurfaceRole::Desktop);
    g_desktop_shell = new slint::ComponentHandle<RadDesktopSurface>(RadDesktopSurface::create());
    platform->set_next_role(SlintSurfaceRole::Terminal);
    g_terminal_shell = new slint::ComponentHandle<RadTerminalSurface>(RadTerminalSurface::create());
    (*g_desktop_shell)->on_toggle_applications([]() {
        if (!g_desktop_shell) return;
        g_desktop.toggleApplicationsMenu();
        set_shell_state();
        if (g_desktop.applicationsMenuOpen()) marker_once(&g_menu_open_marker_sent, "RADIX_SLINT_MENU_OPEN_OK");
    });
    (*g_desktop_shell)->on_launch_terminal([]() {
        launch_terminal(g_terminal_text);
    });
    (*g_desktop_shell)->on_escape_pressed([]() {
        if (!g_desktop_shell) return;
        const bool menu_was_open = g_desktop.applicationsMenuOpen();
        if (g_desktop.handleEscape()) {
            set_shell_state();
            if (menu_was_open) marker_once(&g_menu_escape_marker_sent, "RADIX_SLINT_MENU_ESCAPE_OK");
            else marker_once(&g_terminal_close_marker_sent, "RADIX_SLINT_TERMINAL_CLOSE_OK");
        }
    });
    (*g_terminal_shell)->on_escape_pressed([]() {
        const bool menu_was_open = g_desktop.applicationsMenuOpen();
        if (g_desktop.handleEscape()) {
            set_shell_state();
            if (menu_was_open) marker_once(&g_menu_escape_marker_sent, "RADIX_SLINT_MENU_ESCAPE_OK");
            else marker_once(&g_terminal_close_marker_sent, "RADIX_SLINT_TERMINAL_CLOSE_OK");
        }
    });
    (*g_terminal_shell)->on_focus_terminal_window([]() {
        if (const DesktopWindow *window = g_desktop.terminalWindow()) {
            g_desktop.focusWindow(window->id);
            rad_compositor_focus_surface(&g_compositor, g_terminal_surface_id);
            marker_once(&g_compositor_z_marker_sent, "RADIX_COMPOSITOR_Z_ORDER_OK");
        }
        set_shell_state();
    });
    (*g_terminal_shell)->on_close_terminal_window([]() {
        close_terminal_model_only();
    });
    (*g_terminal_shell)->on_move_terminal_window([](float dx, float dy) {
        if (const DesktopWindow *window = g_desktop.terminalWindow()) {
            if (g_desktop.moveWindow(window->id, static_cast<int32_t>(dx), static_cast<int32_t>(dy))) {
                marker_once(&g_window_move_marker_sent, "RADIX_SLINT_WINDOW_MOVE_OK");
            }
        }
        set_shell_state();
    });
    (*g_terminal_shell)->on_resize_terminal_window([](float dx, float dy) {
        if (const DesktopWindow *window = g_desktop.terminalWindow()) {
            if (g_desktop.resizeWindow(window->id, static_cast<int32_t>(dx), static_cast<int32_t>(dy))) {
                marker_once(&g_window_resize_marker_sent, "RADIX_SLINT_WINDOW_RESIZE_OK");
            }
        }
        set_shell_state();
    });
    set_shell_state("framebuffer=primary shell=radlib state=desktop");
    (*g_desktop_shell)->show();
    (*g_terminal_shell)->show();
    g_slint_started = 1;
    render_ticks(2);
    launch_terminal(terminal_text);
    run_shell_smoke_actions();
    run_ipc_surface_smoke_process();
    render_ticks(2);
    return RAD_STATUS_OK;
}

extern "C" void radix_slint_shell_set_terminal_text(const char *terminal_text) {
    if (!g_slint_started) return;
    copy_terminal_text(terminal_text);
    if (g_desktop.terminalOpen()) g_desktop.terminalReady();
    set_shell_state();
}

extern "C" void radix_slint_shell_poll(void) {
    if (!g_slint_started || !g_platform) return;
    g_platform->tick();
}

extern "C" int radix_slint_shell_ready(void) {
    return g_slint_started && g_ready_marker_sent;
}
