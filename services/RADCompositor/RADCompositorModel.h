#ifndef RAD_COMPOSITOR_MODEL_H
#define RAD_COMPOSITOR_MODEL_H

#include <cstdint>
#include <string>
#include <vector>

namespace RADCompositor {

enum class TerminalAppState {
    Closed = 0,
    Launching = 1,
    Running = 2
};

enum class WindowState {
    Closed = 0,
    Loading = 1,
    Running = 2
};

struct WindowBounds {
    int32_t x = 134;
    int32_t y = 110;
    int32_t width = 640;
    int32_t height = 380;
};

struct AppDescriptor {
    std::string id;
    std::string title;
};

struct Window {
    uint32_t id = 0;
    std::string appId;
    std::string title;
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
    std::string statusText() const;

    const std::vector<AppDescriptor>& apps() const;
    const std::vector<Window>& windows() const;
    const Window *focusedWindow() const;
    const Window *terminalWindow() const;

    uint32_t registerApp(const AppDescriptor& app);
    uint32_t launchApp(const std::string& appId);
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
    Window *findWindowByApp(const std::string& appId);
    const Window *findWindowByApp(const std::string& appId) const;
    const AppDescriptor *findApp(const std::string& appId) const;
    void focusWindowRecord(Window& window);

    bool applicationsMenuOpen_ = false;
    std::vector<AppDescriptor> apps_;
    std::vector<Window> windows_;
    uint32_t nextWindowId_ = 1;
    uint32_t nextZ_ = 1;
};

} // namespace RADCompositor

#endif
