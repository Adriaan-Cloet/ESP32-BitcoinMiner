#include "sha256_fast.h"

#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static const uint32_t H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define BSIG1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SSIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SSIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

/* One SHA-256 block compression: fold a 64-byte block into the state. */
static void compress(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4 + 0] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SSIG1(w[i - 2]) + w[i - 7] + SSIG0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + BSIG1(e) + CH(e, f, g) + K[i] + w[i];
        uint32_t t2 = BSIG0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

/* Write eight state words as 32 big-endian bytes (standard SHA-256 output). */
static void words_to_bytes(const uint32_t s[8], uint8_t out[32])
{
    for (int i = 0; i < 8; i++) {
        out[i * 4 + 0] = (uint8_t)(s[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(s[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(s[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(s[i]);
    }
}

void sha256_midstate(const uint8_t first_block[64], uint32_t midstate[8])
{
    memcpy(midstate, H0, sizeof H0);
    compress(midstate, first_block);
}

void sha256d_finish(const uint32_t midstate[8], const uint8_t tail[16],
                    uint8_t out[32])
{
    /*
     * Second block of the first hash: the 16 tail bytes, then SHA-256 padding
     * for an 80-byte (640-bit) message: a 0x80 byte, zeros, and the length in
     * the final eight bytes.
     */
    uint8_t block[64];
    memcpy(block, tail, 16);
    block[16] = 0x80;
    memset(block + 17, 0, 64 - 17);
    block[62] = 0x02; /* 640 = 0x0280 */
    block[63] = 0x80;

    uint32_t s1[8];
    memcpy(s1, midstate, sizeof s1);
    compress(s1, block);

    /* First digest as bytes, then hash those 32 bytes once more. */
    uint8_t digest1[32];
    words_to_bytes(s1, digest1);

    uint8_t block2[64];
    memcpy(block2, digest1, 32);
    block2[32] = 0x80;
    memset(block2 + 33, 0, 64 - 33);
    block2[62] = 0x01; /* 256 = 0x0100 */
    block2[63] = 0x00;

    uint32_t s2[8];
    memcpy(s2, H0, sizeof s2);
    compress(s2, block2);

    words_to_bytes(s2, out);
}