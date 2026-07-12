#include <RADUi/RADSlintFramebufferBackend.h>
#include <radboot.h>
#include <radixkernel/radixkernel.h>

#include "RADixDesktopShell.h"
#include "shell.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {
uint32_t framebuffer[640u * 360u];

struct Capture {
    std::string text;
};

void capture_write(const char *text, void *context) {
    auto *capture = static_cast<Capture*>(context);
    if (capture && text) {
        capture->text += text;
    }
}

std::string run_command(const char *line) {
    Capture capture;
    rad_terminal_execute(line, capture_write, &capture);
    return capture.text;
}

class KernelPtyTranscript {
public:
    KernelPtyTranscript() {
        if (rad_pty_open_pair("slint-shell", &pty_) != RAD_STATUS_OK) return;
        char slaveName[64]{};
        if (rad_pty_slave_name(pty_, slaveName, sizeof(slaveName)) != RAD_STATUS_OK) return;
        if (rad_tty_open(slaveName, &slave_) != RAD_STATUS_OK) return;
        rad_tty_window_size_t size{24, 78};
        rad_pty_resize(pty_, &size);
        ready_ = true;
    }

    ~KernelPtyTranscript() {
        if (slave_) rad_tty_close(slave_);
        if (pty_) rad_pty_close(pty_);
    }

    bool ready() const {
        return ready_;
    }

    std::string run(const char *line) {
        if (!ready_ || !line) return {};
        append("$ ");
        append(line);
        append("\n");

        size_t written = 0;
        rad_pty_write_master(pty_, line, std::strlen(line), &written);
        rad_pty_write_master(pty_, "\n", 1, &written);
        rad_terminal_poll_tty(slave_);
        drain();
        return text_;
    }

private:
    void append(const char *text) {
        if (text) text_ += text;
    }

    void drain() {
        char buffer[512];
        size_t count = 0;
        while (rad_pty_read_master(pty_, buffer, sizeof(buffer), &count) == RAD_STATUS_OK && count > 0) {
            text_.append(buffer, count);
        }
    }

    rad_pty_t pty_ = nullptr;
    rad_tty_t slave_ = nullptr;
    bool ready_ = false;
    std::string text_;
};

slint::SharedString ss(const std::string& value) {
    return slint::SharedString(std::string_view(value));
}

rad_status_t register_framebuffer() {
    rad_framebuffer_config_t fb{};
    fb.size = sizeof(fb);
    fb.name = "/dev/fb0";
    fb.info.size = sizeof(fb.info);
    fb.info.width = 640u;
    fb.info.height = 360u;
    fb.info.stride_bytes = 640u * sizeof(uint32_t);
    fb.info.pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
    fb.info.pixels = framebuffer;
    fb.output_type = RAD_DISPLAY_OUTPUT_MEMORY;
    fb.connector = "host-memory";
    fb.mode_count = 1u;
    fb.preferred_mode = 0u;
    fb.modes[0].width = fb.info.width;
    fb.modes[0].height = fb.info.height;
    fb.modes[0].refresh_hz = 60u;
    fb.modes[0].stride_bytes = fb.info.stride_bytes;
    fb.modes[0].pixel_format = fb.info.pixel_format;
    fb.primary = 1;
    rad_framebuffer_ops_t ops{};
    return rad_framebuffer_register_ex(&fb, &ops);
}
}

