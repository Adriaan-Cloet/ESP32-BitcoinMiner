/*
 * Desktop driver for phase 1: run the Stratum handshake against a real pool and
 * print the jobs as they arrive. No mining happens here yet; this proves the
 * transport and the protocol layer work end to end against a live server.
 *
 * Flow: connect -> subscribe -> authorize -> then read lines forever, printing
 * set_difficulty updates and mining.notify jobs as they come in.
 */
#include <stdio.h>

#include "config.h"
#include "net_posix.h"
#include "stratum_proto.h"

int main(void)
{
    net_conn_t conn;
    if (!net_connect(&conn, POOL_HOST, POOL_PORT)) {
        fprintf(stderr, "could not connect to %s:%s\n", POOL_HOST, POOL_PORT);
        return 1;
    }
    printf("connected to %s:%s\n", POOL_HOST, POOL_PORT);

    /* Big enough for any single Stratum line, including a fat mining.notify. */
    char line[NET_RECV_BUF_SIZE];

    /* 1. subscribe, then read the reply with our extranonce1. */
    if (stratum_serialize_subscribe(1, USER_AGENT, line, sizeof line) < 0 ||
        !net_send_line(&conn, line)) {
        fprintf(stderr, "failed to send mining.subscribe\n");
        net_close(&conn);
        return 1;
    }
    if (net_recv_line(&conn, line, sizeof line) != 1) {
        fprintf(stderr, "no reply to mining.subscribe\n");
        net_close(&conn);
        return 1;
    }

    stratum_subscribe_t sub;
    if (!stratum_parse_subscribe_result(line, &sub)) {
        fprintf(stderr, "unexpected subscribe reply: %s\n", line);
        net_close(&conn);
        return 1;
    }
    printf("subscribed: extranonce1=%s extranonce2_size=%zu\n",
           sub.extranonce1, sub.extranonce2_size);

    /* 2. authorize with the payout address as username. */
    if (stratum_serialize_authorize(2, MINING_ADDRESS, "x", line, sizeof line) < 0 ||
        !net_send_line(&conn, line)) {
        fprintf(stderr, "failed to send mining.authorize\n");
        net_close(&conn);
        return 1;
    }
    printf("authorize sent for %s\n\n", MINING_ADDRESS);

    /* 3. read whatever the pool sends: the authorize ack, difficulty, jobs. */
    printf("waiting for work (Ctrl-C to stop)...\n\n");
    for (;;) {
        int r = net_recv_line(&conn, line, sizeof line);
        if (r == 0) {
            printf("pool closed the connection\n");
            break;
        }
        if (r < 0) {
            fprintf(stderr, "read error\n");
            break;
        }

        double difficulty;
        stratum_job_t job;
        if (stratum_parse_set_difficulty(line, &difficulty)) {
            printf("difficulty -> %g\n", difficulty);
        } else if (stratum_parse_notify(line, &job)) {
            printf("job %s  clean=%s  merkle_branches=%zu  ntime=%s\n",
                   job.job_id,
                   job.clean_jobs ? "yes" : "no",
                   job.merkle_count,
                   job.ntime);
        } else {
            /* The authorize ack and anything else we do not model yet. */
            printf("<< %s\n", line);
        }
    }

    net_close(&conn);
    return 0;
}