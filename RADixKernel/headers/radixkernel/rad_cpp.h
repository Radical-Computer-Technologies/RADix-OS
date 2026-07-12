#ifndef RADEMBEDDEDKERNEL_RAD_CPP_H
#define RADEMBEDDEDKERNEL_RAD_CPP_H

#include <radixkernel/radixkernel.h>
#include <stddef.h>
#include <stdint.h>

namespace rad {

enum class Status : int32_t {
    ok = RAD_STATUS_OK,
    error = RAD_STATUS_ERROR,
    invalid_argument = RAD_STATUS_INVALID_ARGUMENT,
    not_found = RAD_STATUS_NOT_FOUND,
    no_memory = RAD_STATUS_NO_MEMORY,
    timeout = RAD_STATUS_TIMEOUT,
    not_supported = RAD_STATUS_NOT_SUPPORTED,
    already_exists = RAD_STATUS_ALREADY_EXISTS,
    not_initialized = RAD_STATUS_NOT_INITIALIZED
};

inline constexpr bool succeeded(rad_status_t status) { return status == RAD_STATUS_OK; }
inline constexpr bool failed(rad_status_t status) { return status != RAD_STATUS_OK; }

template <typename T>
class Span {
public:
    constexpr Span() = default;
    constexpr Span(T *data, size_t size) : data_(data), size_(size) {}
    constexpr T *data() const { return data_; }
    constexpr size_t size() const { return size_; }
    constexpr bool empty() const { return size_ == 0; }
    constexpr T& operator[](size_t index) const { return data_[index]; }
    constexpr T *begin() const { return data_; }
    constexpr T *end() const { return data_ + size_; }
private:
    T *data_ = nullptr;
    size_t size_ = 0;
};

template <typename Handle, void (*CloseFn)(Handle)>
class UniqueHandle {
public:
    constexpr UniqueHandle() = default;
    explicit constexpr UniqueHandle(Handle handle) : handle_(handle) {}
    ~UniqueHandle() { reset(); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }
    constexpr Handle get() const { return handle_; }
    constexpr explicit operator bool() const { return handle_ != nullptr; }
    Handle release() {
        Handle handle = handle_;
        handle_ = nullptr;
        return handle;
    }
    void reset(Handle handle = nullptr) {
        if (handle_) CloseFn(handle_);
        handle_ = handle;
    }
private:
    Handle handle_ = nullptr;
};

using Device = UniqueHandle<rad_device_t, rad_device_close>;
using File = UniqueHandle<rad_file_t, rad_vfs_close>;
using Tty = UniqueHandle<rad_tty_t, rad_tty_close>;
using Pty = UniqueHandle<rad_pty_t, rad_pty_close>;

class Mutex {
public:
    Mutex() { status_ = rad_mutex_create(&handle_); }
    ~Mutex() { if (handle_) rad_mutex_destroy(handle_); }
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    rad_status_t status() const { return status_; }
    rad_status_t lock() { return handle_ ? rad_mutex_lock(handle_) : RAD_STATUS_NOT_INITIALIZED; }
    rad_status_t unlock() { return handle_ ? rad_mutex_unlock(handle_) : RAD_STATUS_NOT_INITIALIZED; }
    rad_mutex_t native() const { return handle_; }
private:
    rad_mutex_t handle_ = nullptr;
    rad_status_t status_ = RAD_STATUS_NOT_INITIALIZED;
};

class Event {
public:
    explicit Event(bool signaled = false) { status_ = rad_event_create(&handle_, signaled ? 1 : 0); }
    ~Event() { if (handle_) rad_event_destroy(handle_); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    rad_status_t status() const { return status_; }
    rad_status_t wait(uint32_t timeout_ms) { return handle_ ? rad_event_wait(handle_, timeout_ms) : RAD_STATUS_NOT_INITIALIZED; }
    rad_status_t signal() { return handle_ ? rad_event_signal(handle_) : RAD_STATUS_NOT_INITIALIZED; }
    rad_status_t reset() { return handle_ ? rad_event_reset(handle_) : RAD_STATUS_NOT_INITIALIZED; }
private:
    rad_event_t handle_ = nullptr;
    rad_status_t status_ = RAD_STATUS_NOT_INITIALIZED;
};

class Module {
public:
    explicit Module(const rad_module_descriptor_t& descriptor) : name_(descriptor.name) {
        status_ = rad_module_register(&descriptor);
    }
    ~Module() { if (name_ && status_ == RAD_STATUS_OK) rad_module_unregister(name_); }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    rad_status_t status() const { return status_; }
private:
    const char *name_ = nullptr;
    rad_status_t status_ = RAD_STATUS_NOT_INITIALIZED;
};

} // namespace rad

#endif
