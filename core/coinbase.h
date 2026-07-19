/*
 * Coinbase transaction assembly and payout verification.
 *
 * The pool builds the coinbase, not the miner, and splits it around the point
 * where the miner's extranonce goes:
 *
 *     coinbase = coinb1 + extranonce1 + extranonce2 + coinb2
 *
 * coinb1 and coinb2 come from mining.notify, extranonce1 from mining.subscribe,
 * and extranonce2 is chosen by the miner (this is what lets one miner search a
 * different part of the space than another). Double-SHA256 of the assembled
 * coinbase is the leaf that build_merkle_root folds the branch into.
 *
 * The payout output, the one paying your address, is embedded inside coinb1 or
 * coinb2. Verifying that your own scriptPubKey is really in there is the only
 * hard guarantee that you are mining for yourself and not, unknowingly, for the
 * pool operator.
 */
#ifndef COINBASE_H
#define COINBASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Generous ceiling; a real coinbase is a few hundred bytes at most. */
#define COINBASE_MAX_BYTES 1024

/*
 * Decode and concatenate the four hex pieces into out. Returns the coinbase
 * length in bytes, or -1 if any piece has odd length or a non-hex character, or
 * the total exceeds out_size. Any piece may be the empty string.
 */
int coinbase_assemble(const char *coinb1_hex,
                      const char *extranonce1_hex,
                      const char *extranonce2_hex,
                      const char *coinb2_hex,
                      uint8_t *out, size_t out_size);

/*
 * Return true if scriptpubkey_hex occurs verbatim in the assembled coinbase
 * bytes. For a bech32 P2WPKH address the scriptPubKey is "0014" followed by the
 * 20-byte key hash.
 */
bool coinbase_pays_script(const uint8_t *coinbase, size_t len,
                          const char *scriptpubkey_hex);

#endif /* COINBASE_H */