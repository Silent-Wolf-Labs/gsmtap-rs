/* Third-stage direct calls: indexed fields, search, runs, shifts, arrays. */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <osmocom/core/bitvec.h>

_Static_assert(sizeof(unsigned) * CHAR_BIT == 32 && sizeof(int) * CHAR_BIT == 32, "32-bit ABI");
_Static_assert((int)UINT_MAX == -1, "signed position ABI");
_Static_assert(EOVERFLOW == 75 && EINTR == 4, "recorded errno ABI");
static unsigned case_index;
static const uint8_t seeds[] = { 0, 0xff, 0x2b, 0x80, 0xfe, 0x7f, 0x55, 0xa5 };
static const uint64_t words[] = { 0, UINT64_C(0x8000000000000001), UINT64_MAX };
static const uint32_t array[] = { 0, UINT32_MAX, UINT32_C(0x80000000), UINT32_C(0x12345678) };
static const unsigned widths[] = { 0, 1, 7, 8, 9, 31, 32, 63, 64, 65, 255, 256, 257, 320, 321, UINT_MAX };
static const unsigned shifts[] = { 0, 1, 2, 7, 8, 9, 15, 16, 24, 31, 32, 64, 65, UINT_MAX };
static const unsigned array_widths[] = { 0, 1, 8, 32, 64, 65, 256, 257, UINT_MAX };

static void initial(uint8_t *data, unsigned pattern)
{
    uint8_t seed = seeds[pattern];
    const uint8_t source[] = { seed, seed, seed ^ 0xff, 0x2b, 0xa5, 0, 0xff, 0x80 };
    memcpy(data, source, 8);
}
static void hex(const uint8_t *data) { for (unsigned i = 0; i < 8; i++) printf("%02x", data[i]); }

/* Reject unsafe C spans/shifts; this is an input filter, not an output oracle. */
static int safe_read(unsigned len, unsigned pattern, unsigned index, unsigned n)
{
    if (n && (index + n - 1) / 8 >= len) return 1; /* C's defined errno failure */
    if ((uint64_t)index + n > (uint64_t)len * 8 && n) return 0;
    uint8_t data[8]; initial(data, pattern);
    for (unsigned i = 0; i < n; i++)
        if ((data[(index + i) / 8] & (1 << (7 - (index + i) % 8))) && n - i - 1 >= 64)
            return 0;
    return 1;
}

static void observe(const char *api, unsigned len, unsigned pattern, unsigned cursor,
                    unsigned index, unsigned arg, unsigned word_index, unsigned value,
                    unsigned array_len, unsigned dry_run)
{
    uint8_t data[8], before[8]; initial(data, pattern); memcpy(before, data, 8);
    struct bitvec bv = { .cur_bit = cursor, .data_len = len, .data = data };
    unsigned index_after = index;
    uint64_t read_value = 0, word = words[word_index];
    int64_t rc;
    errno = EINTR;
    if (!strcmp(api, "bitvec_read_field")) { read_value = bitvec_read_field(&bv, &index_after, arg); rc = 0; }
    else if (!strcmp(api, "bitvec_write_field")) rc = bitvec_write_field(&bv, &index_after, word, arg);
    else if (!strcmp(api, "bitvec_get_nth_set_bit")) rc = bitvec_get_nth_set_bit(&bv, arg);
    else if (!strcmp(api, "bitvec_find_bit_pos")) rc = bitvec_find_bit_pos(&bv, arg, (enum bit_value)value);
    else if (!strcmp(api, "bitvec_rl")) rc = bitvec_rl(&bv, value);
    else if (!strcmp(api, "bitvec_rl_curbit")) rc = bitvec_rl_curbit(&bv, value, arg);
    else if (!strcmp(api, "bitvec_shiftl")) { bitvec_shiftl(&bv, arg); rc = 0; }
    else if (!strcmp(api, "bitvec_add_array")) rc = bitvec_add_array(&bv, array, array_len, dry_run, arg);
    else __builtin_trap();
    int error = errno;
    if (case_index) puts(",");
    printf("{\"case\":%u,\"api\":\"%s\",", case_index++, api);
    printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
    printf("\"source_sha256\":\"789ef706bc0c71f497400f831e920c9efad324dfaea99b13292fca717b4f480e\",");
    printf("\"header_sha256\":\"db895f6eb8bfada1d6764248d122128b0a0bb530ec5f4a198953f56f223215b6\",");
    printf("\"test_sha256\":\"5b4a1a3f8c5bafd4113f7b4634f3c80061c274356e05c328d270fb40dfb007f5\",");
    printf("\"c_unsigned_bits\":32,\"data_len\":%u,\"pattern\":%u,\"cursor_before\":%u,", len, pattern, cursor);
    printf("\"index_before\":%u,\"argument\":%u,\"word_hex\":\"%016" PRIx64 "\",", index, arg, word);
    printf("\"value\":%u,\"array_len\":%u,\"dry_run\":%u,", value, array_len, dry_run);
    printf("\"array_hex\":\"00000000ffffffff8000000012345678\",\"before_hex\":\""); hex(before);
    printf("\",\"return_code\":%lld,\"return_hex\":\"%016" PRIx64 "\",", (long long)rc, read_value);
    printf("\"errno_after\":%d,\"index_after\":%u,\"cursor_after\":%u,\"after_hex\":\"", error, index_after, bv.cur_bit);
    hex(data); printf("\"}");
}

