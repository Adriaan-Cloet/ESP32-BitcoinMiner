#include "stratum_proto.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"

/*
 * Copy a cJSON string into a fixed buffer, rejecting anything that is not a
 * string or does not fit. Rejecting on overflow rather than truncating is
 * deliberate: a silently truncated prevhash would hash to nonsense and waste
 * hours chasing a ghost. Better to drop the whole job and wait for the next.
 */
static bool copy_str_field(const cJSON *item, char *dst, size_t dst_size)
{
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    size_t len = strlen(item->valuestring);
    if (len + 1 > dst_size) {
        return false;
    }
    memcpy(dst, item->valuestring, len + 1);
    return true;
}

/*
 * Render a cJSON object to out and dispose of it. Owns root: it is always freed,
 * whether the copy succeeds or not, so callers never leak on the error path.
 */
static int print_and_copy(cJSON *root, char *out, size_t out_size)
{
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return -1;
    }
    size_t len = strlen(json);
    if (len + 1 > out_size) {
        cJSON_free(json);
        return -1;
    }
    memcpy(out, json, len + 1);
    cJSON_free(json);
    return (int)len;
}

int stratum_serialize_subscribe(int id, const char *user_agent,
                                char *out, size_t out_size)
{
    if (user_agent == NULL || out == NULL) {
        return -1;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return -1;
    }
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddStringToObject(root, "method", "mining.subscribe");
    cJSON *params = cJSON_AddArrayToObject(root, "params");
    if (params == NULL) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_AddItemToArray(params, cJSON_CreateString(user_agent));
    return print_and_copy(root, out, out_size);
}

int stratum_serialize_authorize(int id, const char *btc_address,
                                const char *password, char *out, size_t out_size)
{
    if (btc_address == NULL || password == NULL || out == NULL) {
        return -1;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return -1;
    }
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddStringToObject(root, "method", "mining.authorize");
    cJSON *params = cJSON_AddArrayToObject(root, "params");
    if (params == NULL) {
        cJSON_Delete(root);
        return -1;
    }
    /* Username is the BTC payout address; the password is unused by solo pools
     * but the field is mandatory, so "x" is the convention. */
    cJSON_AddItemToArray(params, cJSON_CreateString(btc_address));
    cJSON_AddItemToArray(params, cJSON_CreateString(password));
    return print_and_copy(root, out, out_size);
}

int stratum_serialize_submit(int id, const char *worker, const char *job_id,
                             const char *extranonce2_hex, const char *ntime_hex,
                             uint32_t nonce, char *out, size_t out_size)
{
    if (worker == NULL || job_id == NULL || extranonce2_hex == NULL ||
        ntime_hex == NULL || out == NULL) {
        return -1;
    }

    /* The nonce goes on the wire as 8 big-endian hex digits of its value. */
    char nonce_hex[9];
    snprintf(nonce_hex, sizeof nonce_hex, "%08x", (unsigned)nonce);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return -1;
    }
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddStringToObject(root, "method", "mining.submit");
    cJSON *params = cJSON_AddArrayToObject(root, "params");
    if (params == NULL) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_AddItemToArray(params, cJSON_CreateString(worker));
    cJSON_AddItemToArray(params, cJSON_CreateString(job_id));
    cJSON_AddItemToArray(params, cJSON_CreateString(extranonce2_hex));
    cJSON_AddItemToArray(params, cJSON_CreateString(ntime_hex));
    cJSON_AddItemToArray(params, cJSON_CreateString(nonce_hex));
    return print_and_copy(root, out, out_size);
}

int stratum_serialize_suggest_difficulty(int id, double difficulty,
                                         char *out, size_t out_size)
{
    if (out == NULL) {
        return -1;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return -1;
    }
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddStringToObject(root, "method", "mining.suggest_difficulty");
    cJSON *params = cJSON_AddArrayToObject(root, "params");
    if (params == NULL) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_AddItemToArray(params, cJSON_CreateNumber(difficulty));
    return print_and_copy(root, out, out_size);
}

