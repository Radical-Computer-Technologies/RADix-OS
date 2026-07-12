#ifndef RADIX_DESKTOP_SHELL_H
#define RADIX_DESKTOP_SHELL_H

#include <cstdint>
#include <string>
#include <vector>

namespace RADixShell {

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

struct DesktopAppDescriptor {
    std::string id;
    std::string title;
};

struct DesktopWindow {
    uint32_t id = 0;
    std::string appId;
    std::string title;
    DesktopWindowBounds bounds;
    DesktopWindowState state = DesktopWindowState::Closed;
    uint32_t z = 0;
    bool focused = false;
};

class DesktopShellModel {
public:
    DesktopShellModel();

    bool applicationsMenuOpen() const;
    bool terminalOpen() const;
    bool terminalLaunching() const;
    TerminalAppState terminalState() const;
    std::string statusText() const;

    const std::vector<DesktopAppDescriptor>& apps() const;
    const std::vector<DesktopWindow>& windows() const;
    const DesktopWindow *focusedWindow() const;
    const DesktopWindow *terminalWindow() const;

    uint32_t registerApp(const DesktopAppDescriptor& app);
    uint32_t launchApp(const std::string& appId);
    bool focusWindow(uint32_t windowId);
    bool closeWindow(uint32_t windowId);
    bool moveWindow(uint32_t windowId, int32_t dx, int32_t dy);
    bool resizeWindow(uint32_t windowId, int32_t dx, int32_t dy);
    bool setWindowState(uint32_t windowId, DesktopWindowState state);

    void toggleApplicationsMenu();
    void beginTerminalLaunch();
    void terminalReady();
    void launchTerminal();
    bool handleEscape();

private:
    DesktopWindow *findWindow(uint32_t windowId);
    const DesktopWindow *findWindow(uint32_t windowId) const;
    DesktopWindow *findWindowByApp(const std::string& appId);
    const DesktopWindow *findWindowByApp(const std::string& appId) const;
    const DesktopAppDescriptor *findApp(const std::string& appId) const;
    void focusWindowRecord(DesktopWindow& window);

    bool applicationsMenuOpen_ = false;
    std::vector<DesktopAppDescriptor> apps_;
    std::vector<DesktopWindow> windows_;
    uint32_t nextWindowId_ = 1;
    uint32_t nextZ_ = 1;
};

} // namespace RADixShell

#endif
