/*
 * Reference SHA-256 implementation (FIPS 180-4).
 * See sha256_ref.h for why this file must stay slow and readable.
 */

#include "sha256_ref.h"

/* --- Primitive operations, straight from the specification --------------- */

static uint32_t rotr(uint32_t x, unsigned n)
{
    /* n is always 1..31 here, so there is no undefined shift by 32. */
    return (x >> n) | (x << (32 - n));
}

/* Bitwise "if x then y else z". */
static uint32_t ch(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ (~x & z);
}

/* Bitwise majority vote over three inputs. */
static uint32_t maj(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t big_sigma0(uint32_t x)   { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
static uint32_t big_sigma1(uint32_t x)   { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
static uint32_t small_sigma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
static uint32_t small_sigma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

/*
 * First 32 bits of the fractional parts of the cube roots of the first 64
 * primes. These are "nothing up my sleeve" numbers: constants with an obvious
 * public derivation, so nobody can have chosen them to hide a weakness.
 */
static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

/* --- Core compression function ------------------------------------------- */

/*
 * Process exactly one 64-byte block and fold it into ctx->state.
 *
 * Note the byte order: SHA-256 reads its input as BIG-endian 32-bit words.
 * That is a property of the algorithm, not of the machine, so we assemble the
 * words explicitly instead of casting the pointer to uint32_t*. A cast would
 * silently give the wrong answer on a little-endian CPU, which is every CPU
 * in this project.
 */
static void sha256_ref_compress(sha256_ref_ctx *ctx, const uint8_t block[SHA256_BLOCK_SIZE])
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    unsigned t;

    /* Message schedule: the first 16 words come straight from the block. */
    for (t = 0; t < 16; t++) {
        w[t] = ((uint32_t)block[t * 4 + 0] << 24) |
               ((uint32_t)block[t * 4 + 1] << 16) |
               ((uint32_t)block[t * 4 + 2] <<  8) |
               ((uint32_t)block[t * 4 + 3]);
    }

    /* The remaining 48 words are expanded from the first 16. */
    for (t = 16; t < 64; t++) {
        w[t] = small_sigma1(w[t - 2]) + w[t - 7] + small_sigma0(w[t - 15]) + w[t - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (t = 0; t < 64; t++) {
        uint32_t t1 = h + big_sigma1(e) + ch(e, f, g) + K[t] + w[t];
        uint32_t t2 = big_sigma0(a) + maj(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /*
     * Davies-Meyer feed-forward: add the incoming state to the outgoing one.
     * The 64 rounds above are invertible on their own. This one addition is
     * what makes the construction one-way, because undoing it would require
     * the very value you are trying to recover.
     */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

/* --- Public interface ----------------------------------------------------- */

void sha256_ref_init(sha256_ref_ctx *ctx)
{
    /* First 32 bits of the fractional parts of the square roots of the first
     * 8 primes. Same "nothing up my sleeve" reasoning as K above. */
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;

    ctx->bitlen     = 0;
    ctx->buffer_len = 0;
}

void sha256_ref_update(sha256_ref_ctx *ctx, const uint8_t *data, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buffer_len++] = data[i];

        if (ctx->buffer_len == SHA256_BLOCK_SIZE) {
            sha256_ref_compress(ctx, ctx->buffer);
            ctx->bitlen += SHA256_BLOCK_SIZE * 8;
            ctx->buffer_len = 0;
        }
    }
}

void sha256_ref_final(sha256_ref_ctx *ctx, uint8_t digest[SHA256_DIGEST_SIZE])
{
    size_t   i    = ctx->buffer_len;
    uint64_t bits = ctx->bitlen + (uint64_t)ctx->buffer_len * 8;

    /*
     * Padding: one 1 bit, then zeros, then the 64-bit big-endian message
     * length, chosen so the total is a multiple of 64 bytes.
     */
    ctx->buffer[i++] = 0x80;

    if (i > SHA256_BLOCK_SIZE - 8) {
        /* No room left for the length field: pad this block out with zeros
         * and put the length in a second, otherwise empty, block. */
        while (i < SHA256_BLOCK_SIZE) {
            ctx->buffer[i++] = 0x00;
        }
        sha256_ref_compress(ctx, ctx->buffer);
        i = 0;
    }

    while (i < SHA256_BLOCK_SIZE - 8) {
        ctx->buffer[i++] = 0x00;
    }

    ctx->buffer[56] = (uint8_t)(bits >> 56);
    ctx->buffer[57] = (uint8_t)(bits >> 48);
    ctx->buffer[58] = (uint8_t)(bits >> 40);
    ctx->buffer[59] = (uint8_t)(bits >> 32);
    ctx->buffer[60] = (uint8_t)(bits >> 24);
    ctx->buffer[61] = (uint8_t)(bits >> 16);
    ctx->buffer[62] = (uint8_t)(bits >>  8);
    ctx->buffer[63] = (uint8_t)(bits);

    sha256_ref_compress(ctx, ctx->buffer);

    /* Serialise the final state as big-endian bytes. */
    for (i = 0; i < 8; i++) {
        digest[i * 4 + 0] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >>  8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

void sha256_ref(const uint8_t *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE])
{
    sha256_ref_ctx ctx;

    sha256_ref_init(&ctx);
    sha256_ref_update(&ctx, data, len);
    sha256_ref_final(&ctx, digest);
}

void sha256d_ref(const uint8_t *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE])
{
    uint8_t first[SHA256_DIGEST_SIZE];

    sha256_ref(data, len, first);
    sha256_ref(first, SHA256_DIGEST_SIZE, digest);
}