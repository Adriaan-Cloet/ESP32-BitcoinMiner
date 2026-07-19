#include "coinbase.h"

#include <string.h>

/* One hex digit to its value, or -1 if it is not a hex digit. */
static int hex_value(char c)
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

/*
 * Decode a whole hex string of any even length into out. Returns the number of
 * bytes written, or -1 on odd length, a non-hex character, or overflow. An odd
 * length is caught because the second nibble reads the terminating '\0', which
 * hex_value rejects.
 */
static int decode_hex_string(const char *hex, uint8_t *out, size_t out_size)
{
    size_t i = 0;
    while (hex[2 * i] != '\0') {
        if (i >= out_size) {
            return -1;
        }
        int hi = hex_value(hex[2 * i]);
        int lo = hex_value(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
        i++;
    }
    return (int)i;
}

int coinbase_assemble(const char *coinb1_hex,
                      const char *extranonce1_hex,
                      const char *extranonce2_hex,
                      const char *coinb2_hex,
                      uint8_t *out, size_t out_size)
{
    if (coinb1_hex == NULL || extranonce1_hex == NULL ||
        extranonce2_hex == NULL || coinb2_hex == NULL || out == NULL) {
        return -1;
    }

    const char *parts[4] = {
        coinb1_hex, extranonce1_hex, extranonce2_hex, coinb2_hex
    };

    size_t total = 0;
    for (size_t p = 0; p < 4; p++) {
        int n = decode_hex_string(parts[p], out + total, out_size - total);
        if (n < 0) {
            return -1;
        }
        total += (size_t)n;
    }
    return (int)total;
}

bool coinbase_pays_script(const uint8_t *coinbase, size_t len,
                          const char *scriptpubkey_hex)
{
    if (coinbase == NULL || scriptpubkey_hex == NULL) {
        return false;
    }

    uint8_t script[64];
    int n = decode_hex_string(scriptpubkey_hex, script, sizeof script);
    if (n <= 0 || (size_t)n > len) {
        return false;
    }

    size_t script_len = (size_t)n;
    for (size_t i = 0; i + script_len <= len; i++) {
        if (memcmp(coinbase + i, script, script_len) == 0) {
            return true;
        }
    }
    return false;
}