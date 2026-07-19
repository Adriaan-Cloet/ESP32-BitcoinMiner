/*
 * Tests for difficulty_to_target.
 *
 * The expectations are checked structurally (which bytes are set) rather than
 * against a long hand-written hex string, so they are easy to read and hard to
 * miscount. Difficulty 1 is the reference target 0xFFFF * 2^208.
 */
#include <stdio.h>

#include "difficulty.h"
#include "target.h"
#include "test_util.h"

/* True if every byte of target is zero except the two named indices. */
static int only_ff_at(const uint8_t *target, int a, int b)
{
    for (int i = 0; i < TARGET_SIZE; i++) {
        uint8_t want = (i == a || i == b) ? 0xff : 0x00;
        if (target[i] != want) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    printf("difficulty: difficulty_to_target\n");

    uint8_t t[TARGET_SIZE];

    /* diff 1: 0xFFFF * 2^208, so 0xff at little-endian bytes 26 and 27. */
    check_bool("diff 1 succeeds", difficulty_to_target(1.0, t), 1);
    check_bool("diff 1 target is 0xffff at bytes 26,27", only_ff_at(t, 26, 27), 1);

    /* diff 256: shifted down one byte, 0xff at bytes 25 and 26. */
    check_bool("diff 256 succeeds", difficulty_to_target(256.0, t), 1);
    check_bool("diff 256 target is 0xffff at bytes 25,26", only_ff_at(t, 25, 26), 1);

    /* A higher difficulty must be a stricter (smaller-or-equal) target. */
    uint8_t t1[TARGET_SIZE];
    uint8_t t256[TARGET_SIZE];
    difficulty_to_target(1.0, t1);
    difficulty_to_target(256.0, t256);
    check_bool("higher difficulty is a stricter target", meets_target(t256, t1), 1);

    /* Non-positive difficulty is rejected. */
    check_bool("rejects zero difficulty", difficulty_to_target(0.0, t), 0);

    return test_report();
}