/*
 * Optimised SHA-256 for the mining hot path.
 *
 * The reference implementation in sha256_ref is written to be obviously
 * correct, not fast. This one is written to be fast, and is kept honest by a
 * differential test that compares it against the reference over many inputs.
 *
 * The key trick is the "midstate". A block header is 80 bytes = two SHA-256
 * blocks, and the nonce lives in the last four bytes, i.e. in the second block.
 * The first block (bytes 0..63) is therefore constant while only the nonce
 * changes, so its compression is done once per job/extranonce2 and reused for
 * every one of the ~4 billion nonces. That removes roughly a third of the work
 * per hash before any other optimisation.
 */
#ifndef SHA256_FAST_H
#define SHA256_FAST_H

#include <stdint.h>

/*
 * Compress the first 64-byte block of the header into a midstate: the SHA-256
 * chaining value after that block. Call once whenever bytes 0..63 change (a new
 * job or a new extranonce2).
 */
void sha256_midstate(const uint8_t first_block[64], uint32_t midstate[8]);

/*
 * Finish the double hash of an 80-byte header from a precomputed midstate and
 * the final 16 header bytes (bytes 64..79: the tail of the merkle root, ntime,
 * nbits and the nonce). Writes the 32-byte double-SHA-256 in the same byte order
 * as sha256d_ref, ready for meets_target.
 */
void sha256d_finish(const uint32_t midstate[8], const uint8_t tail[16],
                    uint8_t out[32]);

#endif /* SHA256_FAST_H */