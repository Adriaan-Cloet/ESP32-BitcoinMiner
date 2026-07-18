/*
 * Small helpers shared by the test bench. Header-only on purpose: these are
 * test scaffolding, not part of the miner, and they must never be linked into
 * firmware.
 */

#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int test_failures = 0;

/* Parse a hex string into bytes. Returns the number of bytes written. */
static inline size_t hex_to_bytes(const char *hex, uint8_t *out, size_t out_size)
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

static inline void bytes_to_hex(const uint8_t *data, size_t len, char *out)
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
 * Bitcoin displays hashes and targets in the reverse of their internal byte
 * order. Internal order is what you hash and compare; display order is what a
 * block explorer shows. The conversion is always explicit, never implicit.
 */
static inline void reverse_bytes(uint8_t *data, size_t len)
{
    size_t i;

    for (i = 0; i < len / 2; i++) {
        uint8_t tmp       = data[i];
        data[i]           = data[len - 1 - i];
        data[len - 1 - i] = tmp;
    }
}

static inline void check(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) == 0) {
        printf("  PASS  %s\n", name);
    } else {
        printf("  FAIL  %s\n", name);
        printf("        expected: %s\n", want);
        printf("        got:      %s\n", got);
        test_failures++;
    }
}

static inline void check_bool(const char *name, int got, int want)
{
    if ((got != 0) == (want != 0)) {
        printf("  PASS  %s\n", name);
    } else {
        printf("  FAIL  %s (expected %s, got %s)\n",
               name, want ? "true" : "false", got ? "true" : "false");
        test_failures++;
    }
}

static inline int test_report(void)
{
    printf("\n");
    if (test_failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    printf("%d test(s) FAILED.\n", test_failures);
    return 1;
}

#endif /* TEST_UTIL_H */