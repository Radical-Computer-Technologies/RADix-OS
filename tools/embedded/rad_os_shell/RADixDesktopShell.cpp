#include "RADixDesktopShell.h"

#include <algorithm>

namespace RADixShell {
namespace {
constexpr const char *TerminalAppId = "rad.terminal";
constexpr int32_t MinWindowWidth = 180;
constexpr int32_t MinWindowHeight = 112;
}

DesktopShellModel::DesktopShellModel() {
    registerApp(DesktopAppDescriptor{TerminalAppId, "Terminal"});
    const uint32_t id = launchApp(TerminalAppId);
    setWindowState(id, DesktopWindowState::Running);
}

bool DesktopShellModel::applicationsMenuOpen() const {
    return applicationsMenuOpen_;
}

bool DesktopShellModel::terminalOpen() const {
    const DesktopWindow *window = terminalWindow();
    return window && window->state != DesktopWindowState::Closed;
}

bool DesktopShellModel::terminalLaunching() const {
    const DesktopWindow *window = terminalWindow();
    return window && window->state == DesktopWindowState::Loading;
}

TerminalAppState DesktopShellModel::terminalState() const {
    const DesktopWindow *window = terminalWindow();
    if (!window || window->state == DesktopWindowState::Closed) return TerminalAppState::Closed;
    if (window->state == DesktopWindowState::Loading) return TerminalAppState::Launching;
    return TerminalAppState::Running;
}

std::string DesktopShellModel::statusText() const {
    if (applicationsMenuOpen_) return "applications menu";
    const DesktopWindow *window = focusedWindow();
    if (window && window->state == DesktopWindowState::Loading) return window->title + " launching";
    if (window && window->state == DesktopWindowState::Running) return window->title;
    return "desktop";
}

const std::vector<DesktopAppDescriptor>& DesktopShellModel::apps() const {
    return apps_;
}

const std::vector<DesktopWindow>& DesktopShellModel::windows() const {
    return windows_;
}

const DesktopWindow *DesktopShellModel::focusedWindow() const {
    const DesktopWindow *best = nullptr;
    for (const DesktopWindow& window : windows_) {
        if (window.state == DesktopWindowState::Closed) continue;
        if (window.focused) return &window;
        if (!best || window.z > best->z) best = &window;
    }
    return best;
}

const DesktopWindow *DesktopShellModel::terminalWindow() const {
    return findWindowByApp(TerminalAppId);
}

uint32_t DesktopShellModel::registerApp(const DesktopAppDescriptor& app) {
    if (app.id.empty()) return 0;
    if (const DesktopAppDescriptor *existing = findApp(app.id)) {
        (void)existing;
        return 1;
    }
    apps_.push_back(app);
    return static_cast<uint32_t>(apps_.size());
}

uint32_t DesktopShellModel::launchApp(const std::string& appId) {
    const DesktopAppDescriptor *app = findApp(appId);
    if (!app) return 0;

    DesktopWindow *existing = findWindowByApp(appId);
    if (existing) {
        existing->state = DesktopWindowState::Loading;
        existing->title = app->title;
        focusWindowRecord(*existing);
        applicationsMenuOpen_ = false;
        return existing->id;
    }

    DesktopWindow window;
    window.id = nextWindowId_++;
    window.appId = app->id;
    window.title = app->title;
    window.state = DesktopWindowState::Loading;
    window.bounds = DesktopWindowBounds{};
    window.z = nextZ_++;
    windows_.push_back(window);
    focusWindowRecord(windows_.back());
    applicationsMenuOpen_ = false;
    return windows_.back().id;
}

bool DesktopShellModel::focusWindow(uint32_t windowId) {
    DesktopWindow *window = findWindow(windowId);
    if (!window || window->state == DesktopWindowState::Closed) return false;
    focusWindowRecord(*window);
    applicationsMenuOpen_ = false;
    return true;
}

bool DesktopShellModel::closeWindow(uint32_t windowId) {
    DesktopWindow *window = findWindow(windowId);
    if (!window) return false;
    window->state = DesktopWindowState::Closed;
    window->focused = false;
    applicationsMenuOpen_ = false;
    return true;
}

bool DesktopShellModel::moveWindow(uint32_t windowId, int32_t dx, int32_t dy) {
    DesktopWindow *window = findWindow(windowId);
    if (!window || window->state == DesktopWindowState::Closed) return false;
    window->bounds.x = std::max<int32_t>(0, window->bounds.x + dx);
    window->bounds.y = std::max<int32_t>(38, window->bounds.y + dy);
    focusWindowRecord(*window);
    return true;
}

bool DesktopShellModel::resizeWindow(uint32_t windowId, int32_t dx, int32_t dy) {
    DesktopWindow *window = findWindow(windowId);
    if (!window || window->state == DesktopWindowState::Closed) return false;
    window->bounds.width = std::max<int32_t>(MinWindowWidth, window->bounds.width + dx);
    window->bounds.height = std::max<int32_t>(MinWindowHeight, window->bounds.height + dy);
    focusWindowRecord(*window);
    return true;
}

bool DesktopShellModel::setWindowState(uint32_t windowId, DesktopWindowState state) {
    DesktopWindow *window = findWindow(windowId);
    if (!window) return false;
    window->state = state;
    if (state != DesktopWindowState::Closed) focusWindowRecord(*window);
    return true;
}

void DesktopShellModel::toggleApplicationsMenu() {
    applicationsMenuOpen_ = !applicationsMenuOpen_;
}

void DesktopShellModel::beginTerminalLaunch() {
    launchApp(TerminalAppId);
}

void DesktopShellModel::terminalReady() {
    if (DesktopWindow *window = findWindowByApp(TerminalAppId)) {
        setWindowState(window->id, DesktopWindowState::Running);
    }
}

void DesktopShellModel::launchTerminal() {
    beginTerminalLaunch();
    terminalReady();
}

bool DesktopShellModel::handleEscape() {
    if (applicationsMenuOpen_) {
        applicationsMenuOpen_ = false;
        return true;
    }
    const DesktopWindow *window = focusedWindow();
    if (window) {
        closeWindow(window->id);
        return true;
    }
    return false;
}

DesktopWindow *DesktopShellModel::findWindow(uint32_t windowId) {
    for (DesktopWindow& window : windows_) {
        if (window.id == windowId) return &window;
    }
    return nullptr;
}

const DesktopWindow *DesktopShellModel::findWindow(uint32_t windowId) const {
    for (const DesktopWindow& window : windows_) {
        if (window.id == windowId) return &window;
    }
    return nullptr;
}

DesktopWindow *DesktopShellModel::findWindowByApp(const std::string& appId) {
    for (DesktopWindow& window : windows_) {
        if (window.appId == appId) return &window;
    }
    return nullptr;
}

const DesktopWindow *DesktopShellModel::findWindowByApp(const std::string& appId) const {
    for (const DesktopWindow& window : windows_) {
        if (window.appId == appId) return &window;
    }
    return nullptr;
}

const DesktopAppDescriptor *DesktopShellModel::findApp(const std::string& appId) const {
    for (const DesktopAppDescriptor& app : apps_) {
        if (app.id == appId) return &app;
    }
    return nullptr;
}

void DesktopShellModel::focusWindowRecord(DesktopWindow& focused) {
    for (DesktopWindow& window : windows_) {
        window.focused = false;
    }
    focused.focused = true;
    focused.z = nextZ_++;
}

} // namespace RADixShell
