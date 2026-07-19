/*
 * Stratum v1 protocol: parsing and serialisation.
 *
 * This layer turns raw JSON-RPC lines into structs and back. It deliberately
 * knows nothing about sockets: feed it a string, get a struct. That makes every
 * message testable with a hardcoded line and a breakpoint, which matters because
 * the ESP32 has no debugger and byte-order bugs are silent.
 *
 * Parsing here is faithful, not interpretive. Hex fields are kept as the exact
 * strings the pool sent. Turning them into bytes (and dealing with Bitcoin's
 * mixed endianness) happens later, when the header is assembled. Keeping those
 * two steps apart is the whole point of this file.
 */
#ifndef STRATUM_PROTO_H
#define STRATUM_PROTO_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Upper bounds for the fixed-size buffers below. Chosen generously but finite:
 * no heap allocation in the mining path, and the same struct fits on the ESP32.
 * A coinbase half of 512 hex chars is 256 bytes, comfortably above what pools
 * send in practice.
 */
#define STRATUM_MAX_MERKLE_BRANCHES 20
#define STRATUM_JOB_ID_MAX          64
#define STRATUM_PREVHASH_HEX        64   /* 32 bytes */
#define STRATUM_COINBASE_HEX_MAX    512
#define STRATUM_MERKLE_HEX          64   /* 32 bytes */
#define STRATUM_VERSION_HEX         8    /* 4 bytes */
#define STRATUM_NBITS_HEX           8    /* 4 bytes */
#define STRATUM_NTIME_HEX           8    /* 4 bytes */

/*
 * A mining job as delivered by mining.notify. All hex fields are stored exactly
 * as received, null-terminated. clean_jobs == true means: drop the current job
 * immediately, any share still submitted for it would be stale.
 */
typedef struct {
    char   job_id[STRATUM_JOB_ID_MAX + 1];
    char   prevhash[STRATUM_PREVHASH_HEX + 1];
    char   coinb1[STRATUM_COINBASE_HEX_MAX + 1];
    char   coinb2[STRATUM_COINBASE_HEX_MAX + 1];
    char   merkle_branch[STRATUM_MAX_MERKLE_BRANCHES][STRATUM_MERKLE_HEX + 1];
    size_t merkle_count;
    char   version[STRATUM_VERSION_HEX + 1];
    char   nbits[STRATUM_NBITS_HEX + 1];
    char   ntime[STRATUM_NTIME_HEX + 1];
    bool   clean_jobs;
} stratum_job_t;

/*
 * Parse one line of JSON into a job. Returns true only if the line is a
 * well-formed mining.notify whose fields all fit the buffers above. On false,
 * the contents of *job are unspecified. The input string is not modified.
 */
bool stratum_parse_notify(const char *json_line, stratum_job_t *job);

#endif /* STRATUM_PROTO_H */