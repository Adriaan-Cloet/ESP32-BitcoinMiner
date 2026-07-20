/*
 * ESP32 miner, phase 2 step 2: the full miner on hardware.
 *
 * Two FreeRTOS tasks share the work:
 *   - the network task (app_main itself, on core 0 alongside WiFi/lwIP) reads
 *     jobs and difficulty from the pool into shared state, and sends the shares
 *     the hash task finds;
 *   - the hash task (pinned to core 1) rebuilds the header whenever the job
 *     changes and grinds the nonce against the share target.
 *
 * Only the network task touches the socket. Found shares travel to it over a
 * queue, so the two tasks never write the socket at the same time. Shared job
 * and target state is guarded by a mutex, with a generation counter the hash
 * task watches to notice new work.
 *
 * Two hazards from the design notes are handled here: the task watchdog (the
 * hash task calls vTaskDelay once per batch so core 1's idle task keeps running)
 * and stale shares (a new job bumps the generation, and the hash task switches
 * to it at the next batch boundary).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "config.h"
#include "coinbase.h"
#include "difficulty.h"
#include "header.h"
#include "merkle.h"
#include "net_posix.h"
#include "sha256_fast.h"
#include "sha256_ref.h"
#include "stratum_proto.h"
#include "target.h"
#include "wifi.h"

#define HASH_BATCH  2000       /* nonces per batch before yielding to idle */
#define REPORT_US   5000000    /* hash-rate log interval, microseconds */

static const char *TAG = "miner";

/* ---- shared state (guarded by s_lock) ---------------------------------- */
static SemaphoreHandle_t   s_lock;
static stratum_job_t       s_job;
static uint8_t             s_target[TARGET_SIZE];
static uint32_t            s_generation;   /* bumped on every new job or difficulty */
static bool                s_have_job;
static bool                s_have_target;

/* Set once during the handshake, read-only afterwards. */
static stratum_subscribe_t s_sub;

/* ---- found shares travel from the hash task to the network task -------- */
typedef struct {
    char     job_id[STRATUM_JOB_ID_MAX + 1];
    char     extranonce2[2 * 8 + 1];
    char     ntime[STRATUM_NTIME_HEX + 1];
    uint32_t nonce;
} found_share_t;

static QueueHandle_t s_shares;

/* ---- network task buffers ---------------------------------------------- */
static net_conn_t    conn;
static char          line[NET_RECV_BUF_SIZE];
static stratum_job_t scratch_job;

