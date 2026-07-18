#include <radkernel/radkernel.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef RAD_KERNEL_NEWLIB_MAX_FDS
#define RAD_KERNEL_NEWLIB_MAX_FDS 16
#endif

#ifndef RAD_KERNEL_NEWLIB_HEAP_BYTES
#define RAD_KERNEL_NEWLIB_HEAP_BYTES (256u * 1024u)
#endif

namespace {
struct FdEntry {
    rad_file_t file;
    int used;
    int flags;
};

FdEntry g_fds[RAD_KERNEL_NEWLIB_MAX_FDS]{};
alignas(16) unsigned char g_heap[RAD_KERNEL_NEWLIB_HEAP_BYTES]{};
size_t g_heap_used = 0;
volatile int g_fd_lock = 0;

void lock_fds() {
    while (__atomic_test_and_set(&g_fd_lock, __ATOMIC_ACQUIRE)) {
        rad_task_yield();
    }
}

void unlock_fds() {
    __atomic_clear(&g_fd_lock, __ATOMIC_RELEASE);
}

int status_to_errno(rad_status_t status) {
    switch (status) {
    case RAD_STATUS_OK: return 0;
    case RAD_STATUS_INVALID_ARGUMENT: return EINVAL;
    case RAD_STATUS_NOT_FOUND: return ENOENT;
    case RAD_STATUS_NO_MEMORY: return ENOMEM;
    case RAD_STATUS_TIMEOUT: return ETIMEDOUT;
    case RAD_STATUS_NOT_SUPPORTED: return ENOSYS;
    case RAD_STATUS_ALREADY_EXISTS: return EEXIST;
    case RAD_STATUS_NOT_INITIALIZED: return ENODEV;
    case RAD_STATUS_ERROR:
    default: return EIO;
    }
}

int fail(rad_status_t status) {
    errno = status_to_errno(status);
    return -1;
}

uint32_t translate_open_flags(int flags) {
    uint32_t out = 0;
    const int access = flags & O_ACCMODE;
    if (access == O_RDONLY) out |= RAD_VFS_READ;
    else if (access == O_WRONLY) out |= RAD_VFS_WRITE;
    else if (access == O_RDWR) out |= RAD_VFS_READ | RAD_VFS_WRITE;
    if (flags & O_CREAT) out |= RAD_VFS_CREATE;
    if (flags & O_TRUNC) out |= RAD_VFS_TRUNCATE;
    if (flags & O_APPEND) out |= RAD_VFS_APPEND;
    return out;
}

int alloc_fd(rad_file_t file, int flags) {
    lock_fds();
    for (int fd = 3; fd < RAD_KERNEL_NEWLIB_MAX_FDS; ++fd) {
        if (g_fds[fd].used) continue;
        g_fds[fd].used = 1;
        g_fds[fd].file = file;
        g_fds[fd].flags = flags;
        unlock_fds();
        return fd;
    }
    unlock_fds();
    errno = EMFILE;
    return -1;
}

FdEntry *lookup_fd(int fd) {
    if (fd < 3 || fd >= RAD_KERNEL_NEWLIB_MAX_FDS || !g_fds[fd].used) return nullptr;
    return &g_fds[fd];
}

ssize_t console_write(const void *buffer, size_t size) {
    rad_device_t console = nullptr;
    rad_status_t status = rad_device_open("/dev/console", &console);
    if (status != RAD_STATUS_OK) return fail(status);
    size_t written = 0;
    status = rad_device_write(console, buffer, size, &written);
    rad_device_close(console);
    if (status != RAD_STATUS_OK) return fail(status);
    return static_cast<ssize_t>(written);
}

ssize_t console_read(void *buffer, size_t size) {
    rad_device_t console = nullptr;
    rad_status_t status = rad_device_open("/dev/console", &console);
    if (status != RAD_STATUS_OK) return fail(status);
    size_t read = 0;
    status = rad_device_read(console, buffer, size, &read);
    rad_device_close(console);
    if (status != RAD_STATUS_OK) return fail(status);
    return static_cast<ssize_t>(read);
}

void fill_posix_stat(const rad_vfs_stat_t& rad_stat, struct stat *st) {
    memset(st, 0, sizeof(*st));
    st->st_mode = rad_stat.mode ? rad_stat.mode : (rad_stat.is_directory ? (S_IFDIR | 0755) : (S_IFREG | 0644));
    st->st_size = static_cast<off_t>(rad_stat.size);
    st->st_blksize = 512;
    st->st_blocks = static_cast<blkcnt_t>((rad_stat.size + 511u) / 512u);
    st->st_mtime = static_cast<time_t>(rad_stat.mtime_millis / 1000u);
    st->st_atime = st->st_mtime;
    st->st_ctime = st->st_mtime;
}
}

