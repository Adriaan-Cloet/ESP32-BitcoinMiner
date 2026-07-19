/*
 * Tests for coinbase assembly and payout verification.
 *
 * The assembly is a straight concatenation of decoded hex, so it can be checked
 * against a hand-written expected string with no hashing involved. The payout
 * check uses a scriptPubKey planted inside coinb2.
 */
#include <stdio.h>

#include "coinbase.h"
#include "test_util.h"

int main(void)
{
    printf("coinbase: assemble and verify\n");

    uint8_t buf[COINBASE_MAX_BYTES];

    /* Four pieces concatenate in order; extranonce1/2 fill the middle. */
    int n = coinbase_assemble("01000000", "abcd", "0000", "ffffffff",
                              buf, sizeof buf);
    check_bool("assembles", n > 0, 1);

    char hex[2 * COINBASE_MAX_BYTES + 1];
    bytes_to_hex(buf, (size_t)n, hex);
    check("coinbase bytes", hex, "01000000abcd0000ffffffff");

    /* Empty extranonce pieces are allowed. */
    n = coinbase_assemble("aa", "", "", "bb", buf, sizeof buf);
    bytes_to_hex(buf, (size_t)n, hex);
    check("empty extranonce pieces", hex, "aabb");

    /* Odd-length hex is rejected, not truncated. */
    check_bool("rejects odd-length hex",
               coinbase_assemble("0", "", "", "", buf, sizeof buf) < 0, 1);

    /*
     * Payout verification. A P2WPKH scriptPubKey (0014 + 20 bytes) is planted
     * inside coinb2; the real coinbase carries it the same way.
     */
    const char *script = "0014aabbccddeeff00112233445566778899aabbccdd";
    n = coinbase_assemble("01000000", "1234", "5678",
                          "ee0014aabbccddeeff00112233445566778899aabbccddee",
                          buf, sizeof buf);
    check_bool("assembles with a script", n > 0, 1);
    check_bool("finds our scriptPubKey", coinbase_pays_script(buf, (size_t)n, script), 1);
    check_bool("rejects a different scriptPubKey",
               coinbase_pays_script(buf, (size_t)n,
                   "0014ffffffffffffffffffffffffffffffffffffffff"), 0);

    return test_report();
}