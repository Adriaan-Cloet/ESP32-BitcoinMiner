/*
 * Header-shaped SHA-256 with midstate reuse. The block compression itself lives
 * in sha256_compress (C or Xtensa assembly); this file is the wrapper that turns
 * it into the double hash the miner needs.
 */
#include "sha256_fast.h"

#include <string.h>

#include "sha256_compress.h"

static const uint32_t H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

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
    sha256_compress(midstate, first_block);
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
    sha256_compress(s1, block);

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
    sha256_compress(s2, block2);

    words_to_bytes(s2, out);
}