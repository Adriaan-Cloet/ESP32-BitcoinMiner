#include "merkle.h"

#include <string.h>

#include "sha256_ref.h"

void build_merkle_root(const uint8_t coinbase_hash[MERKLE_HASH_SIZE],
                       uint8_t (*branch)[MERKLE_HASH_SIZE],
                       size_t count,
                       uint8_t out_root[MERKLE_HASH_SIZE])
{
    uint8_t current[MERKLE_HASH_SIZE];
    memcpy(current, coinbase_hash, MERKLE_HASH_SIZE);

    /* pair = current (left) || branch[i] (right), double-hashed each level. */
    uint8_t pair[2 * MERKLE_HASH_SIZE];
    for (size_t i = 0; i < count; i++) {
        memcpy(pair, current, MERKLE_HASH_SIZE);
        memcpy(pair + MERKLE_HASH_SIZE, branch[i], MERKLE_HASH_SIZE);
        sha256d_ref(pair, sizeof pair, current);
    }

    memcpy(out_root, current, MERKLE_HASH_SIZE);
}