#ifndef RAD_COMPOSITOR_MODEL_H
#define RAD_COMPOSITOR_MODEL_H

// Freestanding multi-window desktop model shared by the RADPx-OS kernel compositor
// (SlintShell.cpp) and the hosted rad_os_shell self-test. No std::vector/std::string
// (the kernel builds -nostdlib -fno-exceptions); fixed-capacity arrays + inline char
// buffers only, so this compiles into the freestanding kernel image unchanged.

#include <cstddef>
#include <cstdint>

namespace RADCompositor {

constexpr size_t MaxApps = 12;
constexpr size_t MaxWindows = 16;
constexpr size_t MaxIdLen = 32;
constexpr size_t MaxTitleLen = 48;

enum class TerminalAppState { Closed = 0, Launching = 1, Running = 2 };
enum class WindowState { Closed = 0, Loading = 1, Running = 2 };

struct WindowBounds {
    int32_t x = 134;
    int32_t y = 110;
    int32_t width = 640;
    int32_t height = 380;
};

struct AppDescriptor {
    char id[MaxIdLen] = {0};
    char title[MaxTitleLen] = {0};
};

struct Window {
    uint32_t id = 0;
    char appId[MaxIdLen] = {0};
    char title[MaxTitleLen] = {0};
    WindowBounds bounds;
    WindowState state = WindowState::Closed;
    uint32_t z = 0;
    bool focused = false;
};

class Model {
public:
    Model();

    bool applicationsMenuOpen() const;
    bool terminalOpen() const;
    bool terminalLaunching() const;
    TerminalAppState terminalState() const;
    const char *statusText() const; // valid until the next statusText() call

    // Fixed-capacity accessors (replace the old std::vector getters).
    size_t appCount() const { return appCount_; }
    const AppDescriptor *app(size_t i) const { return i < appCount_ ? &apps_[i] : nullptr; }
    size_t windowCount() const { return windowCount_; }
    const Window *window(size_t i) const { return i < windowCount_ ? &windows_[i] : nullptr; }
    size_t openWindowCount() const;

    const Window *focusedWindow() const;
    const Window *terminalWindow() const;

    uint32_t registerApp(const char *id, const char *title);
    uint32_t launchApp(const char *appId);
    bool focusWindow(uint32_t windowId);
    bool closeWindow(uint32_t windowId);
    bool moveWindow(uint32_t windowId, int32_t dx, int32_t dy);
    bool resizeWindow(uint32_t windowId, int32_t dx, int32_t dy);
    bool setWindowState(uint32_t windowId, WindowState state);

    void toggleApplicationsMenu();
    void beginTerminalLaunch();
    void terminalReady();
    void launchTerminal();
    bool handleEscape();

private:
    Window *findWindow(uint32_t windowId);
    const Window *findWindow(uint32_t windowId) const;
    Window *findWindowByApp(const char *appId);
    const Window *findWindowByApp(const char *appId) const;
    const AppDescriptor *findApp(const char *appId) const;
    void focusWindowRecord(Window& window);

    bool applicationsMenuOpen_ = false;
    AppDescriptor apps_[MaxApps];
    size_t appCount_ = 0;
    Window windows_[MaxWindows];
    size_t windowCount_ = 0;
    uint32_t nextWindowId_ = 1;
    uint32_t nextZ_ = 1;
    mutable char statusBuf_[80] = {0};
};

} // namespace RADCompositor

#endif