bool stratum_parse_notify(const char *json_line, stratum_job_t *job)
{
    if (json_line == NULL || job == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        return false;
    }

    bool ok = false;

    const cJSON *method = cJSON_GetObjectItemCaseSensitive(root, "method");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");

    if (!cJSON_IsString(method) ||
        strcmp(method->valuestring, "mining.notify") != 0) {
        goto done;
    }

    /*
     * mining.notify params are a fixed nine-element array, positional:
     *   [0] job_id    [1] prevhash   [2] coinb1
     *   [3] coinb2    [4] merkle[]   [5] version
     *   [6] nbits     [7] ntime      [8] clean_jobs
     */
    if (!cJSON_IsArray(params) || cJSON_GetArraySize(params) < 9) {
        goto done;
    }

    if (!copy_str_field(cJSON_GetArrayItem(params, 0), job->job_id,   sizeof job->job_id) ||
        !copy_str_field(cJSON_GetArrayItem(params, 1), job->prevhash, sizeof job->prevhash) ||
        !copy_str_field(cJSON_GetArrayItem(params, 2), job->coinb1,   sizeof job->coinb1) ||
        !copy_str_field(cJSON_GetArrayItem(params, 3), job->coinb2,   sizeof job->coinb2) ||
        !copy_str_field(cJSON_GetArrayItem(params, 5), job->version,  sizeof job->version) ||
        !copy_str_field(cJSON_GetArrayItem(params, 6), job->nbits,    sizeof job->nbits) ||
        !copy_str_field(cJSON_GetArrayItem(params, 7), job->ntime,    sizeof job->ntime)) {
        goto done;
    }

    const cJSON *branch = cJSON_GetArrayItem(params, 4);
    if (!cJSON_IsArray(branch) ||
        cJSON_GetArraySize(branch) > STRATUM_MAX_MERKLE_BRANCHES) {
        goto done;
    }
    job->merkle_count = 0;
    for (int i = 0; i < cJSON_GetArraySize(branch); i++) {
        if (!copy_str_field(cJSON_GetArrayItem(branch, i),
                            job->merkle_branch[i],
                            sizeof job->merkle_branch[i])) {
            goto done;
        }
        job->merkle_count++;
    }

    const cJSON *clean = cJSON_GetArrayItem(params, 8);
    if (!cJSON_IsBool(clean)) {
        goto done;
    }
    job->clean_jobs = cJSON_IsTrue(clean);

    ok = true;

done:
    cJSON_Delete(root);
    return ok;
}

bool stratum_parse_subscribe_result(const char *json_line, stratum_subscribe_t *sub)
{
    if (json_line == NULL || sub == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        return false;
    }

    bool ok = false;

    const cJSON *error  = cJSON_GetObjectItemCaseSensitive(root, "error");
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");

    /* A populated error field means the subscribe failed. */
    if (error != NULL && !cJSON_IsNull(error)) {
        goto done;
    }

    /*
     * result is [subscription_details, extranonce1, extranonce2_size].
     * The details at [0] are the subscription ids, which the miner does not use.
     */
    if (!cJSON_IsArray(result) || cJSON_GetArraySize(result) < 3) {
        goto done;
    }

    if (!copy_str_field(cJSON_GetArrayItem(result, 1),
                        sub->extranonce1, sizeof sub->extranonce1)) {
        goto done;
    }

    const cJSON *e2 = cJSON_GetArrayItem(result, 2);
    if (!cJSON_IsNumber(e2) || e2->valueint < 0) {
        goto done;
    }
    sub->extranonce2_size = (size_t)e2->valueint;

    ok = true;

done:
    cJSON_Delete(root);
    return ok;
}

bool stratum_parse_set_difficulty(const char *json_line, double *difficulty)
{
    if (json_line == NULL || difficulty == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        return false;
    }

    bool ok = false;

    const cJSON *method = cJSON_GetObjectItemCaseSensitive(root, "method");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");

    if (!cJSON_IsString(method) ||
        strcmp(method->valuestring, "mining.set_difficulty") != 0) {
        goto done;
    }
    if (!cJSON_IsArray(params) || cJSON_GetArraySize(params) < 1) {
        goto done;
    }

    const cJSON *d = cJSON_GetArrayItem(params, 0);
    if (!cJSON_IsNumber(d)) {
        goto done;
    }
    /* Difficulty can be fractional (pools set it well below 1 for tiny miners),
     * so it is a double, not an integer. */
    *difficulty = d->valuedouble;

    ok = true;

done:
    cJSON_Delete(root);
    return ok;
}

bool stratum_parse_result(const char *json_line, int *id, bool *accepted)
{
    if (json_line == NULL || id == NULL || accepted == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        return false;
    }

    bool ok = false;

    const cJSON *jid    = cJSON_GetObjectItemCaseSensitive(root, "id");
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");

    /* Notifications carry a null id; subscribe carries an array result. Only a
     * numeric id together with a boolean result is an acknowledgement. */
    if (!cJSON_IsNumber(jid) || !cJSON_IsBool(result)) {
        goto done;
    }
    *id       = jid->valueint;
    *accepted = cJSON_IsTrue(result);
    ok = true;

done:
    cJSON_Delete(root);
    return ok;
}