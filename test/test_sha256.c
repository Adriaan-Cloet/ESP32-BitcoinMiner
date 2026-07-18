/*
 * Phase 0 test bench.
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

#include <stdio.h>
#include <string.h>

#include "../core/sha256_ref.h"

static int failures = 0;

/* --- Small helpers -------------------------------------------------------- */

/* Parse a hex string into bytes. Returns the number of bytes written. */
static size_t hex_to_bytes(const char *hex, uint8_t *out, size_t out_size)
{
    size_t n = 0;

    while (hex[0] && hex[1] && n < out_size) {
        unsigned value;
        if (sscanf(hex, "%2x", &value) != 1) {
            break;
        }
        out[n++] = (uint8_t)value;
        hex += 2;
    }
    return n;
}

static void bytes_to_hex(const uint8_t *data, size_t len, char *out)
{
    static const char digits[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < len; i++) {
        out[i * 2 + 0] = digits[data[i] >> 4];
        out[i * 2 + 1] = digits[data[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

/*
 * Reverse a byte array in place.
 *
 * Bitcoin displays hashes in the reverse of their internal byte order. The
 * internal order is what you hash and compare against the target; the display
 * order is what block explorers show. Mixing these up is the single most
 * common source of confusion in this codebase, so the conversion is always
 * explicit and never implicit.
 */
static void reverse_bytes(uint8_t *data, size_t len)
{
    size_t i;

    for (i = 0; i < len / 2; i++) {
        uint8_t tmp        = data[i];
        data[i]            = data[len - 1 - i];
        data[len - 1 - i]  = tmp;
    }
}

static void check(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) == 0) {
        printf("  PASS  %s\n", name);
    } else {
        printf("  FAIL  %s\n", name);
        printf("        expected: %s\n", want);
        printf("        got:      %s\n", got);
        failures++;
    }
}

/* --- Test 1: NIST FIPS 180-4 vectors -------------------------------------- */

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

/* --- Test 2: a long message, to exercise multi-block streaming ------------ */

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

/* --- Test 3: Bitcoin block 125552, the one that really matters ------------ */

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
        failures++;
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
    printf("Phase 0 test bench\n");
    printf("==================\n\n");

    test_nist_vectors();
    test_long_message();
    test_block_125552();

    printf("\n");
    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }

    printf("%d test(s) FAILED.\n", failures);
    return 1;
}