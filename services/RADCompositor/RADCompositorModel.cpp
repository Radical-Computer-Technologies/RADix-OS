#include "RADCompositorModel.h"

// Freestanding: no <algorithm>/<string>. Tiny local string + clamp helpers so this
// links into the -nostdlib kernel image as well as the hosted self-test.

namespace RADCompositor {
namespace {
constexpr const char *TerminalAppId = "rad.terminal";
constexpr int32_t MinWindowWidth = 180;
constexpr int32_t MinWindowHeight = 112;
constexpr int32_t TopBarHeight = 38;

bool str_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
    return *a == *b;
}
bool str_empty(const char *s) { return !s || s[0] == '\0'; }
void str_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    size_t i = 0;
    if (src) for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i];
    dst[i] = '\0';
}
void str_append(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    size_t i = 0;
    while (i + 1 < cap && dst[i]) ++i;
    if (src) for (size_t j = 0; src[j] && i + 1 < cap; ++i, ++j) dst[i] = src[j];
    dst[i] = '\0';
}
int32_t clamp_min(int32_t v, int32_t lo) { return v < lo ? lo : v; }
} // namespace

Model::Model() {
    registerApp(TerminalAppId, "Terminal");
    const uint32_t id = launchApp(TerminalAppId);
    setWindowState(id, WindowState::Running);
}

bool Model::applicationsMenuOpen() const { return applicationsMenuOpen_; }

bool Model::terminalOpen() const {
    const Window *w = terminalWindow();
    return w && w->state != WindowState::Closed;
}

bool Model::terminalLaunching() const {
    const Window *w = terminalWindow();
    return w && w->state == WindowState::Loading;
}

TerminalAppState Model::terminalState() const {
    const Window *w = terminalWindow();
    if (!w || w->state == WindowState::Closed) return TerminalAppState::Closed;
    if (w->state == WindowState::Loading) return TerminalAppState::Launching;
    return TerminalAppState::Running;
}

const char *Model::statusText() const {
    if (applicationsMenuOpen_) { str_copy(statusBuf_, sizeof(statusBuf_), "applications menu"); return statusBuf_; }
    const Window *w = focusedWindow();
    if (w && w->state == WindowState::Loading) {
        str_copy(statusBuf_, sizeof(statusBuf_), w->title);
        str_append(statusBuf_, sizeof(statusBuf_), " launching");
        return statusBuf_;
    }
    if (w && w->state == WindowState::Running) { str_copy(statusBuf_, sizeof(statusBuf_), w->title); return statusBuf_; }
    str_copy(statusBuf_, sizeof(statusBuf_), "desktop");
    return statusBuf_;
}

size_t Model::openWindowCount() const {
    size_t n = 0;
    for (size_t i = 0; i < windowCount_; ++i) if (windows_[i].state != WindowState::Closed) ++n;
    return n;
}

const Window *Model::focusedWindow() const {
    const Window *best = nullptr;
    for (size_t i = 0; i < windowCount_; ++i) {
        const Window& w = windows_[i];
        if (w.state == WindowState::Closed) continue;
        if (w.focused) return &w;
        if (!best || w.z > best->z) best = &w;
    }
    return best;
}

const Window *Model::terminalWindow() const { return findWindowByApp(TerminalAppId); }

uint32_t Model::registerApp(const char *id, const char *title) {
    if (str_empty(id)) return 0;
    if (findApp(id)) return 1;
    if (appCount_ >= MaxApps) return 0;
    AppDescriptor& a = apps_[appCount_++];
    str_copy(a.id, MaxIdLen, id);
    str_copy(a.title, MaxTitleLen, title);
    return static_cast<uint32_t>(appCount_);
}