/* ---- small hex helpers ------------------------------------------------- */
static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool hex_to_bytes32(const char *hex, uint8_t out[32])
{
    for (int i = 0; i < 32; i++) {
        int hi = hex_nibble(hex[2 * i]);
        int lo = hex_nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return hex[64] == '\0';
}

static void extranonce2_hex(uint64_t counter, size_t size_bytes, char *out)
{
    for (size_t i = 0; i < size_bytes; i++) {
        unsigned b = (unsigned)((counter >> (8 * (size_bytes - 1 - i))) & 0xffU);
        snprintf(out + 2 * i, 3, "%02x", b);
    }
    out[2 * size_bytes] = '\0';
}

/* ---- the hash task, pinned to core 1 ----------------------------------- */
static void hash_task(void *arg)
{
    (void)arg;

    /* Big working buffers are static: one instance of this task, and this keeps
     * them off the task stack. */
    static stratum_job_t job;
    static uint8_t       branch[STRATUM_MAX_MERKLE_BRANCHES][32];
    static uint8_t       coinbase[COINBASE_MAX_BYTES];
    static uint8_t       header[BLOCK_HEADER_SIZE];
    static uint8_t       target[TARGET_SIZE];

    uint32_t local_gen = 0;
    bool     have_gen = false;
    bool     have_template = false;
    size_t   branch_count = 0;
    size_t   en2_size = 0;
    uint64_t en2_counter = 0;
    uint32_t nonce = 0;
    char     en2[2 * 8 + 1];
    uint32_t midstate[8];   /* SHA-256 state after header bytes 0..63 */
    uint8_t  tail[16];      /* header bytes 64..79, the nonce is 12..15 */

    uint64_t hashes = 0;
    int64_t  last_report = esp_timer_get_time();

    for (;;) {
        /* 1. Pick up new work if the generation changed. */
        bool ready;
        bool changed = false;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        ready = s_have_job && s_have_target;
        if (ready && (!have_gen || s_generation != local_gen)) {
            job = s_job;
            memcpy(target, s_target, TARGET_SIZE);
            local_gen = s_generation;
            have_gen = true;
            changed = true;
        }
        xSemaphoreGive(s_lock);

        if (!ready) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (changed) {
            en2_size = s_sub.extranonce2_size > 8 ? 8 : s_sub.extranonce2_size;
            branch_count = job.merkle_count;
            bool ok = true;
            for (size_t i = 0; i < branch_count; i++) {
                if (!hex_to_bytes32(job.merkle_branch[i], branch[i])) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                ESP_LOGW(TAG, "bad merkle branch in job %s", job.job_id);
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            en2_counter = 0;
            have_template = false;
        }

        /* 2. Rebuild the coinbase, merkle root and header template as needed. */
        if (!have_template) {
            extranonce2_hex(en2_counter, en2_size, en2);
            int clen = coinbase_assemble(job.coinb1, s_sub.extranonce1, en2,
                                         job.coinb2, coinbase, sizeof coinbase);
            if (clen < 0) {
                ESP_LOGW(TAG, "coinbase malformed for job %s", job.job_id);
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            if (en2_counter == 0) {
                bool pays = coinbase_pays_script(coinbase, (size_t)clen,
                                                 MINING_SCRIPTPUBKEY);
                ESP_LOGI(TAG, "job %s: coinbase pays our address: %s",
                         job.job_id, pays ? "yes" : "NO -- check config.h");
            }
            uint8_t leaf[32];
            sha256d_ref(coinbase, (size_t)clen, leaf);
            uint8_t root[32];
            build_merkle_root(leaf, branch, branch_count, root);
            if (!assemble_header(job.version, job.prevhash, root,
                                 job.ntime, job.nbits, 0, header)) {
                ESP_LOGW(TAG, "header assembly failed for job %s", job.job_id);
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            /* Bytes 0..63 are fixed for this template: fold them into the
             * midstate once, the nonce lives in the 16-byte tail. */
            sha256_midstate(header, midstate);
            memcpy(tail, header + 64, 16);
            nonce = 0;
            have_template = true;
        }

        /* 3. Grind a batch of nonces; only the nonce (tail bytes 12..15) moves. */
        for (int b = 0; b < HASH_BATCH; b++) {
            tail[12] = (uint8_t)(nonce & 0xff);
            tail[13] = (uint8_t)((nonce >> 8) & 0xff);
            tail[14] = (uint8_t)((nonce >> 16) & 0xff);
            tail[15] = (uint8_t)((nonce >> 24) & 0xff);

            uint8_t hash[32];
            sha256d_finish(midstate, tail, hash);
            hashes++;

            if (meets_target(hash, target)) {
                found_share_t fs;
                snprintf(fs.job_id, sizeof fs.job_id, "%s", job.job_id);
                snprintf(fs.extranonce2, sizeof fs.extranonce2, "%s", en2);
                snprintf(fs.ntime, sizeof fs.ntime, "%s", job.ntime);
                fs.nonce = nonce;
                xQueueSend(s_shares, &fs, 0);
                ESP_LOGI(TAG, "share found: job %s nonce %08x",
                         job.job_id, (unsigned)nonce);
            }

            if (nonce == 0xffffffffu) {
                en2_counter++;
                have_template = false;
                break;
            }
            nonce++;
        }

        /* 4. Let core 1's idle task run so the task watchdog stays happy. */
        vTaskDelay(1);

        /* 5. Report the hash rate. */
        int64_t now = esp_timer_get_time();
        int64_t dt = now - last_report;
        if (dt >= REPORT_US) {
            unsigned long hps =
                (unsigned long)((hashes * 1000000ULL) / (uint64_t)dt);
            ESP_LOGI(TAG, "%lu H/s", hps);
            hashes = 0;
            last_report = now;
        }
    }
}

/* ---- handshake, run once on app_main ----------------------------------- */
static bool handshake(void)
{
    if (!net_connect(&conn, POOL_HOST, POOL_PORT)) {
        ESP_LOGE(TAG, "could not connect to %s:%s", POOL_HOST, POOL_PORT);
        return false;
    }
    ESP_LOGI(TAG, "connected to %s:%s", POOL_HOST, POOL_PORT);

    if (stratum_serialize_subscribe(1, USER_AGENT, line, sizeof line) < 0 ||
        !net_send_line(&conn, line)) {
        ESP_LOGE(TAG, "failed to send mining.subscribe");
        return false;
    }
    if (net_recv_line(&conn, line, sizeof line) != 1 ||
        !stratum_parse_subscribe_result(line, &s_sub)) {
        ESP_LOGE(TAG, "unexpected subscribe reply");
        return false;
    }
    ESP_LOGI(TAG, "subscribed: extranonce1=%s extranonce2_size=%u",
             s_sub.extranonce1, (unsigned)s_sub.extranonce2_size);

    if (stratum_serialize_authorize(2, MINING_ADDRESS, "x", line, sizeof line) < 0 ||
        !net_send_line(&conn, line)) {
        ESP_LOGE(TAG, "failed to send mining.authorize");
        return false;
    }
    ESP_LOGI(TAG, "authorize sent for %s", MINING_ADDRESS);

    if (stratum_serialize_suggest_difficulty(3, 1.0, line, sizeof line) >= 0) {
        net_send_line(&conn, line);
    }
    return true;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "connecting to WiFi \"%s\"...", WIFI_SSID);
    if (!wifi_connect(WIFI_SSID, WIFI_PASSWORD)) {
        ESP_LOGE(TAG, "WiFi connection failed");
        return;
    }
    ESP_LOGI(TAG, "WiFi connected");

    if (!handshake()) {
        net_close(&conn);
        return;
    }

    s_lock   = xSemaphoreCreateMutex();
    s_shares = xQueueCreate(8, sizeof(found_share_t));
    if (s_lock == NULL || s_shares == NULL) {
        ESP_LOGE(TAG, "could not create sync primitives");
        return;
    }

    if (!net_set_nonblocking(&conn)) {
        ESP_LOGE(TAG, "could not set the socket non-blocking");
        return;
    }

    /* Hash task on core 1 (APP_CPU); WiFi and this network loop stay on core 0. */
    xTaskCreatePinnedToCore(hash_task, "hash", 8192, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "mining...");
    int submit_id = 100;
    for (;;) {
        /* Drain messages from the pool. */
        for (;;) {
            int r = net_poll_line(&conn, line, sizeof line);
            if (r < 0) {
                ESP_LOGE(TAG, "connection lost, restarting");
                esp_restart();
            }
            if (r == 0) {
                break;
            }

            double diff;
            int    rid;
            bool   accepted;
            if (stratum_parse_set_difficulty(line, &diff)) {
                uint8_t t[TARGET_SIZE];
                if (difficulty_to_target(diff, t)) {
                    xSemaphoreTake(s_lock, portMAX_DELAY);
                    memcpy(s_target, t, TARGET_SIZE);
                    s_have_target = true;
                    s_generation++;
                    xSemaphoreGive(s_lock);
                    ESP_LOGI(TAG, "difficulty -> %ld", (long)diff);
                }
            } else if (stratum_parse_notify(line, &scratch_job)) {
                xSemaphoreTake(s_lock, portMAX_DELAY);
                s_job = scratch_job;
                s_have_job = true;
                s_generation++;
                xSemaphoreGive(s_lock);
                ESP_LOGI(TAG, "job %s clean=%s", scratch_job.job_id,
                         scratch_job.clean_jobs ? "yes" : "no");
            } else if (stratum_parse_result(line, &rid, &accepted)) {
                if (rid == 2) {
                    ESP_LOGI(TAG, "authorized: %s", accepted ? "yes" : "no");
                } else {
                    ESP_LOGI(TAG, "%s (id %d)",
                             accepted ? "share ACCEPTED" : "share rejected", rid);
                }
            } else {
                ESP_LOGI(TAG, "<< %s", line);
            }
        }

        /* Send any shares the hash task found. */
        found_share_t fs;
        while (xQueueReceive(s_shares, &fs, 0) == pdTRUE) {
            char msg[512];
            if (stratum_serialize_submit(submit_id++, MINING_ADDRESS, fs.job_id,
                                         fs.extranonce2, fs.ntime, fs.nonce,
                                         msg, sizeof msg) >= 0) {
                net_send_line(&conn, msg);
                ESP_LOGI(TAG, "submitted share nonce %08x", (unsigned)fs.nonce);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}