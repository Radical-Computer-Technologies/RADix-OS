#include "RADCompositorModel.h"

#include <algorithm>

namespace RADCompositor {
namespace {
constexpr const char *TerminalAppId = "rad.terminal";
constexpr int32_t MinWindowWidth = 180;
constexpr int32_t MinWindowHeight = 112;
}

Model::Model() {
    registerApp(AppDescriptor{TerminalAppId, "Terminal"});
    const uint32_t id = launchApp(TerminalAppId);
    setWindowState(id, WindowState::Running);
}

bool Model::applicationsMenuOpen() const {
    return applicationsMenuOpen_;
}

bool Model::terminalOpen() const {
    const Window *window = terminalWindow();
    return window && window->state != WindowState::Closed;
}

bool Model::terminalLaunching() const {
    const Window *window = terminalWindow();
    return window && window->state == WindowState::Loading;
}

TerminalAppState Model::terminalState() const {
    const Window *window = terminalWindow();
    if (!window || window->state == WindowState::Closed) return TerminalAppState::Closed;
    if (window->state == WindowState::Loading) return TerminalAppState::Launching;
    return TerminalAppState::Running;
}

std::string Model::statusText() const {
    if (applicationsMenuOpen_) return "applications menu";
    const Window *window = focusedWindow();
    if (window && window->state == WindowState::Loading) return window->title + " launching";
    if (window && window->state == WindowState::Running) return window->title;
    return "desktop";
}

const std::vector<AppDescriptor>& Model::apps() const {
    return apps_;
}

const std::vector<Window>& Model::windows() const {
    return windows_;
}

const Window *Model::focusedWindow() const {
    const Window *best = nullptr;
    for (const Window& window : windows_) {
        if (window.state == WindowState::Closed) continue;
        if (window.focused) return &window;
        if (!best || window.z > best->z) best = &window;
    }
    return best;
}

const Window *Model::terminalWindow() const {
    return findWindowByApp(TerminalAppId);
}

uint32_t Model::registerApp(const AppDescriptor& app) {
    if (app.id.empty()) return 0;
    if (const AppDescriptor *existing = findApp(app.id)) {
        (void)existing;
        return 1;
    }
    apps_.push_back(app);
    return static_cast<uint32_t>(apps_.size());
}

uint32_t Model::launchApp(const std::string& appId) {
    const AppDescriptor *app = findApp(appId);
    if (!app) return 0;

    Window *existing = findWindowByApp(appId);
    if (existing) {
        existing->state = WindowState::Loading;
        existing->title = app->title;
        focusWindowRecord(*existing);
        applicationsMenuOpen_ = false;
        return existing->id;
    }

    Window window;
    window.id = nextWindowId_++;
    window.appId = app->id;
    window.title = app->title;
    window.state = WindowState::Loading;
    window.bounds = WindowBounds{};
    window.z = nextZ_++;
    windows_.push_back(window);
    focusWindowRecord(windows_.back());
    applicationsMenuOpen_ = false;
    return windows_.back().id;
}

bool Model::focusWindow(uint32_t windowId) {
    Window *window = findWindow(windowId);
    if (!window || window->state == WindowState::Closed) return false;
    focusWindowRecord(*window);
    applicationsMenuOpen_ = false;
    return true;
}

bool Model::closeWindow(uint32_t windowId) {
    Window *window = findWindow(windowId);
    if (!window) return false;
    window->state = WindowState::Closed;
    window->focused = false;
    applicationsMenuOpen_ = false;
    return true;
}

bool Model::moveWindow(uint32_t windowId, int32_t dx, int32_t dy) {
    Window *window = findWindow(windowId);
    if (!window || window->state == WindowState::Closed) return false;
    window->bounds.x = std::max<int32_t>(0, window->bounds.x + dx);
    window->bounds.y = std::max<int32_t>(38, window->bounds.y + dy);
    focusWindowRecord(*window);
    return true;
}

bool Model::resizeWindow(uint32_t windowId, int32_t dx, int32_t dy) {
    Window *window = findWindow(windowId);
    if (!window || window->state == WindowState::Closed) return false;
    window->bounds.width = std::max<int32_t>(MinWindowWidth, window->bounds.width + dx);
    window->bounds.height = std::max<int32_t>(MinWindowHeight, window->bounds.height + dy);
    focusWindowRecord(*window);
    return true;
}

bool Model::setWindowState(uint32_t windowId, WindowState state) {
    Window *window = findWindow(windowId);
    if (!window) return false;
    window->state = state;
    if (state != WindowState::Closed) focusWindowRecord(*window);
    return true;
}

void Model::toggleApplicationsMenu() {
    applicationsMenuOpen_ = !applicationsMenuOpen_;
}

void Model::beginTerminalLaunch() {
    launchApp(TerminalAppId);
}

void Model::terminalReady() {
    if (Window *window = findWindowByApp(TerminalAppId)) {
        setWindowState(window->id, WindowState::Running);
    }
}

void Model::launchTerminal() {
    beginTerminalLaunch();
    terminalReady();
}

bool Model::handleEscape() {
    if (applicationsMenuOpen_) {
        applicationsMenuOpen_ = false;
        return true;
    }
    const Window *window = focusedWindow();
    if (window) {
        closeWindow(window->id);
        return true;
    }
    return false;
}

Window *Model::findWindow(uint32_t windowId) {
    for (Window& window : windows_) {
        if (window.id == windowId) return &window;
    }
    return nullptr;
}

const Window *Model::findWindow(uint32_t windowId) const {
    for (const Window& window : windows_) {
        if (window.id == windowId) return &window;
    }
    return nullptr;
}

Window *Model::findWindowByApp(const std::string& appId) {
    for (Window& window : windows_) {
        if (window.appId == appId) return &window;
    }
    return nullptr;
}

const Window *Model::findWindowByApp(const std::string& appId) const {
    for (const Window& window : windows_) {
        if (window.appId == appId) return &window;
    }
    return nullptr;
}

const AppDescriptor *Model::findApp(const std::string& appId) const {
    for (const AppDescriptor& app : apps_) {
        if (app.id == appId) return &app;
    }
    return nullptr;
}

void Model::focusWindowRecord(Window& focused) {
    for (Window& window : windows_) {
        window.focused = false;
    }
    focused.focused = true;
    focused.z = nextZ_++;
}

} // namespace RADCompositor
