/*
 * Tests for target arithmetic.
 *
 * The anchor case is Bitcoin block 125552 again, because we already know its
 * real nBits and its real hash. Proving that this block satisfies its own
 * target ties the whole chain together: header serialisation, double SHA-256,
 * nBits expansion and the comparison all have to be right simultaneously for
 * this to pass.
 */

#include "test_util.h"

#include "../core/sha256_ref.h"
#include "../core/target.h"

/* Block 125552, the same header as in test_sha256.c. */
static const char BLOCK_125552_HEADER[] =
    "01000000"
    "81cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000"
    "e320b6c2fffc8d750423db8b1eb942ae710e951ed797f7affc8892b0f1fc122b"
    "c7f5d74d"
    "f2b9441a"
    "42a14695";

/* --- nBits expansion ------------------------------------------------------ */

static void test_bits_to_target(void)
{
    struct {
        const char *name;
        uint32_t    nbits;
        bool        expect_valid;
        const char *expected_display; /* big-endian, as explorers show it */
    } cases[] = {
        {
            "block 125552 (0x1a44b9f2)",
            0x1a44b9f2u, true,
            "00000000000044b9f20000000000000000000000000000000000000000000000"
        },
        {
            "difficulty 1 (0x1d00ffff)",
            0x1d00ffffu, true,
            "00000000ffff0000000000000000000000000000000000000000000000000000"
        },
        {
            "genesis-era maximum, same as difficulty 1",
            0x1d00ffffu, true,
            "00000000ffff0000000000000000000000000000000000000000000000000000"
        },
        {
            "small exponent (0x01003456) shifts down",
            0x01003456u, true,
            "0000000000000000000000000000000000000000000000000000000000000000"
        },
        {
            "zero mantissa (0x00000000)",
            0x00000000u, true,
            "0000000000000000000000000000000000000000000000000000000000000000"
        },
    };

    size_t i;

    printf("nBits expansion\n");

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t target[TARGET_SIZE];
        char    hex[TARGET_SIZE * 2 + 1];
        bool    ok = bits_to_target(cases[i].nbits, target);

        check_bool(cases[i].name, ok, cases[i].expect_valid);
        if (!ok) {
            continue;
        }

        /* Convert to display order so the expectation is readable. */
        reverse_bytes(target, TARGET_SIZE);
        bytes_to_hex(target, TARGET_SIZE, hex);
        check("  -> expanded value", hex, cases[i].expected_display);
    }
}

static void test_bits_rejects_malformed(void)
{
    uint8_t target[TARGET_SIZE];

    printf("\nnBits rejection\n");

    /* Sign bit set: a negative target is meaningless. */
    check_bool("rejects sign bit (0x01800000)",
               bits_to_target(0x01800000u, target), false);

    /* Exponent far past the end of a 256-bit number. */
    check_bool("rejects overflowing exponent (0xff123456)",
               bits_to_target(0xff123456u, target), false);

    check_bool("rejects exponent 34 (0x22123456)",
               bits_to_target(0x22123456u, target), false);
}

/* --- Comparison ----------------------------------------------------------- */

static void test_meets_target(void)
{
    uint8_t target[TARGET_SIZE];
    uint8_t hash[TARGET_SIZE];

    printf("\nTarget comparison\n");

    bits_to_target(0x1d00ffffu, target);

    /* A hash of all zeros is the smallest possible value: always a hit. */
    memset(hash, 0x00, sizeof(hash));
    check_bool("all-zero hash meets any target", meets_target(hash, target), true);

    /* A hash of all 0xff is the largest possible value: never a hit. */
    memset(hash, 0xff, sizeof(hash));
    check_bool("all-ones hash meets nothing", meets_target(hash, target), false);

    /* Exactly equal must count as a hit. */
    memcpy(hash, target, sizeof(hash));
    check_bool("hash == target is a hit", meets_target(hash, target), true);

    /*
     * Exactly one unit above the target must miss.
     *
     * For the difficulty-1 target every byte below index 26 is zero, so
     * setting byte 0 to 1 adds exactly one to the number. Note that we must
     * NOT bump byte 26 here: it holds 0xff, so incrementing it would wrap to
     * 0x00 and make the value smaller instead of larger. Off-by-one thinking
     * on little-endian byte arrays is exactly the trap this project is full of.
     */
    memcpy(hash, target, sizeof(hash));
    hash[0] = 0x01;
    check_bool("one unit above target misses", meets_target(hash, target), false);

    /* Just below the target must hit: drop the top mantissa byte by one. */
    memcpy(hash, target, sizeof(hash));
    hash[26] = 0xfe;
    check_bool("just below target hits", meets_target(hash, target), true);
}

/* --- The integration test that ties everything together ------------------- */

static void test_block_125552_meets_its_own_target(void)
{
    uint8_t header[80];
    uint8_t hash[SHA256_DIGEST_SIZE];
    uint8_t target[TARGET_SIZE];
    uint8_t network_hash[SHA256_DIGEST_SIZE];
    char    hex[TARGET_SIZE * 2 + 1];

    printf("\nBlock 125552 against its own target\n");

    hex_to_bytes(BLOCK_125552_HEADER, header, sizeof(header));
    sha256d_ref(header, sizeof(header), hash);

    /* nBits is bytes 72..75 of the header, little-endian: 0x1a44b9f2. */
    check_bool("nBits expands cleanly", bits_to_target(0x1a44b9f2u, target), true);

    check_bool("the mined block satisfies its own target",
               meets_target(hash, target), true);

    /* Show both in display order, so the leading zeros line up visually. */
    memcpy(network_hash, hash, sizeof(hash));
    reverse_bytes(network_hash, sizeof(network_hash));
    bytes_to_hex(network_hash, sizeof(network_hash), hex);
    printf("  info  hash:   %s\n", hex);

    reverse_bytes(target, TARGET_SIZE);
    bytes_to_hex(target, TARGET_SIZE, hex);
    printf("  info  target: %s\n", hex);
    printf("  info  the hash has more leading zeros, so it is the smaller number\n");
}

int main(void)
{
    printf("Target arithmetic test bench\n");
    printf("============================\n\n");

    test_bits_to_target();
    test_bits_rejects_malformed();
    test_meets_target();
    test_block_125552_meets_its_own_target();

    return test_report();
}