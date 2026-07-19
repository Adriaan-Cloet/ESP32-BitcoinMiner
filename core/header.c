#include "header.h"

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
 * Decode exactly nbytes of hex into out. Returns false if the string is shorter,
 * longer, or contains a non-hex character. Requiring an exact length is what
 * turns a malformed job field into a clean rejection instead of a subtle bug.
 */
static bool hex_decode(const char *hex, uint8_t *out, size_t nbytes)
{
    for (size_t i = 0; i < nbytes; i++) {
        int hi = hex_value(hex[2 * i]);
        int lo = hex_value(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return hex[2 * nbytes] == '\0';
}

bool assemble_header(const char *version_hex,
                     const char *prevhash_hex,
                     const uint8_t merkle_root[32],
                     const char *ntime_hex,
                     const char *nbits_hex,
                     uint32_t nonce,
                     uint8_t out[BLOCK_HEADER_SIZE])
{
    if (version_hex == NULL || prevhash_hex == NULL || merkle_root == NULL ||
        ntime_hex == NULL || nbits_hex == NULL || out == NULL) {
        return false;
    }

    uint8_t version[4];
    uint8_t prevhash[32];
    uint8_t ntime[4];
    uint8_t nbits[4];
    if (!hex_decode(version_hex, version, sizeof version) ||
        !hex_decode(prevhash_hex, prevhash, sizeof prevhash) ||
        !hex_decode(ntime_hex, ntime, sizeof ntime) ||
        !hex_decode(nbits_hex, nbits, sizeof nbits)) {
        return false;
    }

    /* version: whole-field byte reversal. */
    for (size_t i = 0; i < 4; i++) {
        out[i] = version[3 - i];
    }

    /* prevhash: reverse the bytes within each 4-byte word, keep word order. */
    for (size_t w = 0; w < 8; w++) {
        for (size_t k = 0; k < 4; k++) {
            out[4 + w * 4 + k] = prevhash[w * 4 + (3 - k)];
        }
    }

    /* merkle root: already internal order, straight copy. */
    memcpy(out + 36, merkle_root, 32);

    /* ntime and nbits: whole-field byte reversal. */
    for (size_t i = 0; i < 4; i++) {
        out[68 + i] = ntime[3 - i];
        out[72 + i] = nbits[3 - i];
    }

    /* nonce: little-endian. */
    out[76] = (uint8_t)(nonce & 0xff);
    out[77] = (uint8_t)((nonce >> 8) & 0xff);
    out[78] = (uint8_t)((nonce >> 16) & 0xff);
    out[79] = (uint8_t)((nonce >> 24) & 0xff);

    return true;
}