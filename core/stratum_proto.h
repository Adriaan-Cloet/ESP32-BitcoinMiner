/*
 * Stratum v1 protocol: parsing and serialisation.
 *
 * This layer turns raw JSON-RPC lines into structs and back. It deliberately
 * knows nothing about sockets: feed it a string, get a struct; hand it a struct,
 * get a string. That makes every message testable with a hardcoded line and a
 * breakpoint, which matters because the ESP32 has no debugger and byte-order
 * bugs are silent.
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
#include <stdint.h>

/*
 * Upper bounds for the fixed-size buffers below. Chosen generously but finite:
 * no heap allocation in the mining path, and the same structs fit on the ESP32.
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
#define STRATUM_EXTRANONCE1_HEX_MAX 32   /* up to 16 bytes */

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
 * The reply to mining.subscribe. extranonce1 is assigned by the pool and is
 * unique per connection; extranonce2_size is how many bytes the miner then
 * fills in itself. Together they carve the search space so two miners never
 * collide without any coordination.
 */
typedef struct {
    char   extranonce1[STRATUM_EXTRANONCE1_HEX_MAX + 1];
    size_t extranonce2_size;
} stratum_subscribe_t;

/*
 * Serialise the requests the miner sends. All write JSON only, without a
 * trailing newline; the transport layer appends the "\n" that frames a Stratum
 * line. Return the number of characters written, or -1 if the buffer is too
 * small or an argument is NULL.
 */
int stratum_serialize_subscribe(int id, const char *user_agent,
                                char *out, size_t out_size);
int stratum_serialize_authorize(int id, const char *btc_address,
                                const char *password, char *out, size_t out_size);

/*
 * mining.submit params are [worker, job_id, extranonce2, ntime, nonce]. worker
 * is the same username used to authorize. ntime and extranonce2 are hex strings;
 * nonce is formatted here as 8 big-endian hex digits, the value that was written
 * little-endian into the header.
 */
int stratum_serialize_submit(int id, const char *worker, const char *job_id,
                             const char *extranonce2_hex, const char *ntime_hex,
                             uint32_t nonce, char *out, size_t out_size);

/*
 * Ask the pool for an easier share difficulty. Pools may honour or ignore it;
 * the authoritative value is whatever set_difficulty then reports.
 */
int stratum_serialize_suggest_difficulty(int id, double difficulty,
                                         char *out, size_t out_size);

/*
 * Parse one line of JSON. Each returns true only if the line is the expected
 * message and every field it needs is present and fits. On false, the output
 * argument is left unspecified. The input string is never modified.
 */
bool stratum_parse_notify(const char *json_line, stratum_job_t *job);
bool stratum_parse_subscribe_result(const char *json_line, stratum_subscribe_t *sub);
bool stratum_parse_set_difficulty(const char *json_line, double *difficulty);

/*
 * Parse a JSON-RPC result reply of the form {"id":N,"result":<bool>,...}, as
 * sent to acknowledge authorize and submit. Returns true only when the line has
 * a numeric id and a boolean result, filling *id and *accepted. A subscribe
 * reply (result is an array) and a notification (id is null) both return false,
 * so this cleanly tells acknowledgements apart from everything else.
 */
bool stratum_parse_result(const char *json_line, int *id, bool *accepted);

#endif /* STRATUM_PROTO_H */