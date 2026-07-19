/*
 * Desktop miner, phase 1 finale: the full loop against a live pool.
 *
 * connect -> subscribe -> authorize -> suggest an easier difficulty -> then, per
 * job: build and verify the coinbase, fold the merkle root, assemble the header,
 * and grind the nonce against the share target. A hit is submitted with
 * mining.submit; the pool answers accepted or rejected.
 *
 * The socket goes non-blocking after the handshake so new jobs and difficulty
 * changes are picked up between batches of nonces without stalling the grind.
 * This is deliberately single-threaded and unoptimised: correctness first, the
 * same core then moves to the ESP32 in phase 2.
 *
 * nanosleep needs this feature-test macro under -std=c11.
 */
#define _POSIX_C_SOURCE 200112L

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "coinbase.h"
#include "difficulty.h"
#include "header.h"
#include "merkle.h"
#include "net_posix.h"
#include "sha256_ref.h"
#include "stratum_proto.h"
#include "target.h"

/* Nonces hashed between checks for new pool messages. */
#define NONCE_BATCH 200000u

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Decode a 64-char hex string into 32 bytes. */
static bool hex_to_bytes32(const char *hex, uint8_t out[32])
{
    for (int i = 0; i < 32; i++) {
        int hi = hex_nibble(hex[2 * i]);
        int lo = hex_nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return hex[64] == '\0';
}

/* Format a counter as big-endian hex of size_bytes bytes (<= 8). */
static void extranonce2_hex(uint64_t counter, size_t size_bytes, char *out)
{
    for (size_t i = 0; i < size_bytes; i++) {
        unsigned b = (unsigned)((counter >> (8 * (size_bytes - 1 - i))) & 0xffU);
        snprintf(out + 2 * i, 3, "%02x", b);
    }
    out[2 * size_bytes] = '\0';
}

int main(void)
{
    net_conn_t conn;
    if (!net_connect(&conn, POOL_HOST, POOL_PORT)) {
        fprintf(stderr, "could not connect to %s:%s\n", POOL_HOST, POOL_PORT);
        return 1;
    }
    printf("connected to %s:%s\n", POOL_HOST, POOL_PORT);

    char line[NET_RECV_BUF_SIZE];

    /* subscribe */
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
    size_t en2_size = sub.extranonce2_size > 8 ? 8 : sub.extranonce2_size;
    printf("subscribed: extranonce1=%s extranonce2_size=%zu\n",
           sub.extranonce1, sub.extranonce2_size);

    /* authorize */
    if (stratum_serialize_authorize(2, MINING_ADDRESS, "x", line, sizeof line) < 0 ||
        !net_send_line(&conn, line)) {
        fprintf(stderr, "failed to send mining.authorize\n");
        net_close(&conn);
        return 1;
    }
    printf("authorize sent for %s\n", MINING_ADDRESS);

    /* Politely ask for an easier difficulty; the pool may honour or ignore it. */
    if (stratum_serialize_suggest_difficulty(3, 1.0, line, sizeof line) >= 0) {
        net_send_line(&conn, line);
    }

    if (!net_set_nonblocking(&conn)) {
        fprintf(stderr, "could not set the socket non-blocking\n");
        net_close(&conn);
        return 1;
    }
    printf("mining (Ctrl-C to stop)...\n\n");

    /* mining state */
    stratum_job_t job;
    uint8_t       share_target[TARGET_SIZE];
    uint8_t       header[BLOCK_HEADER_SIZE];
    uint8_t       branch[STRATUM_MAX_MERKLE_BRANCHES][32];
    size_t        branch_count = 0;
    char          en2[2 * 8 + 1];
    uint64_t      en2_counter = 0;
    uint32_t      nonce = 0;
    bool          have_job = false;
    bool          have_target = false;
    bool          template_ready = false;
    int           submit_id = 100;

    uint64_t hashes = 0;
    time_t   last_report = time(NULL);

    for (;;) {
        /* 1. Drain everything the pool has sent since last time. */
        for (;;) {
            int r = net_poll_line(&conn, line, sizeof line);
            if (r < 0) {
                printf("connection closed\n");
                net_close(&conn);
                return 0;
            }
            if (r == 0) {
                break;
            }

            double diff;
            int    rid;
            bool   accepted;
            if (stratum_parse_set_difficulty(line, &diff)) {
                if (difficulty_to_target(diff, share_target)) {
                    have_target = true;
                    printf("difficulty -> %g\n", diff);
                }
            } else if (stratum_parse_notify(line, &job)) {
                have_job = true;
                template_ready = false;
                en2_counter = 0;
                nonce = 0;
                branch_count = job.merkle_count;
                bool branch_ok = true;
                for (size_t i = 0; i < branch_count; i++) {
                    if (!hex_to_bytes32(job.merkle_branch[i], branch[i])) {
                        branch_ok = false;
                        break;
                    }
                }
                if (!branch_ok) {
                    printf("bad merkle branch in job %s, skipping\n", job.job_id);
                    have_job = false;
                } else {
                    printf("new job %s (clean=%s)\n",
                           job.job_id, job.clean_jobs ? "yes" : "no");
                }
            } else if (stratum_parse_result(line, &rid, &accepted)) {
                printf("%s (id %d)\n",
                       accepted ? "share ACCEPTED" : "share rejected", rid);
            } else {
                printf("<< %s\n", line);
            }
        }

        if (!have_job || !have_target) {
            struct timespec idle = {0, 50L * 1000L * 1000L}; /* 50 ms */
            nanosleep(&idle, NULL);
            continue;
        }

        /* 2. Rebuild the coinbase, merkle root and header template whenever the
         *    job or the extranonce2 changed. Only the nonce moves after this. */
        if (!template_ready) {
            extranonce2_hex(en2_counter, en2_size, en2);

            uint8_t coinbase[COINBASE_MAX_BYTES];
            int clen = coinbase_assemble(job.coinb1, sub.extranonce1, en2,
                                         job.coinb2, coinbase, sizeof coinbase);
            if (clen < 0) {
                printf("coinbase malformed or too large for job %s, skipping\n",
                       job.job_id);
                have_job = false;
                continue;
            }

            if (en2_counter == 0) {
                bool pays = coinbase_pays_script(coinbase, (size_t)clen,
                                                 MINING_SCRIPTPUBKEY);
                printf("  coinbase pays our address: %s\n",
                       pays ? "yes" : "NO -- check MINING_SCRIPTPUBKEY in config.h");
            }

            uint8_t leaf[32];
            sha256d_ref(coinbase, (size_t)clen, leaf);
            uint8_t root[32];
            build_merkle_root(leaf, branch, branch_count, root);

            if (!assemble_header(job.version, job.prevhash, root,
                                 job.ntime, job.nbits, 0, header)) {
                printf("could not assemble header for job %s, skipping\n",
                       job.job_id);
                have_job = false;
                continue;
            }
            nonce = 0;
            template_ready = true;
        }

        /* 3. Grind a batch of nonces, only the last four header bytes change. */
        for (unsigned b = 0; b < NONCE_BATCH; b++) {
            header[76] = (uint8_t)(nonce & 0xff);
            header[77] = (uint8_t)((nonce >> 8) & 0xff);
            header[78] = (uint8_t)((nonce >> 16) & 0xff);
            header[79] = (uint8_t)((nonce >> 24) & 0xff);

            uint8_t hash[32];
            sha256d_ref(header, BLOCK_HEADER_SIZE, hash);
            hashes++;

            if (meets_target(hash, share_target)) {
                char msg[512];
                if (stratum_serialize_submit(submit_id++, MINING_ADDRESS,
                                             job.job_id, en2, job.ntime, nonce,
                                             msg, sizeof msg) >= 0) {
                    net_send_line(&conn, msg);
                    printf("  >> share found: job %s nonce %08x extranonce2 %s\n",
                           job.job_id, (unsigned)nonce, en2);
                }
            }

            if (nonce == 0xffffffffu) {
                /* Nonce space exhausted: advance extranonce2 and rebuild. */
                en2_counter++;
                template_ready = false;
                break;
            }
            nonce++;
        }

        /* 4. Report the hashrate every few seconds. */
        time_t now = time(NULL);
        if (now - last_report >= 5) {
            double secs = (double)(now - last_report);
            printf("  %.1f kH/s (%" PRIu64 " hashes in %.0fs)\n",
                   (double)hashes / secs / 1000.0, hashes, secs);
            hashes = 0;
            last_report = now;
        }
    }
}