extern "C" {

int _open(const char *path, int flags, int mode) {
    (void)mode;
    if (!path) return fail(RAD_STATUS_INVALID_ARGUMENT);
    rad_file_t file = nullptr;
    rad_status_t status = rad_vfs_open(path, translate_open_flags(flags), &file);
    if (status != RAD_STATUS_OK) return fail(status);
    const int fd = alloc_fd(file, flags);
    if (fd < 0) {
        rad_vfs_close(file);
        return -1;
    }
    return fd;
}

int _close(int fd) {
    if (fd >= 0 && fd <= 2) return 0;
    lock_fds();
    FdEntry *entry = lookup_fd(fd);
    if (!entry) {
        unlock_fds();
        errno = EBADF;
        return -1;
    }
    rad_file_t file = entry->file;
    memset(entry, 0, sizeof(*entry));
    unlock_fds();
    rad_vfs_close(file);
    return 0;
}

ssize_t _read(int fd, void *buffer, size_t size) {
    if (!buffer) return fail(RAD_STATUS_INVALID_ARGUMENT);
    if (fd == 0) return console_read(buffer, size);
    lock_fds();
    FdEntry *entry = lookup_fd(fd);
    rad_file_t file = entry ? entry->file : nullptr;
    unlock_fds();
    if (!file) {
        errno = EBADF;
        return -1;
    }
    size_t read = 0;
    rad_status_t status = rad_vfs_read(file, buffer, size, &read);
    if (status != RAD_STATUS_OK) return fail(status);
    return static_cast<ssize_t>(read);
}

ssize_t _write(int fd, const void *buffer, size_t size) {
    if (!buffer) return fail(RAD_STATUS_INVALID_ARGUMENT);
    if (fd == 1 || fd == 2) return console_write(buffer, size);
    lock_fds();
    FdEntry *entry = lookup_fd(fd);
    rad_file_t file = entry ? entry->file : nullptr;
    unlock_fds();
    if (!file) {
        errno = EBADF;
        return -1;
    }
    size_t written = 0;
    rad_status_t status = rad_vfs_write(file, buffer, size, &written);
    if (status != RAD_STATUS_OK && written == 0) return fail(status);
    return static_cast<ssize_t>(written);
}

off_t _lseek(int fd, off_t offset, int whence) {
    lock_fds();
    FdEntry *entry = lookup_fd(fd);
    rad_file_t file = entry ? entry->file : nullptr;
    unlock_fds();
    if (!file) {
        errno = EBADF;
        return -1;
    }
    rad_seek_origin_t origin = RAD_SEEK_SET;
    if (whence == SEEK_CUR) origin = RAD_SEEK_CUR;
    else if (whence == SEEK_END) origin = RAD_SEEK_END;
    else if (whence != SEEK_SET) return fail(RAD_STATUS_INVALID_ARGUMENT);
    rad_status_t status = rad_vfs_seek(file, offset, origin);
    if (status != RAD_STATUS_OK) return fail(status);
    return static_cast<off_t>(rad_vfs_tell(file));
}

int _fstat(int fd, struct stat *st) {
    if (!st) return fail(RAD_STATUS_INVALID_ARGUMENT);
    if (fd >= 0 && fd <= 2) {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFCHR | 0600;
        return 0;
    }
    lock_fds();
    FdEntry *entry = lookup_fd(fd);
    rad_file_t file = entry ? entry->file : nullptr;
    unlock_fds();
    if (!file) {
        errno = EBADF;
        return -1;
    }
    const off_t current = static_cast<off_t>(rad_vfs_tell(file));
    if (rad_vfs_seek(file, 0, RAD_SEEK_END) != RAD_STATUS_OK) return fail(RAD_STATUS_ERROR);
    const uint64_t size = rad_vfs_tell(file);
    rad_vfs_seek(file, current, RAD_SEEK_SET);
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFREG | 0644;
    st->st_size = static_cast<off_t>(size);
    st->st_blksize = 512;
    st->st_blocks = static_cast<blkcnt_t>((size + 511u) / 512u);
    return 0;
}

int _stat(const char *path, struct stat *st) {
    if (!path || !st) return fail(RAD_STATUS_INVALID_ARGUMENT);
    rad_vfs_stat_t rad_stat{};
    rad_status_t status = rad_vfs_stat(path, &rad_stat);
    if (status != RAD_STATUS_OK) return fail(status);
    fill_posix_stat(rad_stat, st);
    return 0;
}

int _isatty(int fd) {
    if (fd >= 0 && fd <= 2) return 1;
    errno = ENOTTY;
    return 0;
}

int _unlink(const char *path) {
    if (!path) return fail(RAD_STATUS_INVALID_ARGUMENT);
    rad_status_t status = rad_vfs_remove(path);
    return status == RAD_STATUS_OK ? 0 : fail(status);
}

int _rename(const char *old_path, const char *new_path) {
    rad_status_t status = rad_vfs_rename(old_path, new_path);
    return status == RAD_STATUS_OK ? 0 : fail(status);
}

int _mkdir(const char *path, mode_t mode) {
    (void)mode;
    rad_status_t status = rad_vfs_mkdir(path);
    return status == RAD_STATUS_OK ? 0 : fail(status);
}

int _rmdir(const char *path) {
    rad_status_t status = rad_vfs_rmdir(path);
    return status == RAD_STATUS_OK ? 0 : fail(status);
}

int chdir(const char *path) {
    rad_status_t status = rad_vfs_chdir(path);
    return status == RAD_STATUS_OK ? 0 : fail(status);
}

char *getcwd(char *buffer, size_t size) {
    if (!buffer || size == 0) {
        errno = EINVAL;
        return nullptr;
    }
    rad_status_t status = rad_vfs_getcwd(buffer, size);
    if (status != RAD_STATUS_OK) {
        errno = status_to_errno(status);
        return nullptr;
    }
    return buffer;
}

int _gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (!tv) return fail(RAD_STATUS_INVALID_ARGUMENT);
    const uint64_t micros = rad_time_micros();
    tv->tv_sec = static_cast<time_t>(micros / 1000000u);
    tv->tv_usec = static_cast<suseconds_t>(micros % 1000000u);
    return 0;
}

int _getpid(void) {
    return 1;
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

void *_sbrk(ptrdiff_t increment) {
    if (increment < 0 || g_heap_used + static_cast<size_t>(increment) > sizeof(g_heap)) {
        errno = ENOMEM;
        return reinterpret_cast<void*>(-1);
    }
    void *previous = g_heap + g_heap_used;
    g_heap_used += static_cast<size_t>(increment);
    g_heap_used = (g_heap_used + 15u) & ~static_cast<size_t>(15u);
    return previous;
}

void _exit(int status) {
    (void)status;
    rad_kernel_request_shutdown();
    for (;;) rad_sleep_ms(1000);
}

void abort(void) {
    _exit(134);
}

}
