#include "stratum_proto.h"

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