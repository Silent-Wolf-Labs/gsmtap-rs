/* Integer, fill/padding and byte-transfer observations from the local C API. */
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <osmocom/core/bitvec.h>

_Static_assert(sizeof(unsigned int) * CHAR_BIT == 32, "32-bit unsigned ABI");
_Static_assert(sizeof(int) * CHAR_BIT == 32 && (int)UINT_MAX == -1, "signed reference ABI");
static unsigned case_index;
static const uint8_t seeds[] = { 0, 0xff, 0x2b };
static const uint64_t words[] = { 0, 1, UINT64_C(0x8000000080000000), UINT64_MAX };
static const unsigned widths[] = { 0, 1, 7, 8, 9, 15, 16, 31, 32, 33, 63, 64, 65, 255 };
static const unsigned wide_counts[] = { 256, 257, 288, 320, 321, 511, UINT_MAX };

static void hex(const uint8_t *data, unsigned len)
{
    for (unsigned i = 0; i < len; i++) printf("%02x", data[i]);
}

/* Do not observe undefined shifts or reads outside declared vector storage. */
static int safe_uint_read(unsigned len, unsigned pattern, unsigned cursor, unsigned n)
{
    const uint8_t data[] = { seeds[pattern], seeds[pattern] ^ 0xff, 0x2b, 0xa5, 0xff, 0x80, 0x01, 0x55 };
    for (unsigned i = 0; i < n; i++) {
        if (cursor / 8 >= len) return 1; /* defined capacity error before shifting */
        if ((data[cursor / 8] & (1 << (7 - cursor % 8))) && n - i - 1 >= 32) return 0;
        cursor++;
    }
    return 1;
}

static void observe(const char *api, unsigned len, unsigned pattern, unsigned cursor,
                    unsigned n, unsigned word_index, unsigned value, unsigned mode)
{
    uint8_t data[] = { seeds[pattern], seeds[pattern] ^ 0xff, 0x2b, 0xa5, 0xff, 0x80, 0x01, 0x55 };
    uint8_t io[] = { 0xa5, 0x00, 0xff, 0x2b, 0x80, 0x5a };
    uint8_t before[8]; memcpy(before, data, sizeof(data));
    struct bitvec bv = { .cur_bit = cursor, .data_len = len, .data = data };
    uint64_t word = words[word_index];
    int rc;
    if (!strcmp(api, "bitvec_set_u64")) rc = bitvec_set_u64(&bv, word, n, mode);
    else if (!strcmp(api, "bitvec_set_uint")) rc = bitvec_set_uint(&bv, (unsigned)word, n);
    else if (!strcmp(api, "bitvec_get_uint")) rc = bitvec_get_uint(&bv, n);
    else if (!strcmp(api, "bitvec_get_int16_msb")) rc = bitvec_get_int16_msb(&bv, n);
    else if (!strcmp(api, "bitvec_fill")) rc = bitvec_fill(&bv, n, (enum bit_value)value);
    else if (!strcmp(api, "bitvec_spare_padding")) rc = bitvec_spare_padding(&bv, n);
    else if (!strcmp(api, "bitvec_get_bytes")) rc = bitvec_get_bytes(&bv, io + 1, n);
    else if (!strcmp(api, "bitvec_set_bytes")) rc = bitvec_set_bytes(&bv, io + 1, n);
    else __builtin_trap();
    if (case_index) puts(",");
    printf("{\"case\":%u,\"api\":\"%s\",", case_index++, api);
    printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
    printf("\"source_sha256\":\"789ef706bc0c71f497400f831e920c9efad324dfaea99b13292fca717b4f480e\",");
    printf("\"header_sha256\":\"db895f6eb8bfada1d6764248d122128b0a0bb530ec5f4a198953f56f223215b6\",");
    printf("\"test_sha256\":\"5b4a1a3f8c5bafd4113f7b4634f3c80061c274356e05c328d270fb40dfb007f5\",");
    printf("\"c_unsigned_bits\":32,\"data_len\":%u,\"pattern\":%u,", len, pattern);
    printf("\"cursor_before\":%u,\"num_bits\":%u,\"word_hex\":\"%016" PRIx64 "\",", cursor, n, word);
    printf("\"value\":%u,\"mode\":%u,\"before_hex\":\"", value, mode); hex(before, 8);
    printf("\",\"io_before_hex\":\"a500ff2b805a\",\"return_code\":%d,\"cursor_after\":%u,\"after_hex\":\"", rc, bv.cur_bit);
    hex(data, 8); printf("\",\"io_after_hex\":\""); hex(io, 6); printf("\"}");
}

int main(void)
{
    puts("[");
    for (unsigned len_index = 0; len_index < 5; len_index++) {
        unsigned len = (unsigned[]){ 0, 1, 2, 4, 8 }[len_index];
        for (unsigned pattern = 0; pattern < 3; pattern++) {
            unsigned cursors[] = { 0, 1, 7, 8, 15, len * 8, len * 8 + 1, UINT_MAX };
            for (unsigned ci = 0; ci < 8; ci++) {
                unsigned cursor = cursors[ci];
                for (unsigned wi = 0; wi < sizeof(widths)/sizeof(*widths); wi++) {
                    unsigned n = widths[wi];
                    for (unsigned word = 0; word < 4; word++) {
                        for (unsigned mode = 0; mode < 2; mode++)
                            observe("bitvec_set_u64", len, pattern, cursor, n, word, 0, mode);
                        observe("bitvec_set_uint", len, pattern, cursor, n, word, 0, 0);
                    }
                    if (safe_uint_read(len, pattern, cursor, n))
                        observe("bitvec_get_uint", len, pattern, cursor, n, 0, 0, 0);
                    if (n > 15 || cursor < n || n == 0 || len >= (n + 7) / 8)
                        observe("bitvec_get_int16_msb", len, pattern, cursor, n, 0, 0, 0);
                    for (unsigned value = 0; value < 4; value++)
                        observe("bitvec_fill", len, pattern, cursor, n, 0, value, 0);
                    observe("bitvec_spare_padding", len, pattern, cursor, n, 0, 0, 0);
                }
                for (unsigned wi = 0; wi < sizeof(wide_counts)/sizeof(*wide_counts); wi++) {
                    unsigned n = wide_counts[wi];
                    observe("bitvec_set_uint", len, pattern, cursor, n, 3, 0, 0);
                    for (unsigned value = 0; value < 4; value++)
                        observe("bitvec_fill", len, pattern, cursor, n, 0, value, 0);
                    observe("bitvec_spare_padding", len, pattern, cursor, n, 0, 0, 0);
                }
                for (unsigned n = 0; n <= 4; n++) {
                    observe("bitvec_get_bytes", len, pattern, cursor, n, 0, 0, 0);
                    observe("bitvec_set_bytes", len, pattern, cursor, n, 0, 0, 0);
                }
            }
        }
    }
    /* Exercise every byte-transfer alignment with preserved prefix/suffix bits. */
    for (unsigned pattern = 0; pattern < 3; pattern++)
        for (unsigned cursor = 0; cursor < 32; cursor++)
            for (unsigned n = 0; n <= 4; n++) {
                observe("bitvec_get_bytes", 8, pattern, cursor, n, 0, 0, 0);
                observe("bitvec_set_bytes", 8, pattern, cursor, n, 0, 0, 0);
            }
    puts("\n]");
    return 0;
}
