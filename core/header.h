/*
 * Block header assembly: the 80 bytes you actually hash.
 *
 * This is the single place where Bitcoin's mixed endianness is resolved. Every
 * field arrives from mining.notify in one convention and has to land in the
 * header in another, and getting any of them wrong produces a header that hashes
 * to garbage without any error. The block 125552 golden test pins all of it
 * down at once.
 */
#ifndef HEADER_H
#define HEADER_H

#include <stdbool.h>
#include <stdint.h>

#define BLOCK_HEADER_SIZE 80

/*
 * Assemble the 80-byte header from a job's fields plus a candidate nonce.
 *
 * Field           bytes  transform from the mining.notify value
 *   version         4     hex (big-endian), reversed into the header
 *   prevhash       32     hex, reversed WITHIN each 4-byte word (the classic trap)
 *   merkle root    32     already-internal bytes from build_merkle_root, copied as-is
 *   ntime           4     hex, reversed
 *   nbits           4     hex, reversed
 *   nonce           4     written little-endian
 *
 * version_hex, prevhash_hex, ntime_hex and nbits_hex are exactly the strings
 * mining.notify delivered. Returns false if any hex field has the wrong length
 * or a non-hex character, leaving out untouched.
 */
bool assemble_header(const char *version_hex,
                     const char *prevhash_hex,
                     const uint8_t merkle_root[32],
                     const char *ntime_hex,
                     const char *nbits_hex,
                     uint32_t nonce,
                     uint8_t out[BLOCK_HEADER_SIZE]);

#endif /* HEADER_H */