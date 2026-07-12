#include "slint_shell.h"

#include <radixkernel/rad_display.h>
#include <radixkernel/rad_input.h>
#include <radixkernel/rad_pty.h>

#include <memory>
#include <span>
#include <string_view>
#include <utility>

#include <slint-platform.h>

#include "shell.h"

#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_write(const char *text);
extern "C" rad_status_t radix_shell_launch_terminal_process(void) __attribute__((weak));
extern "C" rad_status_t x86_user_spawn_process_with_stdio(const char *path, int32_t parent_pid, const char *stdio_path, int32_t *pid_out, rad_task_t *task_out) __attribute__((weak));

namespace {

constexpr uint32_t MaxShellText = 8192u;
constexpr uint32_t TerminalWindowId = 1u;
constexpr int32_t MinWindowWidth = 180;
constexpr int32_t MinWindowHeight = 112;

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
    int32_t x = 118;
    int32_t y = 96;
    int32_t width = 368;
    int32_t height = 200;
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
int g_shell_smoke_actions_done = 0;
rad_pty_t g_terminal_pty = nullptr;
int32_t g_terminal_pid = 0;
rad_task_t g_terminal_task = nullptr;
EmbeddedDesktopShellModel g_desktop;

slint::ComponentHandle<RadOsShell> *g_shell = nullptr;

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

void drain_terminal_pty(void) {
    if (!g_terminal_pty || !g_shell) return;
    char buffer[256];
    size_t count = 0;
    int changed = 0;
    while (rad_pty_read_master(g_terminal_pty, buffer, sizeof(buffer), &count) == RAD_STATUS_OK && count > 0) {
        append_terminal_text(buffer, count);
        changed = 1;
    }
    if (changed) {
        (*g_shell)->set_terminal(shared_string(g_terminal_text));
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
    RadixSlintWindowAdapter(rad_framebuffer_t framebuffer, const rad_framebuffer_info_t& info)
        : framebuffer_(framebuffer),
          info_(info),
          renderer_(slint::platform::SoftwareRenderer::RepaintBufferType::ReusedBuffer),
          size_({ info.width, info.height }) {}

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

    rad_status_t render_if_needed(bool *rendered) {
        if (rendered) *rendered = false;
        if (!needs_redraw_) return RAD_STATUS_OK;
        needs_redraw_ = false;
        if (!info_.pixels || info_.pixel_format != RAD_PIXEL_FORMAT_XRGB8888) {
            return RAD_STATUS_NOT_SUPPORTED;
        }

        const uint32_t pixel_stride = info_.stride_bytes / sizeof(uint32_t);
        auto *pixels = reinterpret_cast<uint32_t*>(info_.pixels);
        renderer_.render_by_line<slint::Rgb8Pixel>(
            [&](std::size_t line, std::size_t begin, std::size_t end, auto render_line) {
                if (line >= info_.height || begin >= end || end > info_.width) return;
                const std::size_t count = end - begin;
                if (count > sizeof(line_buffer_) / sizeof(line_buffer_[0])) return;
                std::span<slint::Rgb8Pixel> span(line_buffer_, count);
                render_line(span);
                auto *dst = pixels + (line * pixel_stride) + begin;
                for (std::size_t i = 0; i < count; ++i) {
                    const auto& src = line_buffer_[i];
                    dst[i] = 0xff000000u
                        | (uint32_t(src.r) << 16u)
                        | (uint32_t(src.g) << 8u)
                        | uint32_t(src.b);
                }
            });
        const rad_framebuffer_rect_t rect{0u, 0u, info_.width, info_.height};
        const rad_status_t flush = rad_framebuffer_flush(framebuffer_, &rect);
        if (flush != RAD_STATUS_OK && flush != RAD_STATUS_NOT_SUPPORTED) return flush;
        if (rendered) *rendered = true;
        return RAD_STATUS_OK;
    }

    void dispatch_input_event(const rad_input_event_t& event) {
        switch (event.type) {
        case RAD_INPUT_EVENT_KEY:
            dispatch_key_event(event);
            break;
        case RAD_INPUT_EVENT_POINTER_MOTION:
            window().dispatch_pointer_move_event(pointer_position(event));
            request_redraw();
            break;
        case RAD_INPUT_EVENT_POINTER_BUTTON:
            if (event.pressed) {
                window().dispatch_pointer_press_event(pointer_position(event), pointer_button(event.code));
            } else {
                window().dispatch_pointer_release_event(pointer_position(event), pointer_button(event.code));
            }
            request_redraw();
            break;
        case RAD_INPUT_EVENT_POINTER_SCROLL:
            window().dispatch_pointer_scroll_event(pointer_position(event),
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

    slint::LogicalPosition pointer_position(const rad_input_event_t& event) const {
        float x = static_cast<float>(event.x);
        float y = static_cast<float>(event.y);
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

    rad_framebuffer_t framebuffer_ = nullptr;
    rad_framebuffer_info_t info_{};
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

    std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter() override {
        auto window = std::make_unique<RadixSlintWindowAdapter>(framebuffer_, info_);
        window_ = window.get();
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
        if (!window_) return RAD_STATUS_OK;
        bool rendered = false;
        const rad_status_t status = window_->render_if_needed(&rendered);
        if (rendered && !g_boot_marker_sent) {
            marker_once(&g_boot_marker_sent, "RADIX_SLINT_BOOT_SHELL_OK");
        }
        if (window_->window().has_active_animations()) {
            window_->request_redraw();
        }
        return status;
    }

private:
    void poll_input_device(rad_device_t device) {
        if (!device || !window_) return;
        rad_input_event_t event{};
        while (rad_input_read_event(device, &event) == RAD_STATUS_OK) {
            window_->dispatch_input_event(event);
        }
    }

    rad_framebuffer_t framebuffer_ = nullptr;
    rad_framebuffer_info_t info_{};
    rad_device_t keyboard_ = nullptr;
    rad_device_t pointer_ = nullptr;
    uint64_t started_ms_ = 0;
    RadixSlintWindowAdapter *window_ = nullptr;
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
    if (!g_shell) return;
    const DesktopWindow *terminal = g_desktop.terminalWindow();
    (*g_shell)->set_backend(shared_string("x86_64_grub / RADix"));
    (*g_shell)->set_status(shared_string(status ? status : shell_status_text()));
    (*g_shell)->set_terminal(shared_string(g_terminal_text));
    (*g_shell)->set_applications_open(g_desktop.applicationsMenuOpen());
    (*g_shell)->set_terminal_open(g_desktop.terminalOpen());
    (*g_shell)->set_terminal_loading(g_desktop.terminalLaunching());
    if (terminal) {
        (*g_shell)->set_terminal_window_id(static_cast<int32_t>(terminal->id));
        (*g_shell)->set_terminal_window_title(shared_string(terminal->title));
        (*g_shell)->set_terminal_window_x(static_cast<float>(terminal->bounds.x));
        (*g_shell)->set_terminal_window_y(static_cast<float>(terminal->bounds.y));
        (*g_shell)->set_terminal_window_width(static_cast<float>(terminal->bounds.width));
        (*g_shell)->set_terminal_window_height(static_cast<float>(terminal->bounds.height));
        (*g_shell)->set_terminal_window_focused(terminal->focused);
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
    marker_once(&g_terminal_relaunch_marker_sent, "RADIX_SLINT_TERMINAL_RELAUNCH_OK");
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

    auto *platform = new RadixSlintPlatform(framebuffer, info);
    g_platform = platform;
    slint::platform::set_platform(std::unique_ptr<slint::platform::Platform>(platform));
    g_shell = new slint::ComponentHandle<RadOsShell>(RadOsShell::create());
    (*g_shell)->on_toggle_applications([]() {
        if (!g_shell) return;
        g_desktop.toggleApplicationsMenu();
        set_shell_state();
        if (g_desktop.applicationsMenuOpen()) marker_once(&g_menu_open_marker_sent, "RADIX_SLINT_MENU_OPEN_OK");
    });
    (*g_shell)->on_launch_terminal([]() {
        launch_terminal(g_terminal_text);
    });
    (*g_shell)->on_escape_pressed([]() {
        if (!g_shell) return;
        const bool menu_was_open = g_desktop.applicationsMenuOpen();
        if (g_desktop.handleEscape()) {
            set_shell_state();
            if (menu_was_open) marker_once(&g_menu_escape_marker_sent, "RADIX_SLINT_MENU_ESCAPE_OK");
            else marker_once(&g_terminal_close_marker_sent, "RADIX_SLINT_TERMINAL_CLOSE_OK");
        }
    });
    (*g_shell)->on_focus_terminal_window([]() {
        if (const DesktopWindow *window = g_desktop.terminalWindow()) {
            g_desktop.focusWindow(window->id);
        }
        set_shell_state();
    });
    (*g_shell)->on_close_terminal_window([]() {
        close_terminal_model_only();
    });
    (*g_shell)->on_move_terminal_window([](float dx, float dy) {
        if (const DesktopWindow *window = g_desktop.terminalWindow()) {
            if (g_desktop.moveWindow(window->id, static_cast<int32_t>(dx), static_cast<int32_t>(dy))) {
                marker_once(&g_window_move_marker_sent, "RADIX_SLINT_WINDOW_MOVE_OK");
            }
        }
        set_shell_state();
    });
    (*g_shell)->on_resize_terminal_window([](float dx, float dy) {
        if (const DesktopWindow *window = g_desktop.terminalWindow()) {
            if (g_desktop.resizeWindow(window->id, static_cast<int32_t>(dx), static_cast<int32_t>(dy))) {
                marker_once(&g_window_resize_marker_sent, "RADIX_SLINT_WINDOW_RESIZE_OK");
            }
        }
        set_shell_state();
    });
    set_shell_state("framebuffer=primary shell=radlib state=desktop");
    (*g_shell)->show();
    g_slint_started = 1;
    render_ticks(2);
    launch_terminal(terminal_text);
    run_shell_smoke_actions();
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
