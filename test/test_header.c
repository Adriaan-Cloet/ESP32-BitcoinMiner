/*
 * The block 125552 golden test.
 *
 * This is the most important test in the project. Block 125552 is the canonical
 * worked example from the Bitcoin wiki: a known header and a known hash. Its
 * fields are fed in here exactly as mining.notify would deliver them, and the
 * assembled 80-byte header and its double-SHA256 are checked against the
 * published values. If this passes, every endianness decision in header.c is
 * correct. If it fails, there is no chance the miner ever produces a valid hash.
 */
#include <stdio.h>

#include "header.h"
#include "sha256_ref.h"
#include "test_util.h"

int main(void)
{
    printf("header: block 125552 golden test\n");

    /* Block 125552 fields, in mining.notify format (big-endian hex). */
    const char *version  = "00000001";
    const char *prevhash =
        "ab02cd818b9e567ee21793cddef299feb29ad444a41b85b8000008a300000000";
    const char *ntime    = "4dd7f5c7";
    const char *nbits    = "1a44b9f2";
    uint32_t    nonce    = 0x9546a142u;

    /* The merkle root as it sits in the header (internal byte order). */
    uint8_t merkle[32];
    hex_to_bytes(
        "e320b6c2fffc8d750423db8b1eb942ae710e951ed797f7affc8892b0f1fc122b",
        merkle, sizeof merkle);

    uint8_t header[BLOCK_HEADER_SIZE];
    check_bool("assembles",
               assemble_header(version, prevhash, merkle, ntime, nbits, nonce, header), 1);

    char header_hex[2 * BLOCK_HEADER_SIZE + 1];
    bytes_to_hex(header, BLOCK_HEADER_SIZE, header_hex);
    check("header bytes", header_hex,
          "0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000"
          "e320b6c2fffc8d750423db8b1eb942ae710e951ed797f7affc8892b0f1fc122b"
          "c7f5d74df2b9441a42a14695");

    /* Double-SHA256 of the header, in internal order. A block explorer shows
     * the reverse of this. */
    uint8_t hash[32];
    sha256d_ref(header, BLOCK_HEADER_SIZE, hash);
    char hash_hex[65];
    bytes_to_hex(hash, 32, hash_hex);
    check("block hash (internal order)", hash_hex,
          "1dbd981fe6985776b644b173a4d0385ddc1aa2a829688d1e0000000000000000");

    /* A malformed field must be rejected, not quietly assembled. */
    check_bool("rejects a short version field",
               assemble_header("0001", prevhash, merkle, ntime, nbits, nonce, header), 0);

    return test_report();
}