int main(int argc, char **argv) {
    const bool self_test = argc > 1 && std::strcmp(argv[1], "--self-test") == 0;

    rad_boot_info_t boot{};
    radboot_prepare_default(&boot, "linux_sim", "rad-os-shell", "/dev/console", "none");
    rad_kernel_config_t config{};
    config.backend_name = "linux_sim";
    config.boot_info = &boot;
    if (rad_kernel_init(&config) != RAD_STATUS_OK) {
        std::cerr << "rad_kernel_init failed\n";
        return EXIT_FAILURE;
    }
    if (register_framebuffer() != RAD_STATUS_OK) {
        std::cerr << "framebuffer registration failed\n";
        return EXIT_FAILURE;
    }

    RADUi::Slint::FramebufferPlatformConfig ui_config;
    ui_config.maxFrames = self_test ? 1u : 0u;
    ui_config.keyboardInputDevice = "/dev/input/event0";
    ui_config.pointerInputDevice = "/dev/input/event1";
    std::shared_ptr<RADUi::Slint::SlintBackend> backend;
    const rad_status_t ui_status = RADUi::Slint::tryInstallFramebufferBackend(ui_config, &backend);
    if (ui_status != RAD_STATUS_OK) {
        std::cerr << "Slint framebuffer backend failed: " << ui_status << "\n";
        return EXIT_FAILURE;
    }

    std::string terminal;
    bool terminalLoadingObserved = false;
    bool terminalReadyObserved = false;
    bool windowManagerObserved = false;
    bool terminalWindowObserved = false;
    bool menuOpenObserved = false;
    bool menuEscapeObserved = false;
    bool windowMoveObserved = false;
    bool windowResizeObserved = false;
    bool terminalCloseObserved = false;
    bool terminalRelaunchObserved = false;

    auto ui = RadOsShell::create();
    RADixShell::DesktopShellModel desktop;
    auto refreshShell = [&]() {
        const RADixShell::DesktopWindow *terminalWindow = desktop.terminalWindow();
        ui->set_backend(ss(std::string(rad_kernel_backend_name()) + " / " + rad_kernel_version_string()));
        ui->set_status(ss(std::string("framebuffer=primary shell=radlib state=") + desktop.statusText()));
        ui->set_terminal(ss(terminal));
        ui->set_applications_open(desktop.applicationsMenuOpen());
        ui->set_terminal_open(desktop.terminalOpen());
        ui->set_terminal_loading(desktop.terminalLaunching());
        if (terminalWindow) {
            ui->set_terminal_window_id(static_cast<int32_t>(terminalWindow->id));
            ui->set_terminal_window_title(ss(terminalWindow->title));
            ui->set_terminal_window_x(static_cast<float>(terminalWindow->bounds.x));
            ui->set_terminal_window_y(static_cast<float>(terminalWindow->bounds.y));
            ui->set_terminal_window_width(static_cast<float>(terminalWindow->bounds.width));
            ui->set_terminal_window_height(static_cast<float>(terminalWindow->bounds.height));
            ui->set_terminal_window_focused(terminalWindow->focused);
            if (terminalWindow->state != RADixShell::DesktopWindowState::Closed) terminalWindowObserved = true;
        }
        if (desktop.terminalLaunching()) terminalLoadingObserved = true;
        if (desktop.terminalState() == RADixShell::TerminalAppState::Running) terminalReadyObserved = true;
        if (!desktop.windows().empty() && !desktop.apps().empty()) windowManagerObserved = true;
        if (desktop.applicationsMenuOpen()) menuOpenObserved = true;
    };
    auto launchTerminal = [&]() {
        desktop.beginTerminalLaunch();
        refreshShell();

        terminal.clear();
        KernelPtyTranscript ptyTerminal;
        if (ptyTerminal.ready()) {
            ptyTerminal.run("bootinfo");
            ptyTerminal.run("cores");
            ptyTerminal.run("mem");
            ptyTerminal.run("devices");
            ptyTerminal.run("perf");
            ptyTerminal.run("latency");
            terminal = ptyTerminal.run("fb");
        } else {
            terminal += "$ bootinfo\n" + run_command("bootinfo");
            terminal += "$ cores\n" + run_command("cores");
            terminal += "$ mem\n" + run_command("mem");
            terminal += "$ devices\n" + run_command("devices");
            terminal += "$ perf\n" + run_command("perf");
            terminal += "$ latency\n" + run_command("latency");
            terminal += "$ fb\n" + run_command("fb");
        }
        desktop.terminalReady();
        refreshShell();
    };
    ui->on_toggle_applications([&]() {
        desktop.toggleApplicationsMenu();
        refreshShell();
    });
    ui->on_launch_terminal([&]() {
        launchTerminal();
    });
    ui->on_escape_pressed([&]() {
        desktop.handleEscape();
        refreshShell();
    });
    ui->on_focus_terminal_window([&]() {
        if (const RADixShell::DesktopWindow *window = desktop.terminalWindow()) {
            desktop.focusWindow(window->id);
        }
        refreshShell();
    });
    ui->on_close_terminal_window([&]() {
        if (const RADixShell::DesktopWindow *window = desktop.terminalWindow()) {
            desktop.closeWindow(window->id);
        }
        refreshShell();
    });
    ui->on_move_terminal_window([&](float dx, float dy) {
        if (const RADixShell::DesktopWindow *window = desktop.terminalWindow()) {
            desktop.moveWindow(window->id, static_cast<int32_t>(dx), static_cast<int32_t>(dy));
        }
        refreshShell();
    });
    ui->on_resize_terminal_window([&](float dx, float dy) {
        if (const RADixShell::DesktopWindow *window = desktop.terminalWindow()) {
            desktop.resizeWindow(window->id, static_cast<int32_t>(dx), static_cast<int32_t>(dy));
        }
        refreshShell();
    });
    launchTerminal();
    if (self_test) {
        desktop.toggleApplicationsMenu();
        refreshShell();
        if (desktop.handleEscape()) {
            menuEscapeObserved = true;
            refreshShell();
        }
        if (const RADixShell::DesktopWindow *window = desktop.terminalWindow()) {
            const auto before = window->bounds;
            desktop.moveWindow(window->id, 12, 8);
            const RADixShell::DesktopWindow *moved = desktop.terminalWindow();
            windowMoveObserved = moved && moved->bounds.x == before.x + 12 && moved->bounds.y == before.y + 8;
            desktop.resizeWindow(window->id, 24, 16);
            const RADixShell::DesktopWindow *resized = desktop.terminalWindow();
            windowResizeObserved = resized && resized->bounds.width == before.width + 24 && resized->bounds.height == before.height + 16;
            refreshShell();
        }
        if (const RADixShell::DesktopWindow *window = desktop.terminalWindow()) {
            desktop.closeWindow(window->id);
            terminalCloseObserved = desktop.terminalState() == RADixShell::TerminalAppState::Closed;
            refreshShell();
        }
        launchTerminal();
        terminalRelaunchObserved = desktop.terminalState() == RADixShell::TerminalAppState::Running;
    }
    ui->show();
    RADUi::run();

    uint32_t checksum = 0;
    for (uint32_t pixel : framebuffer) {
        checksum |= pixel;
    }
    if (self_test) {
        std::cout << "RAD OS Slint shell rendered checksum=0x" << std::hex << checksum << "\n";
        if (terminalLoadingObserved) std::cout << "RADIX_SLINT_TERMINAL_LOADING_OK\n";
        if (terminalReadyObserved) std::cout << "RADIX_SLINT_TERMINAL_READY_OK\n";
        if (windowManagerObserved) std::cout << "RADIX_SLINT_WM_OK\n";
        if (terminalWindowObserved) std::cout << "RADIX_SLINT_APP_TERMINAL_WINDOW_OK\n";
        if (menuOpenObserved) std::cout << "RADIX_SLINT_MENU_OPEN_OK\n";
        if (menuEscapeObserved) std::cout << "RADIX_SLINT_MENU_ESCAPE_OK\n";
        if (windowMoveObserved) std::cout << "RADIX_SLINT_WINDOW_MOVE_OK\n";
        if (windowResizeObserved) std::cout << "RADIX_SLINT_WINDOW_RESIZE_OK\n";
        if (terminalCloseObserved) std::cout << "RADIX_SLINT_TERMINAL_CLOSE_OK\n";
        if (terminalRelaunchObserved) std::cout << "RADIX_SLINT_TERMINAL_RELAUNCH_OK\n";
    }
    rad_kernel_shutdown();
    return checksum ? EXIT_SUCCESS : EXIT_FAILURE;
}
