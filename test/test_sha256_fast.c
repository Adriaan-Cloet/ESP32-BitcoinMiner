/*
 * Differential test: the optimised SHA-256 must agree with the reference on
 * every input. The reference is the oracle; if sha256d_finish ever disagrees
 * with it, the fast path is wrong, full stop.
 *
 * Inputs are 80-byte headers (the shape the miner actually hashes), drawn from a
 * fixed-seed PRNG so the run is reproducible. The block 125552 header is thrown
 * in as a named, known-answer case on top of the random sweep.
 */
#include <stdio.h>
#include <string.h>

#include "sha256_fast.h"
#include "sha256_ref.h"
#include "test_util.h"

static uint32_t rng_state = 0x01234567u;

static uint32_t xorshift(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

int main(void)
{
    printf("sha256_fast: differential vs reference\n");

    int mismatches = 0;
    for (int t = 0; t < 50000; t++) {
        uint8_t header[80];
        for (int i = 0; i < 80; i++) {
            header[i] = (uint8_t)(xorshift() & 0xff);
        }

        uint8_t ref[32];
        sha256d_ref(header, 80, ref);

        uint32_t mid[8];
        sha256_midstate(header, mid);
        uint8_t fast[32];
        sha256d_finish(mid, header + 64, fast);

        if (memcmp(ref, fast, 32) != 0) {
            if (mismatches < 3) {
                char a[65];
                char b[65];
                bytes_to_hex(ref, 32, a);
                bytes_to_hex(fast, 32, b);
                printf("  MISMATCH at header %d\n    ref:  %s\n    fast: %s\n",
                       t, a, b);
            }
            mismatches++;
        }
    }
    char m[16];
    snprintf(m, sizeof m, "%d", mismatches);
    check("50000 random headers match the reference", m, "0");

    /* Named case: block 125552, whose double hash is published. */
    uint8_t h[80];
    hex_to_bytes(
        "0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000"
        "e320b6c2fffc8d750423db8b1eb942ae710e951ed797f7affc8892b0f1fc122b"
        "c7f5d74df2b9441a42a14695",
        h, sizeof h);

    uint32_t mid[8];
    sha256_midstate(h, mid);
    uint8_t fast[32];
    sha256d_finish(mid, h + 64, fast);
    char hx[65];
    bytes_to_hex(fast, 32, hx);
    check("block 125552 hash", hx,
          "1dbd981fe6985776b644b173a4d0385ddc1aa2a829688d1e0000000000000000");

    return test_report();
}