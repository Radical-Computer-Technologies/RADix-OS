#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <new>

extern "C" void x86_serial_write(const char *text);

namespace {
alignas(16) uint8_t g_heap[2u * 1024u * 1024u];
size_t g_heap_used = 0;
int g_cpp_ctor_ran = 0;

struct CppRuntimeProbe {
    CppRuntimeProbe() { g_cpp_ctor_ran = 1; }
};

CppRuntimeProbe g_cpp_runtime_probe;

void *allocate_aligned(size_t size, size_t alignment) {
    if (size == 0) size = 1;
    if (alignment < 16u) alignment = 16u;
    if ((alignment & (alignment - 1u)) != 0) return nullptr;
    const size_t base = reinterpret_cast<size_t>(g_heap);
    const size_t current = base + g_heap_used;
    const size_t aligned = (current + alignment - 1u) & ~(alignment - 1u);
    const size_t offset = aligned - base;
    size = (size + 15u) & ~size_t(15u);
    if (offset > sizeof(g_heap) || size > sizeof(g_heap) - offset) return nullptr;
    g_heap_used = offset + size;
    return reinterpret_cast<void*>(aligned);
}

void put_char(char *buffer, size_t size, size_t& pos, char ch) {
    if (size && pos + 1 < size) buffer[pos] = ch;
    ++pos;
}

void put_text(char *buffer, size_t size, size_t& pos, const char *text) {
    if (!text) text = "(null)";
    while (*text) put_char(buffer, size, pos, *text++);
}

void put_unsigned(char *buffer, size_t size, size_t& pos, unsigned long long value, unsigned base, bool upper = false) {
    char tmp[32];
    size_t n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        tmp[n++] = digits[value % base];
        value /= base;
    } while (value && n < sizeof(tmp));
    while (n) put_char(buffer, size, pos, tmp[--n]);
}
}

extern "C" {

extern void (*__init_array_start[])();
extern void (*__init_array_end[])();

void *memcpy(void *dst, const void *src, size_t n) {
    auto *d = static_cast<unsigned char*>(dst);
    const auto *s = static_cast<const unsigned char*>(src);
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    auto *d = static_cast<unsigned char*>(dst);
    const auto *s = static_cast<const unsigned char*>(src);
    if (d < s) {
        for (size_t i = 0; i < n; ++i) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; --i) d[i - 1] = s[i - 1];
    }
    return dst;
}

void *memset(void *dst, int value, size_t n) {
    auto *d = static_cast<unsigned char*>(dst);
    for (size_t i = 0; i < n; ++i) d[i] = static_cast<unsigned char>(value);
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const auto *aa = static_cast<const unsigned char*>(a);
    const auto *bb = static_cast<const unsigned char*>(b);
    for (size_t i = 0; i < n; ++i) {
        if (aa[i] != bb[i]) return aa[i] < bb[i] ? -1 : 1;
    }
    return 0;
}

int bcmp(const void *a, const void *b, size_t n) {
    return memcmp(a, b, n);
}

void *memchr(const void *s, int c, size_t n) {
    const auto *p = static_cast<const unsigned char*>(s);
    for (size_t i = 0; i < n; ++i) {
        if (p[i] == static_cast<unsigned char>(c)) return const_cast<unsigned char*>(p + i);
    }
    return nullptr;
}

size_t strlen(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i] || !a[i] || !b[i]) {
            return static_cast<unsigned char>(a[i]) - static_cast<unsigned char>(b[i]);
        }
    }
    return 0;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src && src[i]; ++i) dst[i] = src[i];
    for (; i < n; ++i) dst[i] = '\0';
    return dst;
}

char *strchr(const char *s, int c) {
    while (s && *s) {
        if (*s == static_cast<char>(c)) return const_cast<char*>(s);
        ++s;
    }
    return c == 0 && s ? const_cast<char*>(s) : nullptr;
}

char *strrchr(const char *s, int c) {
    const char *last = nullptr;
    while (s && *s) {
        if (*s == static_cast<char>(c)) last = s;
        ++s;
    }
    if (c == 0 && s) return const_cast<char*>(s);
    return const_cast<char*>(last);
}

char *strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return nullptr;
    if (!*needle) return const_cast<char*>(haystack);
    for (const char *h = haystack; *h; ++h) {
        const char *a = h;
        const char *b = needle;
        while (*a && *b && *a == *b) {
            ++a;
            ++b;
        }
        if (!*b) return const_cast<char*>(h);
    }
    return nullptr;
}

int isprint(int ch) {
    return ch >= 0x20 && ch <= 0x7e;
}

int snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    size_t pos = 0;
    for (const char *p = format; p && *p; ++p) {
        if (*p != '%') {
            put_char(buffer, size, pos, *p);
            continue;
        }
        ++p;
        bool long_long = false;
        bool long_one = false;
        bool size_t_arg = false;
        if (*p == 'z') {
            size_t_arg = true;
            ++p;
        } else if (*p == 'l') {
            long_one = true;
            ++p;
            if (*p == 'l') {
                long_long = true;
                ++p;
            }
        }
        switch (*p) {
        case '%': put_char(buffer, size, pos, '%'); break;
        case 'c': put_char(buffer, size, pos, static_cast<char>(va_arg(args, int))); break;
        case 's': put_text(buffer, size, pos, va_arg(args, const char*)); break;
        case 'd':
        case 'i': {
            long long value = long_long ? va_arg(args, long long)
                : long_one ? va_arg(args, long)
                : va_arg(args, int);
            if (value < 0) {
                put_char(buffer, size, pos, '-');
                value = -value;
            }
            put_unsigned(buffer, size, pos, static_cast<unsigned long long>(value), 10);
            break;
        }
        case 'u': {
            unsigned long long value = size_t_arg ? va_arg(args, size_t)
                : long_long ? va_arg(args, unsigned long long)
                : long_one ? va_arg(args, unsigned long)
                : va_arg(args, unsigned int);
            put_unsigned(buffer, size, pos, value, 10);
            break;
        }
        case 'x':
        case 'X': {
            unsigned long long value = size_t_arg ? va_arg(args, size_t)
                : long_long ? va_arg(args, unsigned long long)
                : long_one ? va_arg(args, unsigned long)
                : va_arg(args, unsigned int);
            put_unsigned(buffer, size, pos, value, 16, *p == 'X');
            break;
        }
        case 'p': {
            auto value = reinterpret_cast<uintptr_t>(va_arg(args, void*));
            put_text(buffer, size, pos, "0x");
            put_unsigned(buffer, size, pos, value, 16);
            break;
        }
        default:
            put_char(buffer, size, pos, '%');
            put_char(buffer, size, pos, *p);
            break;
        }
    }
    if (size) {
        buffer[pos < size ? pos : size - 1] = '\0';
    }
    va_end(args);
    return static_cast<int>(pos);
}

void *malloc(size_t size) {
    return allocate_aligned(size, 16u);
}

void free(void*) {}

void abort(void) {
    x86_serial_write("abort\n");
    for (;;) asm volatile("cli; hlt");
}

void rust_eh_personality(void) {}
void _Unwind_Resume(void*) { abort(); }
void __assert_fail(const char*, const char*, unsigned int, const char*) { abort(); }

int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void *__dso_handle = nullptr;
void __cxa_pure_virtual() { abort(); }
int __cxa_guard_acquire(uint64_t *guard) { return !(*guard); }
void __cxa_guard_release(uint64_t *guard) { *guard = 1; }
void __cxa_guard_abort(uint64_t*) {}

void x86_run_global_constructors(void) {
    for (auto ctor = __init_array_start; ctor != __init_array_end; ++ctor) {
        if (*ctor) (*ctor)();
    }
}

int x86_cpp_runtime_self_test(void) {
    void *aligned = allocate_aligned(64u, 64u);
    return g_cpp_ctor_ran && aligned && (reinterpret_cast<uintptr_t>(aligned) & 63u) == 0u;
}

}

void *operator new(size_t size) { return malloc(size); }
void *operator new[](size_t size) { return malloc(size); }
void *operator new(size_t size, const std::nothrow_t&) noexcept { return malloc(size); }
void *operator new[](size_t size, const std::nothrow_t&) noexcept { return malloc(size); }
void *operator new(size_t size, std::align_val_t alignment) { return allocate_aligned(size, static_cast<size_t>(alignment)); }
void *operator new[](size_t size, std::align_val_t alignment) { return allocate_aligned(size, static_cast<size_t>(alignment)); }
void *operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return allocate_aligned(size, static_cast<size_t>(alignment)); }
void *operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return allocate_aligned(size, static_cast<size_t>(alignment)); }
void operator delete(void *ptr) noexcept { free(ptr); }
void operator delete[](void *ptr) noexcept { free(ptr); }
void operator delete(void *ptr, size_t) noexcept { free(ptr); }
void operator delete[](void *ptr, size_t) noexcept { free(ptr); }
void operator delete(void *ptr, const std::nothrow_t&) noexcept { free(ptr); }
void operator delete[](void *ptr, const std::nothrow_t&) noexcept { free(ptr); }
void operator delete(void *ptr, std::align_val_t) noexcept { free(ptr); }
void operator delete[](void *ptr, std::align_val_t) noexcept { free(ptr); }
void operator delete(void *ptr, size_t, std::align_val_t) noexcept { free(ptr); }
void operator delete[](void *ptr, size_t, std::align_val_t) noexcept { free(ptr); }
void operator delete(void *ptr, std::align_val_t, const std::nothrow_t&) noexcept { free(ptr); }
void operator delete[](void *ptr, std::align_val_t, const std::nothrow_t&) noexcept { free(ptr); }
