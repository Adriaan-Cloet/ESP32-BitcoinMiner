/*
 * On-device SHA-256 self-test firmware.
 *
 * The host test bench runs on x86 and so cannot exercise the Xtensa assembly.
 * This firmware runs the same differential check on the ESP32 itself: for many
 * random 80-byte headers it compares the fast path (sha256_midstate +
 * sha256d_finish, using whichever sha256_compress the build selected) against
 * the reference, and reports PASS or FAIL. It then measures raw throughput so
 * the C and assembly versions can be compared cleanly, away from the miner's
 * other work.
 *
 * Build it with:  idf.py -DMINER_SELFTEST=ON [-DUSE_XTENSA_SHA256=ON] build
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "sha256_fast.h"
#include "sha256_ref.h"

static const char *TAG = "selftest";

static uint32_t rng_state = 0x01234567u;

static uint32_t xorshift(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static void to_hex(const uint8_t *data, int len, char *out)
{
    static const char d[] = "0123456789abcdef";
    for (int i = 0; i < len; i++) {
        out[i * 2 + 0] = d[data[i] >> 4];
        out[i * 2 + 1] = d[data[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

void app_main(void)
{
    ESP_LOGI(TAG, "SHA-256 differential self-test starting");

    const int headers = 20000;
    int mismatches = 0;

    for (int t = 0; t < headers; t++) {
        uint8_t header[80];
        for (int i = 0; i < 80; i++) {
            header[i] = (uint8_t)xorshift();
        }

        uint8_t ref[32];
        sha256d_ref(header, 80, ref);

        uint32_t mid[8];
        sha256_midstate(header, mid);
        uint8_t fast[32];
        sha256d_finish(mid, header + 64, fast);

        if (memcmp(ref, fast, 32) != 0) {
            if (mismatches == 0) {
                char hh[161];
                char rh[65];
                char fh[65];
                to_hex(header, 80, hh);
                to_hex(ref, 32, rh);
                to_hex(fast, 32, fh);
                ESP_LOGE(TAG, "first mismatch at header %d:", t);
                ESP_LOGE(TAG, "  input %s", hh);
                ESP_LOGE(TAG, "  ref   %s", rh);
                ESP_LOGE(TAG, "  got   %s", fh);
            }
            mismatches++;
        }
        if ((t & 0x3ff) == 0) {
            vTaskDelay(1); /* keep the task watchdog fed */
        }
    }

    if (mismatches == 0) {
        ESP_LOGI(TAG, "PASS: %d headers match the reference", headers);
    } else {
        ESP_LOGE(TAG, "FAIL: %d of %d headers mismatched", mismatches, headers);
    }

    /*
     * Throughput: grind one template's nonce space for a while and time it.
     * This is a purer hash-rate number than the miner reports, without the
     * coinbase, merkle and network work around it.
     */
    uint8_t header[80];
    for (int i = 0; i < 80; i++) {
        header[i] = (uint8_t)xorshift();
    }
    uint32_t mid[8];
    sha256_midstate(header, mid);
    uint8_t tail[16];
    memcpy(tail, header + 64, 16);
    uint8_t out[32];

    const uint32_t iters = 200000;
    int64_t start = esp_timer_get_time();
    for (uint32_t n = 0; n < iters; n++) {
        tail[12] = (uint8_t)(n & 0xff);
        tail[13] = (uint8_t)((n >> 8) & 0xff);
        tail[14] = (uint8_t)((n >> 16) & 0xff);
        tail[15] = (uint8_t)((n >> 24) & 0xff);
        sha256d_finish(mid, tail, out);
        if ((n & 0x3fff) == 0) {
            vTaskDelay(1);
        }
    }
    int64_t dt = esp_timer_get_time() - start;
    unsigned long hps = (unsigned long)((uint64_t)iters * 1000000ULL / (uint64_t)dt);
    ESP_LOGI(TAG, "throughput: %lu H/s (%lu hashes, tiny yield overhead included)",
             hps, (unsigned long)iters);

    ESP_LOGI(TAG, "self-test done");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}