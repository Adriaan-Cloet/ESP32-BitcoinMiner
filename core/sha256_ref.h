/*
 * Reference SHA-256 implementation (FIPS 180-4).
 *
 * This file is deliberately written for clarity, not for speed. It exists to
 * be the oracle: the optimised miner implementation is verified against this
 * one with differential tests. It never runs in the mining hot path.
 *
 * Do not optimise this file. If it ever becomes fast, it has stopped being
 * obviously correct, and it loses its only reason to exist.
 */

#ifndef SHA256_REF_H
#define SHA256_REF_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

typedef struct {
    uint32_t state[8];                  /* current chaining value H0..H7   */
    uint64_t bitlen;                    /* message length in bits so far   */
    uint8_t  buffer[SHA256_BLOCK_SIZE];
    size_t   buffer_len;                /* bytes currently held in buffer  */
} sha256_ref_ctx;

/* Streaming interface. */
void sha256_ref_init(sha256_ref_ctx *ctx);
void sha256_ref_update(sha256_ref_ctx *ctx, const uint8_t *data, size_t len);
void sha256_ref_final(sha256_ref_ctx *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

/* One-shot convenience wrapper. */
void sha256_ref(const uint8_t *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

/*
 * Double SHA-256, as used everywhere in Bitcoin: SHA256(SHA256(data)).
 * The second pass runs over the 32 raw bytes of the first digest.
 */
void sha256d_ref(const uint8_t *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

#endif /* SHA256_REF_H */