/*
 * SHA-256 block compression in Xtensa LX6 assembly (ESP32).
 *
 * Same contract as the C version in sha256_compress.c:
 *     void sha256_compress(uint32_t state[8], const uint8_t block[64]);
 *   a2 = state pointer, a3 = block pointer (windowed ABI).
 *
 * Rotate-right is done with the funnel shift: `ssai n; src rd, x, x` gives
 * ROTR(x, n). The message schedule is expanded into a 64-word buffer on the
 * stack, then 64 rounds run out of registers with the eight working variables
 * a..h held in a4..a11. Correctness is enforced by the on-device self-test,
 * which compares this against the reference over many random headers.
 *
 * Stack frame (from a1): W[64] at +16, saved state pointer at +272, t1 spill
 * at +276.
 */

    .section .rodata
    .align 4
sha256_k_asm:
    .word 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
    .word 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
    .word 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
    .word 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
    .word 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
    .word 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
    .word 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
    .word 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
    .word 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
    .word 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
    .word 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
    .word 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
    .word 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
    .word 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
    .word 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
    .word 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2

    .text
    .align 4
    .global sha256_compress
    .type   sha256_compress, @function
sha256_compress:
    entry   a1, 320
    s32i    a2, a1, 272             /* save state pointer for the end */

    /* ---- load the 16 big-endian message words into W[0..15] ---- */
    mov     a4, a3                  /* a4 = block pointer */
    addi    a5, a1, 16              /* a5 = &W[0] */
    movi    a6, 16
.Lload:
    l8ui    a7, a4, 0
    l8ui    a8, a4, 1
    slli    a7, a7, 8
    or      a7, a7, a8
    l8ui    a8, a4, 2
    slli    a7, a7, 8
    or      a7, a7, a8
    l8ui    a8, a4, 3
    slli    a7, a7, 8
    or      a7, a7, a8              /* a7 = big-endian word */
    s32i    a7, a5, 0
    addi    a4, a4, 4
    addi    a5, a5, 4
    addi    a6, a6, -1
    bnez    a6, .Lload

    /* ---- expand W[16..63]; a4 = &W[i-16], offsets are all positive ---- */
    addi    a4, a1, 16              /* &W[0] */
    movi    a6, 48
.Lext:
    l32i    a7, a4, 56              /* W[i-2] */
    ssai    17
    src     a8, a7, a7
    ssai    19
    src     a9, a7, a7
    xor     a8, a8, a9
    srli    a9, a7, 10
    xor     a8, a8, a9              /* SSIG1(W[i-2]) */
    l32i    a9, a4, 36              /* W[i-7] */
    add     a8, a8, a9
    l32i    a7, a4, 4               /* W[i-15] */
    ssai    7
    src     a9, a7, a7
    ssai    18
    src     a10, a7, a7
    xor     a9, a9, a10
    srli    a10, a7, 3
    xor     a9, a9, a10             /* SSIG0(W[i-15]) */
    add     a8, a8, a9
    l32i    a9, a4, 0               /* W[i-16] */
    add     a8, a8, a9
    s32i    a8, a4, 64              /* W[i] */
    addi    a4, a4, 4
    addi    a6, a6, -1
    bnez    a6, .Lext

    /* ---- load the working state a..h into a4..a11 ---- */
    l32i    a4,  a2, 0              /* a */
    l32i    a5,  a2, 4              /* b */
    l32i    a6,  a2, 8              /* c */
    l32i    a7,  a2, 12             /* d */
    l32i    a8,  a2, 16            /* e */
    l32i    a9,  a2, 20            /* f */
    l32i    a10, a2, 24            /* g */
    l32i    a11, a2, 28            /* h */

    /* ---- 64 rounds ---- */
    movi    a12, sha256_k_asm       /* K base */
    addi    a13, a1, 16             /* W base */
    movi    a14, 0                  /* round index i */
.Lround:
    addx4   a15, a14, a12           /* &K[i] */
    l32i    a2,  a15, 0             /* K[i] */
    addx4   a15, a14, a13           /* &W[i] */
    l32i    a15, a15, 0             /* W[i] */
    add     a2,  a2, a15
    add     a2,  a2, a11            /* + h */
    /* BSIG1(e), e = a8 */
    ssai    6
    src     a15, a8, a8
    ssai    11
    src     a3,  a8, a8
    xor     a15, a15, a3
    ssai    25
    src     a3,  a8, a8
    xor     a15, a15, a3            /* BSIG1(e) */
    add     a2,  a2, a15
    /* CH(e,f,g) = (e&f) ^ (~e & g) */
    and     a15, a8, a9             /* e&f */
    movi    a3,  -1
    xor     a3,  a8, a3             /* ~e */
    and     a3,  a3, a10            /* ~e & g */
    xor     a15, a15, a3            /* CH */
    add     a2,  a2, a15            /* a2 = t1 */
    s32i    a2,  a1, 276            /* spill t1 */
    /* BSIG0(a), a = a4 */
    ssai    2
    src     a2,  a4, a4
    ssai    13
    src     a3,  a4, a4
    xor     a2,  a2, a3
    ssai    22
    src     a3,  a4, a4
    xor     a2,  a2, a3             /* BSIG0(a) */
    /* MAJ(a,b,c) = (a&b) ^ (c & (a^b)) */
    xor     a3,  a4, a5             /* a^b */
    and     a3,  a3, a6             /* c & (a^b) */
    and     a15, a4, a5             /* a&b */
    xor     a3,  a15, a3            /* MAJ */
    add     a2,  a2, a3             /* a2 = t2 */
    /* rotate the working variables */
    mov     a11, a10                /* h = g */
    mov     a10, a9                 /* g = f */
    mov     a9,  a8                 /* f = e */
    l32i    a15, a1, 276            /* t1 */
    add     a8,  a7, a15            /* e = d + t1 */
    mov     a7,  a6                 /* d = c */
    mov     a6,  a5                 /* c = b */
    mov     a5,  a4                 /* b = a */
    add     a4,  a2, a15            /* a = t2 + t1 */
    addi    a14, a14, 1
    movi    a3,  64
    bne     a14, a3, .Lround

    /* ---- add the working state back into state[] ---- */
    l32i    a2, a1, 272             /* state pointer */
    l32i    a3, a2, 0
    add     a3, a3, a4
    s32i    a3, a2, 0
    l32i    a3, a2, 4
    add     a3, a3, a5
    s32i    a3, a2, 4
    l32i    a3, a2, 8
    add     a3, a3, a6
    s32i    a3, a2, 8
    l32i    a3, a2, 12
    add     a3, a3, a7
    s32i    a3, a2, 12
    l32i    a3, a2, 16
    add     a3, a3, a8
    s32i    a3, a2, 16
    l32i    a3, a2, 20
    add     a3, a3, a9
    s32i    a3, a2, 20
    l32i    a3, a2, 24
    add     a3, a3, a10
    s32i    a3, a2, 24
    l32i    a3, a2, 28
    add     a3, a3, a11
    s32i    a3, a2, 28

    retw.n
    .size   sha256_compress, .-sha256_compress