int main(void)
{
    puts("[");
    /* Fields: independent initial cursor/index, narrowing, partial writes. */
    for (unsigned li = 0; li < 5; li++) {
        unsigned len = (unsigned[]){ 0, 1, 2, 4, 8 }[li];
        for (unsigned pattern = 0; pattern < 3; pattern++) {
            for (unsigned ci = 0; ci < 2; ci++) {
                unsigned cursor = ci ? UINT_MAX : 3;
                unsigned indices[] = { 0, 1, 7, 8, len * 8, len * 8 + 1, UINT_MAX };
                for (unsigned ii = 0; ii < 7; ii++) {
                    for (unsigned wi = 0; wi < sizeof(widths)/sizeof(*widths); wi++) {
                        unsigned n = widths[wi], index = indices[ii];
                        if (safe_read(len, pattern, index, n))
                            observe("bitvec_read_field", len, pattern, cursor, index, n, 0, 0, 0, 0);
                        for (unsigned word = 0; word < 3; word++)
                            observe("bitvec_write_field", len, pattern, cursor, index, n, word, 0, 0, 0);
                    }
                }
            }
        }
    }
    for (unsigned li = 0; li < 6; li++) {
        unsigned len = (unsigned[]){ 0, 1, 2, 3, 4, 8 }[li], cap = len * 8;
        for (unsigned pattern = 0; pattern < 8; pattern++) {
            unsigned ordinals[] = { 0, 1, 2, 7, 8, 9, 32, 64, 65, UINT_MAX };
            for (unsigned ni = 0; ni < 10; ni++)
                observe("bitvec_get_nth_set_bit", len, pattern, UINT_MAX, 0, ordinals[ni], 0, 0, 0, 0);
            unsigned starts[] = { 0, 1, 7, 8, cap ? cap - 1 : 0, cap, cap + 1, UINT_MAX };
            for (unsigned ni = 0; ni < 8; ni++)
                for (unsigned value = 0; value < 4; value++)
                    observe("bitvec_find_bit_pos", len, pattern, UINT_MAX, 0, starts[ni], 0, value, 0, 0);
            for (unsigned ci = 0; ci <= cap + 3; ci++) {
                unsigned cursor = ci == cap + 2 ? UINT_MAX - 7 : ci == cap + 3 ? UINT_MAX : ci;
                for (unsigned value = 0; value < 2; value++) {
                    if (cursor <= cap || (len && seeds[pattern] != (value ? 0xff : 0)))
                        observe("bitvec_rl", len, pattern, cursor, 0, 0, 0, value, 0, 0);
                    unsigned limits[] = { 0, 1, 7, 8, cap };
                    for (unsigned mi = 0; mi < 5; mi++)
                        if (limits[mi] <= cap)
                            observe("bitvec_rl_curbit", len, pattern, cursor, 0, limits[mi], 0, value, 0, 0);
                }
            }
            unsigned cursors[] = { 0, 1, 7, 8, cap, cap + 1, UINT_MAX };
            for (unsigned ci = 0; ci < 7; ci++)
                for (unsigned si = 0; si < sizeof(shifts)/sizeof(*shifts); si++) {
                    unsigned n = shifts[si], cursor = cursors[ci];
                    if (!n || n >= cursor || (len >= 2 && n / 8 <= len))
                        observe("bitvec_shiftl", len, pattern, cursor, 0, n, 0, 0, 0, 0);
                }
        }
    }
    for (unsigned li = 0; li < 5; li++) {
        unsigned len = (unsigned[]){ 0, 1, 2, 4, 8 }[li];
        for (unsigned pattern = 0; pattern < 3; pattern++) {
            unsigned cursors[] = { 0, 7, len * 8, UINT_MAX };
            for (unsigned ci = 0; ci < 4; ci++)
                for (unsigned ai = 0; ai < 3; ai++)
                    for (unsigned wi = 0; wi < sizeof(array_widths)/sizeof(*array_widths); wi++)
                        for (unsigned dry = 0; dry < 2; dry++)
                            observe("bitvec_add_array", len, pattern, cursors[ci], 0, array_widths[wi], 0, 0, (unsigned[]){0,1,4}[ai], dry);
        }
    }
    /* Defined early overflow in read_field, used by the cursor-run routine. */
    observe("bitvec_rl_curbit", 1, 0, 9, 0, 16, 0, 1, 0, 0);
    /* Oversized limits with defined early mismatch: no out-of-bounds read. */
    observe("bitvec_rl_curbit", 1, 0, 0, 0, UINT_MAX, 0, 1, 0, 0);
    observe("bitvec_rl_curbit", 1, 3, 0, 0, UINT_MAX, 0, 0, 0, 0);
    puts("\n]");
    return 0;
}
