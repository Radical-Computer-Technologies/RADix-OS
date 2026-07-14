#include "user_auth.h"

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[64];
} radix_sha256_t;

static uint32_t rotr32(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32u - bits));
}

static uint32_t load_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24u) | ((uint32_t)p[1] << 16u) | ((uint32_t)p[2] << 8u) | (uint32_t)p[3];
}

static void store_be32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24u);
    p[1] = (uint8_t)(value >> 16u);
    p[2] = (uint8_t)(value >> 8u);
    p[3] = (uint8_t)value;
}

static void store_be64(uint8_t *p, uint64_t value) {
    for (int i = 7; i >= 0; --i) {
        p[i] = (uint8_t)value;
        value >>= 8u;
    }
}

static void sha256_transform(radix_sha256_t *ctx, const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
    };
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = load_be32(block + i * 4);
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    for (int i = 0; i < 64; ++i) {
        const uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + s1 + ch + k[i] + w[i];
        const uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(radix_sha256_t *ctx) {
    ctx->state[0] = 0x6a09e667u; ctx->state[1] = 0xbb67ae85u; ctx->state[2] = 0x3c6ef372u; ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu; ctx->state[5] = 0x9b05688cu; ctx->state[6] = 0x1f83d9abu; ctx->state[7] = 0x5be0cd19u;
    ctx->bit_count = 0;
    for (int i = 0; i < 64; ++i) ctx->buffer[i] = 0;
}

static void sha256_update(radix_sha256_t *ctx, const uint8_t *data, size_t size) {
    size_t used = (size_t)((ctx->bit_count >> 3u) & 63u);
    ctx->bit_count += (uint64_t)size * 8u;
    for (size_t i = 0; i < size; ++i) {
        ctx->buffer[used++] = data[i];
        if (used == 64u) {
            sha256_transform(ctx, ctx->buffer);
            used = 0;
        }
    }
}

static void sha256_final(radix_sha256_t *ctx, uint8_t digest[32]) {
    size_t used = (size_t)((ctx->bit_count >> 3u) & 63u);
    const uint64_t bits = ctx->bit_count;
    ctx->buffer[used++] = 0x80u;
    if (used > 56u) {
        while (used < 64u) ctx->buffer[used++] = 0;
        sha256_transform(ctx, ctx->buffer);
        used = 0;
    }
    while (used < 56u) ctx->buffer[used++] = 0;
    store_be64(ctx->buffer + 56, bits);
    sha256_transform(ctx, ctx->buffer);
    for (int i = 0; i < 8; ++i) store_be32(digest + i * 4, ctx->state[i]);
}

static size_t auth_len(const char *text) {
    size_t n = 0;
    if (text) while (text[n]) ++n;
    return n;
}

static void hex_encode(const uint8_t *bytes, size_t size, char *out) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < size; ++i) {
        out[i * 2] = hex[bytes[i] >> 4u];
        out[i * 2 + 1] = hex[bytes[i] & 15u];
    }
    out[size * 2] = 0;
}

void radix_auth_sha256_hex(const char *text, char out_hex[65]) {
    radix_sha256_t ctx;
    uint8_t digest[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)(text ? text : ""), auth_len(text));
    sha256_final(&ctx, digest);
    hex_encode(digest, sizeof(digest), out_hex);
}

void radix_auth_password_hash(const char *salt, const char *password, char out_hex[65]) {
    radix_sha256_t ctx;
    uint8_t digest[32];
    const char sep = ':';
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)(salt ? salt : ""), auth_len(salt));
    sha256_update(&ctx, (const uint8_t*)&sep, 1);
    sha256_update(&ctx, (const uint8_t*)(password ? password : ""), auth_len(password));
    sha256_final(&ctx, digest);
    hex_encode(digest, sizeof(digest), out_hex);
}
