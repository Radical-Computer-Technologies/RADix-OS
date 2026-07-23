/* radslint-demo - a userland application that renders a real Slint UI (the same
 * Slint software renderer the kernel uses, here linked into a userland process)
 * into a shared-memory surface composited by the kernel. It gets the WM's
 * server-side window frame (title bar + close button) for free, exactly like
 * any other compositor client.
 *
 * This is the pattern the in-kernel Terminal / File Explorer / Text Editor
 * windows will be ported onto: an app is a separate process = Slint over
 * libradcompositor. */
#include "app.h"

#include "radcompositor.h"

#include <slint-platform.h>

#include <chrono>
#include <memory>
#include <span>

using namespace slint::platform;

static const uint32_t CONTENT_W = 400;
static const uint32_t CONTENT_H = 260;

static rad_wc_surface_t g_surface;      // the shm-backed compositor surface
static uint32_t g_frame = 0;

extern "C" long rad_syscall6(long, long, long, long, long, long, long);
static void say(const char *msg) {
    long n = 0;
    while (msg[n]) ++n;
    rad_syscall6(1, 1, (long)msg, n, 0, 0, 0);
}

// A window adapter whose swap-chain is the client's shared-memory surface.
class ClientAdapter : public WindowAdapter {
public:
    ClientAdapter()
        : renderer_(SoftwareRenderer::RepaintBufferType::ReusedBuffer),
          size_({CONTENT_W, CONTENT_H}) {}

    AbstractRenderer &renderer() override { return renderer_; }
    slint::PhysicalSize size() override { return size_; }
    void request_redraw() override { needs_redraw_ = true; }

    // Slint calls this from show(): lay the window out and schedule a repaint.
    void set_visible(bool visible) override {
        if (visible) {
            window().dispatch_resize_event(
                slint::LogicalSize({(float)CONTENT_W, (float)CONTENT_H}));
            needs_redraw_ = true;
        }
    }

    // Render the Slint scene, line by line, into the shm surface (XRGB8888) and
    // commit the touched region to the compositor. Mirrors the kernel windows.
    bool render_frame() {
        if (!needs_redraw_) return false;
        needs_redraw_ = false;
        uint32_t *pixels = g_surface.pixels;
        const uint32_t stride = g_surface.stride;
        if (!pixels) return false;

        std::size_t min_x = CONTENT_W, min_y = CONTENT_H, max_x = 0, max_y = 0;
        renderer_.render_by_line<slint::Rgb8Pixel>(
            [&](std::size_t line, std::size_t begin, std::size_t end, auto render_line) {
                if (line >= CONTENT_H || begin >= end || end > CONTENT_W) return;
                const std::size_t count = end - begin;
                if (count > CONTENT_W) return;
                if (begin < min_x) min_x = begin;
                if (line < min_y) min_y = line;
                if (end > max_x) max_x = end;
                if (line + 1u > max_y) max_y = line + 1u;
                std::span<slint::Rgb8Pixel> span(line_buffer_, count);
                render_line(span);
                uint32_t *dst = pixels + (line * stride) + begin;
                for (std::size_t i = 0; i < count; ++i) {
                    const auto &s = line_buffer_[i];
                    dst[i] = 0xff000000u | (uint32_t(s.r) << 16) | (uint32_t(s.g) << 8) | uint32_t(s.b);
                }
            });
        if (min_x < max_x && min_y < max_y) {
            rad_wc_surface_commit(&g_surface, (int)min_x, (int)min_y,
                                  (int)(max_x - min_x), (int)(max_y - min_y));
        }
        return true;
    }

    SoftwareRenderer renderer_;
    slint::PhysicalSize size_;
    slint::Rgb8Pixel line_buffer_[CONTENT_W];
    bool needs_redraw_ = true;
};

class ClientPlatform : public Platform {
public:
    std::unique_ptr<WindowAdapter> create_window_adapter() override {
        auto adapter = std::make_unique<ClientAdapter>();
        adapter_ = adapter.get();
        return adapter;
    }
    // Slint animations need a monotonic clock; frame*16ms is close enough.
    std::chrono::milliseconds duration_since_start() override {
        return std::chrono::milliseconds((long long)g_frame * 16);
    }
    ClientAdapter *adapter_ = nullptr;
};

static ClientPlatform *g_platform = nullptr;

static slint::LogicalPosition pos_of(const rad_wc_input_event_t &e) {
    return slint::LogicalPosition({(float)e.x, (float)e.y});
}

int main(void) {
    say("radslint-demo: starting userland Slint client\n");
    if (rad_wc_surface_open(&g_surface, "radslint-demo", CONTENT_W, CONTENT_H, 560, 200, 27) != 0) {
        say("radslint-demo: FAILED to open surface\n");
        return 1;
    }

    auto platform = std::make_unique<ClientPlatform>();
    g_platform = platform.get();
    slint::platform::set_platform(std::move(platform));

    auto app = AppWindow::create();
    // Accessing window() binds the component and creates our adapter (via the
    // platform). Drive layout + first repaint manually, like the kernel windows
    // do -- do NOT call show() (that hands the window to an event loop we don't
    // run, and leaves the scene unrendered).
    auto &win = app->window();
    win.dispatch_resize_event(slint::LogicalSize({(float)CONTENT_W, (float)CONTENT_H}));
    if (g_platform->adapter_) g_platform->adapter_->request_redraw();
    say("radslint-demo: RAD_SLINT_USERLAND_APP_OK window is up\n");

    int running = 1;
    while (running) {
        slint::platform::update_timers_and_animations();

        rad_wc_input_event_t ev;
        int r;
        auto &win = g_platform->adapter_->window();
        while ((r = rad_wc_surface_poll_input(&g_surface, &ev)) == 1) {
            if (ev.type == RAD_WC_EVENT_CLOSE) {
                say("radslint-demo: RAD_SLINT_USERLAND_CLOSE_OK\n");
                running = 0;
            } else if (ev.type == RAD_WC_EVENT_POINTER_MOTION) {
                win.dispatch_pointer_move_event(pos_of(ev));
            } else if (ev.type == RAD_WC_EVENT_POINTER_BUTTON) {
                if (ev.pressed) win.dispatch_pointer_press_event(pos_of(ev), slint::PointerEventButton::Left);
                else win.dispatch_pointer_release_event(pos_of(ev), slint::PointerEventButton::Left);
            }
        }
        if (r < 0) break;

        if (g_platform->adapter_) g_platform->adapter_->render_frame();
        ++g_frame;
        rad_wc_sleep_ms(16);
    }

    rad_wc_surface_close(&g_surface);
    return 0;
}
