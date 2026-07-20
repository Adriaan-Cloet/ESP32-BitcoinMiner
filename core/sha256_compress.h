/*
 * SHA-256 block compression: the hot inner of the miner.
 *
 * Two implementations of this one function live in the repo and the build picks
 * one:
 *   - sha256_compress.c          portable C (used on the host and as a fallback)
 *   - sha256_compress_xtensa.S   hand-written Xtensa assembly (ESP32 only)
 *
 * They must be interchangeable, which the on-device differential test enforces:
 * whichever is compiled has to agree with the reference SHA-256 exactly.
 */
#ifndef SHA256_COMPRESS_H
#define SHA256_COMPRESS_H

#include <stdint.h>

/*
 * Fold one 64-byte block (big-endian words) into the eight-word state, in
 * place. This is the standard SHA-256 compression function; padding and message
 * length are the caller's job.
 */
void sha256_compress(uint32_t state[8], const uint8_t block[64]);

#endif /* SHA256_COMPRESS_H */