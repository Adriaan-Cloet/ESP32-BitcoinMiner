/*
 * Convert a Stratum share difficulty into a target.
 *
 * The pool sends a difficulty with mining.set_difficulty; a share is accepted
 * when the block hash is at or below the corresponding target. A higher
 * difficulty is a smaller target, i.e. more leading zero bits required. This is
 * separate from the network target that bits_to_target derives: the share
 * target is deliberately far easier, so a tiny miner produces visible shares.
 */
#ifndef DIFFICULTY_H
#define DIFFICULTY_H

#include <stdbool.h>
#include <stdint.h>

#include "target.h" /* TARGET_SIZE and the little-endian byte-order contract */

/*
 * Write the share target for the given difficulty into target, in the same
 * little-endian order as bits_to_target, so meets_target compares it directly
 * against a block hash. Returns false for a non-positive or NaN difficulty.
 */
bool difficulty_to_target(double difficulty, uint8_t target[TARGET_SIZE]);

#endif /* DIFFICULTY_H */