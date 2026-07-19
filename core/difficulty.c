#include "difficulty.h"

#include <math.h>
#include <string.h>

bool difficulty_to_target(double difficulty, uint8_t target[TARGET_SIZE])
{
    /* Rejects <= 0 and, because the comparison is false for NaN, NaN too. */
    if (!(difficulty > 0.0)) {
        return false;
    }

    /*
     * target = diff1 / difficulty, with diff1 = 0xFFFF * 2^208 (the classic
     * "difficulty 1" target). long double keeps ~64 significant bits, far more
     * than a share check needs, and this runs once per set_difficulty, never in
     * the hash loop, so the floating point is not a hot-path concern.
     */
    long double value =
        (long double)0xFFFF * powl(2.0L, 208.0L) / (long double)difficulty;

    /*
     * Lay the value down little-endian: target[i] is the coefficient of 256^i.
     * Extract from the most significant byte so each step removes what it took.
     */
    memset(target, 0, TARGET_SIZE);
    for (int i = TARGET_SIZE - 1; i >= 0; i--) {
        long double place = powl(2.0L, 8.0L * (long double)i);
        long double digit = floorl(value / place);
        if (digit > 255.0L) {
            digit = 255.0L; /* difficulty below 1 can overflow the top byte */
        }
        target[i] = (uint8_t)digit;
        value -= digit * place;
    }
    return true;
}