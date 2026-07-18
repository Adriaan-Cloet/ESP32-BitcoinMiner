/*
 * Phase 0 test bench for SHA-256.
 *
 * Two layers of tests:
 *
 *   1. NIST FIPS 180-4 vectors, which prove the SHA-256 implementation itself.
 *   2. Bitcoin block 125552, which additionally proves that we serialise a
 *      block header with the correct byte order.
 *
 * Test 2 is the one that matters for this project. Byte-order bugs in Bitcoin
 * are silent: everything appears to run and you simply never find a valid
 * hash. If block 125552 does not reproduce exactly, nothing built on top of
 * this can ever work.
 */

#include "test_util.h"

#include "../core/sha256_ref.h"

/* --- NIST FIPS 180-4 vectors ---------------------------------------------- */

static void test_nist_vectors(void)
{
    struct {
        const char *name;
        const char *input;
        const char *expected;
    } cases[] = {
        {
            "empty string",
            "",
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        },
        {
            "\"abc\"",
            "abc",
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        },
        {
            "56-byte message (two-block padding)",
            "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
        },
    };

    size_t i;

    printf("NIST FIPS 180-4 vectors\n");

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t digest[SHA256_DIGEST_SIZE];
        char    hex[SHA256_DIGEST_SIZE * 2 + 1];

        sha256_ref((const uint8_t *)cases[i].input, strlen(cases[i].input), digest);
        bytes_to_hex(digest, sizeof(digest), hex);
        check(cases[i].name, hex, cases[i].expected);
    }
}

/* --- Multi-block streaming ------------------------------------------------ */

static void test_long_message(void)
{
    /* One million 'a' characters. Classic FIPS test, catches bugs in the
     * length counter and in block boundary handling. */
    sha256_ref_ctx ctx;
    uint8_t        digest[SHA256_DIGEST_SIZE];
    char           hex[SHA256_DIGEST_SIZE * 2 + 1];
    uint8_t        chunk[1000];
    int            i;

    printf("\nLong message\n");

    memset(chunk, 'a', sizeof(chunk));
    sha256_ref_init(&ctx);
    for (i = 0; i < 1000; i++) {
        sha256_ref_update(&ctx, chunk, sizeof(chunk));
    }
    sha256_ref_final(&ctx, digest);
    bytes_to_hex(digest, sizeof(digest), hex);

    check("one million 'a'", hex,
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

/* --- Bitcoin block 125552, the one that really matters -------------------- */

static void test_block_125552(void)
{
    /*
     * Block 125552 is the canonical worked example from the Bitcoin wiki.
     * Its header fields, shown here the way a block explorer displays them:
     *
     *   version     1
     *   prev block  00000000000008a3a41b85b8b29ad444def299fee21793cd8b9e567eab02cd81
     *   merkle root 2b12fcf1b09288fcaff797d71e950e71ae42b91e8bdb2304758dfcffc2b620e3
     *   timestamp   0x4dd7f5c7  (2011-05-21 17:26:31 UTC)
     *   bits        0x1a44b9f2
     *   nonce       0x9546a142
     *
     * Serialised into the 80 raw header bytes, every multi-byte field is
     * little-endian, so each one appears byte-reversed relative to the
     * display form above. That reversal is exactly what this test pins down.
     */
    static const char header_hex[] =
        "01000000"                                                          /* version   */
        "81cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000"  /* prev hash */
        "e320b6c2fffc8d750423db8b1eb942ae710e951ed797f7affc8892b0f1fc122b"  /* merkle    */
        "c7f5d74d"                                                          /* time      */
        "f2b9441a"                                                          /* bits      */
        "42a14695";                                                         /* nonce     */

    /* The hash as block explorers display it: reversed, with leading zeros. */
    static const char expected_display[] =
        "00000000000000001e8d6829a8a21adc5d38d0a473b144b6765798e61f98bd1d";

    uint8_t header[80];
    uint8_t digest[SHA256_DIGEST_SIZE];
    char    hex[SHA256_DIGEST_SIZE * 2 + 1];
    size_t  header_len;

    printf("\nBitcoin block 125552\n");

    header_len = hex_to_bytes(header_hex, header, sizeof(header));
    if (header_len != 80) {
        printf("  FAIL  header is %zu bytes, expected 80\n", header_len);
        test_failures++;
        return;
    }
    printf("  PASS  header is exactly 80 bytes\n");

    sha256d_ref(header, header_len, digest);

    /* Internal order first: this is the form the target comparison uses. */
    bytes_to_hex(digest, sizeof(digest), hex);
    printf("  info  internal (little-endian) order: %s\n", hex);

    /* Then the display order, which is what we can compare to the wiki. */
    reverse_bytes(digest, sizeof(digest));
    bytes_to_hex(digest, sizeof(digest), hex);
    check("double SHA-256 of the header", hex, expected_display);
}

int main(void)
{
    printf("SHA-256 test bench\n");
    printf("==================\n\n");

    test_nist_vectors();
    test_long_message();
    test_block_125552();

    return test_report();
}