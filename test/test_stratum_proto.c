/*
 * Tests for the Stratum mining.notify parser.
 *
 * The input is a single, real-shaped mining.notify line. Each field carries a
 * distinct, recognisable value so a mis-indexed array element shows up at once
 * instead of hiding behind plausible-looking hex.
 */
#include <stdio.h>

#include "stratum_proto.h"
#include "test_util.h"

int main(void)
{
    printf("stratum_proto: mining.notify\n");

    /* Well-formed notify: two merkle branches, clean_jobs = true. */
    const char *notify =
        "{\"id\":null,\"method\":\"mining.notify\",\"params\":["
        "\"job0001\","
        "\"00000000000000000001aaaa0000000000000000000000000000000000000000\","
        "\"01000000010000\","
        "\"0affffffff0100\","
        "[\"1111111111111111111111111111111111111111111111111111111111111111\","
        "\"2222222222222222222222222222222222222222222222222222222222222222\"],"
        "\"20000000\","
        "\"170331db\","
        "\"64ec9d1a\","
        "true]}";

    stratum_job_t job;
    bool parsed = stratum_parse_notify(notify, &job);
    check_bool("parses cleanly", parsed, 1);

    if (parsed) {
        check("job_id",   job.job_id,   "job0001");
        check("prevhash", job.prevhash,
              "00000000000000000001aaaa0000000000000000000000000000000000000000");
        check("coinb1",   job.coinb1,   "01000000010000");
        check("coinb2",   job.coinb2,   "0affffffff0100");
        check("version",  job.version,  "20000000");
        check("nbits",    job.nbits,    "170331db");
        check("ntime",    job.ntime,    "64ec9d1a");
        check_bool("clean_jobs", job.clean_jobs, 1);

        char count[16];
        snprintf(count, sizeof count, "%zu", job.merkle_count);
        check("merkle_count", count, "2");
        check("merkle[0]", job.merkle_branch[0],
              "1111111111111111111111111111111111111111111111111111111111111111");
        check("merkle[1]", job.merkle_branch[1],
              "2222222222222222222222222222222222222222222222222222222222222222");
    }

    /* A non-notify line must be rejected, not silently accepted. */
    check_bool("rejects non-notify",
               stratum_parse_notify("{\"id\":1,\"result\":true,\"error\":null}", &job), 0);

    /* Malformed JSON must be rejected too. */
    check_bool("rejects garbage", stratum_parse_notify("{not json", &job), 0);

    return test_report();
}