uint32_t Model::launchApp(const char *appId) {
    const AppDescriptor *app = findApp(appId);
    if (!app) return 0;
    Window *existing = findWindowByApp(appId);
    if (existing) {
        existing->state = WindowState::Loading;
        str_copy(existing->title, MaxTitleLen, app->title);
        focusWindowRecord(*existing);
        applicationsMenuOpen_ = false;
        return existing->id;
    }
    if (windowCount_ >= MaxWindows) return 0;
    Window& w = windows_[windowCount_++];
    w = Window{};
    w.id = nextWindowId_++;
    str_copy(w.appId, MaxIdLen, app->id);
    str_copy(w.title, MaxTitleLen, app->title);
    w.state = WindowState::Loading;
    // Cascade new windows so multiple don't stack exactly.
    w.bounds.x = 134 + static_cast<int32_t>((windowCount_ - 1) * 28);
    w.bounds.y = 110 + static_cast<int32_t>((windowCount_ - 1) * 26);
    w.z = nextZ_++;
    focusWindowRecord(w);
    applicationsMenuOpen_ = false;
    return w.id;
}

bool Model::focusWindow(uint32_t windowId) {
    Window *w = findWindow(windowId);
    if (!w || w->state == WindowState::Closed) return false;
    focusWindowRecord(*w);
    applicationsMenuOpen_ = false;
    return true;
}

bool Model::closeWindow(uint32_t windowId) {
    Window *w = findWindow(windowId);
    if (!w) return false;
    w->state = WindowState::Closed;
    w->focused = false;
    applicationsMenuOpen_ = false;
    return true;
}

bool Model::moveWindow(uint32_t windowId, int32_t dx, int32_t dy) {
    Window *w = findWindow(windowId);
    if (!w || w->state == WindowState::Closed) return false;
    w->bounds.x = clamp_min(w->bounds.x + dx, 0);
    w->bounds.y = clamp_min(w->bounds.y + dy, TopBarHeight);
    focusWindowRecord(*w);
    return true;
}

bool Model::resizeWindow(uint32_t windowId, int32_t dx, int32_t dy) {
    Window *w = findWindow(windowId);
    if (!w || w->state == WindowState::Closed) return false;
    w->bounds.width = clamp_min(w->bounds.width + dx, MinWindowWidth);
    w->bounds.height = clamp_min(w->bounds.height + dy, MinWindowHeight);
    focusWindowRecord(*w);
    return true;
}

bool Model::setWindowState(uint32_t windowId, WindowState state) {
    Window *w = findWindow(windowId);
    if (!w) return false;
    w->state = state;
    if (state != WindowState::Closed) focusWindowRecord(*w);
    return true;
}

void Model::toggleApplicationsMenu() { applicationsMenuOpen_ = !applicationsMenuOpen_; }
void Model::beginTerminalLaunch() { launchApp(TerminalAppId); }

void Model::terminalReady() {
    if (Window *w = findWindowByApp(TerminalAppId)) setWindowState(w->id, WindowState::Running);
}

void Model::launchTerminal() { beginTerminalLaunch(); terminalReady(); }

bool Model::handleEscape() {
    if (applicationsMenuOpen_) { applicationsMenuOpen_ = false; return true; }
    const Window *w = focusedWindow();
    if (w) { closeWindow(w->id); return true; }
    return false;
}

Window *Model::findWindow(uint32_t windowId) {
    for (size_t i = 0; i < windowCount_; ++i) if (windows_[i].id == windowId) return &windows_[i];
    return nullptr;
}
const Window *Model::findWindow(uint32_t windowId) const {
    for (size_t i = 0; i < windowCount_; ++i) if (windows_[i].id == windowId) return &windows_[i];
    return nullptr;
}
Window *Model::findWindowByApp(const char *appId) {
    for (size_t i = 0; i < windowCount_; ++i) if (str_eq(windows_[i].appId, appId)) return &windows_[i];
    return nullptr;
}
const Window *Model::findWindowByApp(const char *appId) const {
    for (size_t i = 0; i < windowCount_; ++i) if (str_eq(windows_[i].appId, appId)) return &windows_[i];
    return nullptr;
}
const AppDescriptor *Model::findApp(const char *appId) const {
    for (size_t i = 0; i < appCount_; ++i) if (str_eq(apps_[i].id, appId)) return &apps_[i];
    return nullptr;
}

void Model::focusWindowRecord(Window& focused) {
    for (size_t i = 0; i < windowCount_; ++i) windows_[i].focused = false;
    focused.focused = true;
    focused.z = nextZ_++;
}

} // namespace RADCompositor
