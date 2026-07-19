/*
 * Tests for merkle root construction.
 *
 * There is no hardcoded expected value here. Each expectation is computed with
 * the already-trusted sha256d_ref, so the test checks that build_merkle_root
 * folds the branch in the right order and combines the bytes correctly, without
 * depending on any precomputed vector. A real-block vector can be added on top
 * once we capture one from a live job.
 */
#include <stdio.h>
#include <string.h>

#include "merkle.h"
#include "sha256_ref.h"
#include "test_util.h"

int main(void)
{
    printf("merkle: build_merkle_root\n");

    uint8_t coinbase[32];
    uint8_t b0[32];
    uint8_t b1[32];
    for (int i = 0; i < 32; i++) {
        coinbase[i] = (uint8_t)i;
        b0[i]       = (uint8_t)(0xff - i);
        b1[i]       = (uint8_t)(i * 7);
    }

    uint8_t root[32];
    char got[65];
    char want[65];

    /* Empty branch: the root is just the coinbase hash. */
    build_merkle_root(coinbase, NULL, 0, root);
    bytes_to_hex(root, 32, got);
    bytes_to_hex(coinbase, 32, want);
    check("empty branch returns the coinbase hash", got, want);

    /* One level: root == sha256d(coinbase || b0). */
    uint8_t pair[64];
    uint8_t expected1[32];
    memcpy(pair, coinbase, 32);
    memcpy(pair + 32, b0, 32);
    sha256d_ref(pair, sizeof pair, expected1);

    uint8_t branch1[1][32];
    memcpy(branch1[0], b0, 32);
    build_merkle_root(coinbase, branch1, 1, root);
    bytes_to_hex(root, 32, got);
    bytes_to_hex(expected1, 32, want);
    check("single branch folds once", got, want);

    /* Two levels: root == sha256d(sha256d(coinbase || b0) || b1). */
    uint8_t expected2[32];
    memcpy(pair, expected1, 32);
    memcpy(pair + 32, b1, 32);
    sha256d_ref(pair, sizeof pair, expected2);

    uint8_t branch2[2][32];
    memcpy(branch2[0], b0, 32);
    memcpy(branch2[1], b1, 32);
    build_merkle_root(coinbase, branch2, 2, root);
    bytes_to_hex(root, 32, got);
    bytes_to_hex(expected2, 32, want);
    check("two branches fold left to right", got, want);

    return test_report();
}