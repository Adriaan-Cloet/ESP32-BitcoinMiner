/*
 * Target arithmetic: the "is this hash good enough" side of mining.
 *
 * BYTE ORDER, READ THIS FIRST
 * ---------------------------
 * Both a block hash and a target are 256-bit numbers. Inside this project they
 * are always stored as 32 bytes in LITTLE-ENDIAN order, meaning byte[0] is the
 * least significant and byte[31] the most significant.
 *
 * That is the same layout sha256d() produces, so no conversion is ever needed
 * in the mining loop. It is NOT the order block explorers display: they show
 * the reverse. A hash that "starts with many zeros" on a website therefore
 * ENDS with many zero bytes in this representation.
 *
 * Every conversion between the two orders is explicit and lives in the caller.
 */

#ifndef TARGET_H
#define TARGET_H

#include <stdbool.h>
#include <stdint.h>

#define TARGET_SIZE 32

/*
 * Expand the compact "nBits" representation into a full 256-bit target.
 *
 * nBits is a miniature floating point format packed into 4 bytes:
 *   - the top byte is an exponent
 *   - the low three bytes are a mantissa
 *   - target = mantissa * 256^(exponent - 3)
 *
 * Returns false and leaves target zeroed when nBits is not valid:
 *   - the 0x00800000 bit is a sign bit; a negative target is meaningless
 *   - an exponent large enough to overflow 256 bits
 *
 * Rejecting these instead of silently truncating matters: a wrong target does
 * not crash, it just means you mine forever against the wrong threshold.
 */
bool bits_to_target(uint32_t nbits, uint8_t target[TARGET_SIZE]);

/*
 * Return true when hash <= target, both interpreted as little-endian 256-bit
 * numbers. Equality counts as a hit, matching Bitcoin consensus rules.
 */
bool meets_target(const uint8_t hash[TARGET_SIZE], const uint8_t target[TARGET_SIZE]);

#endif /* TARGET_H */