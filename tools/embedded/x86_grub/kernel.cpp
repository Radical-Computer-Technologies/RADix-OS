#include <stdint.h>
#include <stddef.h>

namespace {
constexpr uint32_t Multiboot2BootloaderMagic = 0x36d76289u;
constexpr uint16_t TagFramebuffer = 8u;

struct Mb2Tag {
    uint32_t type;
    uint32_t size;
};

struct Mb2FramebufferTag {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t kind;
    uint16_t reserved;
};

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void serial_init() {
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x80);
    outb(0x3f8 + 0, 0x03);
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x03);
    outb(0x3f8 + 2, 0xc7);
    outb(0x3f8 + 4, 0x0b);
}

int serial_ready() {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(0x3f8 + 5));
    return value & 0x20;
}

void serial_put(char c) {
    while (!serial_ready()) {}
    outb(0x3f8, static_cast<uint8_t>(c));
}

void serial_write(const char *text) {
    while (text && *text) {
        if (*text == '\n') {
            serial_put('\r');
        }
        serial_put(*text++);
    }
}

void serial_hex(uint32_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    serial_write("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        serial_put(digits[(value >> shift) & 0xf]);
    }
}

void fill_framebuffer(const Mb2FramebufferTag *fb) {
    if (!fb || fb->bpp != 32 || fb->addr == 0) {
        return;
    }
    auto *pixels = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(fb->addr));
    const uint32_t stride = fb->pitch / sizeof(uint32_t);
    for (uint32_t y = 0; y < fb->height; ++y) {
        for (uint32_t x = 0; x < fb->width; ++x) {
            const uint8_t r = static_cast<uint8_t>((x * 255u) / (fb->width ? fb->width : 1u));
            const uint8_t g = static_cast<uint8_t>((y * 255u) / (fb->height ? fb->height : 1u));
            pixels[y * stride + x] = 0xff000000u | (uint32_t(r) << 16u) | (uint32_t(g) << 8u) | 0x40u;
        }
    }
}

const Mb2FramebufferTag *find_framebuffer(uint32_t info_addr) {
    const auto *base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(info_addr));
    const uint32_t total_size = *reinterpret_cast<const uint32_t*>(base);
    size_t offset = 8;
    while (offset + sizeof(Mb2Tag) <= total_size) {
        const auto *tag = reinterpret_cast<const Mb2Tag*>(base + offset);
        if (tag->type == 0) {
            break;
        }
        if (tag->type == TagFramebuffer && tag->size >= sizeof(Mb2FramebufferTag)) {
            return reinterpret_cast<const Mb2FramebufferTag*>(tag);
        }
        offset += (tag->size + 7u) & ~size_t(7u);
    }
    return nullptr;
}
}

extern "C" void kernel_main(uint32_t magic, uint32_t info_addr) {
    serial_init();
    serial_write("RAD x86 GRUB handoff\n");
    serial_write("magic=");
    serial_hex(magic);
    serial_write(" info=");
    serial_hex(info_addr);
    serial_write("\n");

    if (magic != Multiboot2BootloaderMagic) {
        serial_write("invalid multiboot2 magic\n");
        return;
    }

    const Mb2FramebufferTag *fb = find_framebuffer(info_addr);
    if (fb) {
        serial_write("framebuffer=yes width=");
        serial_hex(fb->width);
        serial_write(" height=");
        serial_hex(fb->height);
        serial_write(" bpp=");
        serial_hex(fb->bpp);
        serial_write("\nprovider=grub connector=multiboot2-framebuffer primary=1\n");
        fill_framebuffer(fb);
    } else {
        serial_write("framebuffer=no\n");
    }
    serial_write("RAD x86 GRUB smoke ready\n");
}
