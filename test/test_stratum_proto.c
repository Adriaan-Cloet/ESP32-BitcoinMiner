/*
 * Tests for the Stratum protocol layer: serialising the requests the miner
 * sends and parsing the messages the pool sends back. No sockets involved, so
 * every case is a fixed string with a known-good expectation.
 *
 * The notify input uses a distinct, recognisable value per field so a
 * mis-indexed array element shows up at once instead of hiding behind
 * plausible-looking hex.
 */
#include <stdio.h>

#include "stratum_proto.h"
#include "test_util.h"

static void test_serialise(void)
{
    printf("stratum_proto: serialise\n");

    char buf[256];

    int n = stratum_serialize_subscribe(1, "cloetminer/0.1", buf, sizeof buf);
    check_bool("subscribe returns length", n > 0, 1);
    check("subscribe json", buf,
          "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"cloetminer/0.1\"]}");

    /* A dummy worker name on purpose: the real payout address never lives in
     * the repo, it comes from config.h at runtime. */
    n = stratum_serialize_authorize(2, "bc1qtestworker", "x", buf, sizeof buf);
    check_bool("authorize returns length", n > 0, 1);
    check("authorize json", buf,
          "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"bc1qtestworker\",\"x\"]}");

    char tiny[8];
    check_bool("subscribe rejects a small buffer",
               stratum_serialize_subscribe(1, "cloetminer/0.1", tiny, sizeof tiny) < 0, 1);
}

static void test_subscribe_result(void)
{
    printf("stratum_proto: subscribe result\n");

    const char *ok_reply =
        "{\"id\":1,\"result\":["
        "[[\"mining.set_difficulty\",\"1\"],[\"mining.notify\",\"ae6812eb\"]],"
        "\"081000\",4],\"error\":null}";

    stratum_subscribe_t sub;
    check_bool("parses subscribe result",
               stratum_parse_subscribe_result(ok_reply, &sub), 1);
    check("extranonce1", sub.extranonce1, "081000");

    char e2[16];
    snprintf(e2, sizeof e2, "%zu", sub.extranonce2_size);
    check("extranonce2_size", e2, "4");

    const char *err_reply =
        "{\"id\":1,\"result\":null,\"error\":[20,\"Other/unknown\",null]}";
    check_bool("rejects an error reply",
               stratum_parse_subscribe_result(err_reply, &sub), 0);
}

static void test_set_difficulty(void)
{
    printf("stratum_proto: set_difficulty\n");

    double diff = 0.0;
    check_bool("parses integer difficulty",
               stratum_parse_set_difficulty(
                   "{\"id\":null,\"method\":\"mining.set_difficulty\",\"params\":[500]}",
                   &diff), 1);
    char db[32];
    snprintf(db, sizeof db, "%g", diff);
    check("difficulty 500", db, "500");

    check_bool("parses fractional difficulty",
               stratum_parse_set_difficulty(
                   "{\"id\":null,\"method\":\"mining.set_difficulty\",\"params\":[0.001]}",
                   &diff), 1);
    snprintf(db, sizeof db, "%g", diff);
    check("difficulty 0.001", db, "0.001");

    check_bool("rejects a non-difficulty line",
               stratum_parse_set_difficulty(
                   "{\"id\":1,\"result\":true,\"error\":null}", &diff), 0);
}

static void test_notify(void)
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
}

int main(void)
{
    test_serialise();
    test_subscribe_result();
    test_set_difficulty();
    test_notify();
    return test_report();
}