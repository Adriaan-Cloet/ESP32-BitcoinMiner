/*
 * Merkle root construction from a Stratum job.
 *
 * The pool does not send the whole merkle tree. It sends the coinbase halves
 * (from which the miner builds the coinbase and hashes it) plus a "branch": one
 * sibling hash per level of the tree. Folding the branch into the coinbase hash
 * reproduces the root without ever seeing the other transactions. That is the
 * economy of the design, and why a 4 KB job describes a block with thousands of
 * transactions.
 */
#ifndef MERKLE_H
#define MERKLE_H

#include <stddef.h>
#include <stdint.h>

#define MERKLE_HASH_SIZE 32

/*
 * Compute the merkle root from the coinbase hash and the branch.
 *
 * All hashes are 32 bytes in internal order, exactly as sha256d_ref produces
 * and as the branch hex decodes (no byte reversal anywhere in this step). At
 * each level the current hash is placed on the LEFT and the branch sibling on
 * the RIGHT, and the 64-byte pair is double-SHA256'd. This left-fold is the
 * convention Stratum branches assume.
 *
 * count may be 0, in which case the root is the coinbase hash unchanged and
 * branch is ignored (may be NULL). out_root must not alias branch, but may
 * point at its own separate buffer.
 *
 * branch is not marked const on purpose: ISO C (before C23) rejects passing a
 * plain uint8_t(*)[N] where a const-qualified one is expected, which -Wpedantic
 * turns into an error. The function still never writes through branch.
 */
void build_merkle_root(const uint8_t coinbase_hash[MERKLE_HASH_SIZE],
                       uint8_t (*branch)[MERKLE_HASH_SIZE],
                       size_t count,
                       uint8_t out_root[MERKLE_HASH_SIZE]);

#endif /* MERKLE_H */