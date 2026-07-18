#include "target.h"

#include <string.h>

bool bits_to_target(uint32_t nbits, uint8_t target[TARGET_SIZE])
{
    uint32_t exponent = nbits >> 24;
    uint32_t mantissa = nbits & 0x007fffffu;

    memset(target, 0, TARGET_SIZE);

    /* The 0x00800000 bit is a sign flag inherited from the original compact
     * number format. A negative target has no meaning in consensus rules, so
     * anything setting it is malformed. */
    if (nbits & 0x00800000u) {
        return false;
    }

    /* A zero mantissa means a zero target: unreachable, but well defined. */
    if (mantissa == 0) {
        return true;
    }

    if (exponent <= 3) {
        /* Small exponents shift the mantissa DOWN instead of up. This branch
         * never occurs on mainnet but is part of the format. */
        mantissa >>= 8 * (3 - exponent);
        target[0] = (uint8_t)(mantissa);
        target[1] = (uint8_t)(mantissa >> 8);
        target[2] = (uint8_t)(mantissa >> 16);
        return true;
    }

    /*
     * The mantissa occupies three bytes starting at offset (exponent - 3).
     * Anything that would place a non-zero byte past the end of a 256-bit
     * number is an overflow and therefore invalid.
     */
    if (exponent > TARGET_SIZE) {
        return false;
    }

    {
        uint32_t offset = exponent - 3;

        /* offset + 2 must stay inside the array. */
        if (offset + 2 >= TARGET_SIZE) {
            return false;
        }

        target[offset + 0] = (uint8_t)(mantissa);
        target[offset + 1] = (uint8_t)(mantissa >> 8);
        target[offset + 2] = (uint8_t)(mantissa >> 16);
    }

    return true;
}

bool meets_target(const uint8_t hash[TARGET_SIZE], const uint8_t target[TARGET_SIZE])
{
    int i;

    /*
     * Walk from the most significant byte down. The first byte where the two
     * differ decides the comparison, so this exits early in the overwhelming
     * majority of cases: index 31 is almost always non-zero in the hash and
     * zero in the target, which is an immediate reject.
     */
    for (i = TARGET_SIZE - 1; i >= 0; i--) {
        if (hash[i] < target[i]) {
            return true;
        }
        if (hash[i] > target[i]) {
            return false;
        }
    }

    /* Every byte equal: hash == target, which consensus counts as a hit. */
    return true;
}