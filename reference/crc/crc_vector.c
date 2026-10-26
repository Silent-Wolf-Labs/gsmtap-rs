/* Direct-call CRC evidence; expected results come only from local C APIs. */
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osmocom/core/crc16.h>
#include <osmocom/core/crcgen.h>

static void hex(const uint8_t *bytes, unsigned len)
{
    for (unsigned i = 0; i < len; ++i)
        printf("%02x", bytes[i]);
}
static uint64_t mask(unsigned width)
{
    return width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
}
static void input(uint8_t *bytes, unsigned len, unsigned mode)
{
    const uint8_t nonbinary[] = {0, 1, 2, 3, 0x80, 0xff};
    for (unsigned i = 0; i < len; ++i)
        bytes[i] = mode == 0 ? 0 : mode == 1 ? 1 : mode == 2 ? (i & 1) : nonbinary[i % 6];
}
static uint64_t compute(unsigned width, int bits, uint64_t poly, uint64_t init,
                        uint64_t remainder, const uint8_t *in, int len)
{
#define CALL(W) case W: { struct osmo_crc##W##gen_code code = {bits, poly, init, remainder}; return osmo_crc##W##gen_compute_bits(&code, in, len); }
    switch (width) { CALL(8) CALL(16) CALL(32) CALL(64) }
#undef CALL
    abort();
}
static void set(unsigned width, int bits, uint64_t poly, uint64_t init,
                uint64_t remainder, const uint8_t *in, int len, uint8_t *out)
{
#define CALL(W) case W: { struct osmo_crc##W##gen_code code = {bits, poly, init, remainder}; osmo_crc##W##gen_set_bits(&code, in, len, out); return; }
    switch (width) { CALL(8) CALL(16) CALL(32) CALL(64) }
#undef CALL
    abort();
}
static int check(unsigned width, int bits, uint64_t poly, uint64_t init,
                 uint64_t remainder, const uint8_t *in, int len, const uint8_t *out)
{
#define CALL(W) case W: { struct osmo_crc##W##gen_code code = {bits, poly, init, remainder}; return osmo_crc##W##gen_check_bits(&code, in, len, out); }
    switch (width) { CALL(8) CALL(16) CALL(32) CALL(64) }
#undef CALL
    abort();
}
static void byte_vectors(void)
{
    const uint16_t states[] = {0, 1, 0xffff, 0x5555, 0xaaaa};
    const unsigned lengths[] = {0, 1, 2, 7, 8, 16, 31, 32, 255, 256};
    unsigned id = 0;
    for (unsigned flavor = 0; flavor < 2; ++flavor) {
        for (unsigned byte = 0; byte < 256; ++byte)
            printf("{\"kind\":\"table\",\"case\":%u,\"flavor\":%u,\"byte\":%u,\"result\":%u}\n",
                   id++, flavor, byte, flavor ? osmo_crc16_ccitt_table[byte] : osmo_crc16_table[byte]);
        for (unsigned state = 0; state < 5; ++state)
        for (unsigned byte = 0; byte < 256; ++byte)
            printf("{\"kind\":\"byte\",\"case\":%u,\"flavor\":%u,\"init\":%u,\"byte\":%u,\"result\":%u}\n",
                   id++, flavor, states[state], byte,
                   flavor ? osmo_crc16_ccitt_byte(states[state], byte) : osmo_crc16_byte(states[state], byte));
        for (unsigned state = 0; state < 5; ++state)
        for (unsigned mode = 0; mode < 4; ++mode)
        for (unsigned l = 0; l < 10; ++l) {
            uint8_t in[260], before[260];
            for (unsigned i = 0; i < 260; ++i)
                in[i] = mode == 0 ? 0 : mode == 1 ? 0xff : mode == 2 ? (i & 1 ? 0x55 : 0xaa) : (uint8_t)(i * 73 + 19);
            memcpy(before, in, sizeof(in));
            unsigned len = lengths[l], split = len / 2;
            uint16_t result = flavor ? osmo_crc16_ccitt(states[state], len ? in : NULL, len) : osmo_crc16(states[state], len ? in : NULL, len);
            uint16_t mid = flavor ? osmo_crc16_ccitt(states[state], in, split) : osmo_crc16(states[state], in, split);
            uint16_t continuation = flavor ? osmo_crc16_ccitt(mid, in + split, len - split) : osmo_crc16(mid, in + split, len - split);
            printf("{\"kind\":\"buffer\",\"case\":%u,\"flavor\":%u,\"init\":%u,\"mode\":%u,\"len\":%u,\"split\":%u,\"result\":%u,\"mid\":%u,\"continuation\":%u,\"before\":\"",
                   id++, flavor, states[state], mode, len, split, result, mid, continuation);
            hex(before, sizeof(before)); printf("\",\"after\":\""); hex(in, sizeof(in)); puts("\"}");
        }
    }
}
static void generic_vectors(void)
{
    const unsigned widths[] = {8, 16, 32, 64};
    const int lengths[] = {INT_MIN, -1, 0, 1, 7, 32};
    unsigned id = 0;
    for (unsigned w = 0; w < 4; ++w)
    for (unsigned bits = 1; bits <= widths[w]; ++bits)
    for (unsigned variant = 0; variant < 4; ++variant)
    for (unsigned mode = 0; mode < 4; ++mode)
    for (unsigned l = 0; l < 6; ++l)
    for (unsigned suffix = 0; suffix < 2; ++suffix) {
        unsigned width = widths[w]; int len = lengths[l];
        /* Positive full-width 32/64 invokes UB in the upstream mask. */
        if (width >= 32 && bits == width && len > 0)
            continue;
        uint64_t storage = mask(width);
        uint64_t poly = variant == 0 ? 0 : variant == 1 ? 3 : variant == 2 ? storage : UINT64_C(0xaaaaaaaaaaaaaaaa) & storage;
        uint64_t init = variant < 2 ? variant : variant == 2 ? storage : UINT64_C(0x5555555555555555) & storage;
        uint64_t remainder = variant == 0 ? 0 : variant == 1 ? storage : variant == 2 ? 1 : storage;
        uint8_t in[36], in_before[36], out[67], before[67], flips[64], nonbinary[64];
        input(in, sizeof(in), mode); memcpy(in_before, in, sizeof(in));
        unsigned capacity = bits + (suffix ? 3 : 0);
        memset(out, 0x7e, capacity); memcpy(before, out, capacity);
        const uint8_t *source = len <= 0 ? NULL : in;
        uint64_t result = compute(width, bits, poly, init, remainder, source, len);
        set(width, bits, poly, init, remainder, source, len, out);
        int match = check(width, bits, poly, init, remainder, source, len, out);
        for (unsigned bit = 0; bit < bits; ++bit) {
            out[bit] ^= 1;
            flips[bit] = check(width, bits, poly, init, remainder, source, len, out);
            out[bit] ^= 1;
            out[bit] ^= 2;
            nonbinary[bit] = check(width, bits, poly, init, remainder, source, len, out);
            out[bit] ^= 2;
        }
        printf("{\"kind\":\"generic\",\"case\":%u,\"width\":%u,\"bits\":%u,\"variant\":%u,\"mode\":%u,\"len\":%d,\"capacity\":%u,\"poly\":%"PRIu64",\"init\":%"PRIu64",\"remainder\":%"PRIu64",\"result\":%"PRIu64",\"match\":%d,\"input_before\":\"",
               id++, width, bits, variant, mode, len, capacity, poly, init, remainder, result, match);
        hex(in_before, sizeof(in)); printf("\",\"input_after\":\""); hex(in, sizeof(in));
        printf("\",\"before\":\""); hex(before, capacity); printf("\",\"after\":\""); hex(out, capacity);
        printf("\",\"flips\":\""); hex(flips, bits); printf("\",\"nonbinary\":\""); hex(nonbinary, bits); puts("\"}");
    }
}
int main(int argc, char **argv)
{
    if (sizeof(int) != 4 || sizeof(uint64_t) != 8)
        return 1;
    if (argc == 5 && strcmp(argv[1], "audit") == 0) {
        unsigned width = strtoul(argv[2], NULL, 10);
        int bits = strtol(argv[3], NULL, 10), len = strtol(argv[4], NULL, 10);
        uint8_t in[36] = {1}, out[67] = {0};
        (void)compute(width, bits, mask(width), mask(width), mask(width), in, len);
        set(width, bits, mask(width), mask(width), mask(width), in, len, out);
        (void)check(width, bits, mask(width), mask(width), mask(width), in, len, out);
        return 0;
    }
    if (argc != 3 || (strcmp(argv[1], "byte") && strcmp(argv[1], "generic")))
        return 1;
    printf("{\"kind\":\"metadata\",\"stage\":\"%s\",\"revision\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",\"int_bits\":%zu,\"manifest\":\"%s\"}\n", argv[1], sizeof(int)*CHAR_BIT, argv[2]);
    if (!strcmp(argv[1], "byte")) byte_vectors(); else generic_vectors();
    return 0;
}
