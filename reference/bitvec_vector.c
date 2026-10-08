/* Direct-call observations for the first bitvec conversion stage. */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <osmocom/core/bitvec.h>

_Static_assert(sizeof(unsigned int) * CHAR_BIT == 32, "fixture requires 32-bit unsigned int");

static const enum bit_value sequence[] = { ZERO, ONE, L, H, ONE, ZERO };
static unsigned int case_index;

static void hex(const uint8_t *data)
{
    for (unsigned int i = 0; i < 4; i++)
        printf("%02x", data[i]);
}

static void observe(const char *api, unsigned int len, unsigned int pattern,
                    unsigned int cursor, unsigned int pos, unsigned int value,
                    unsigned int count)
{
    static const uint8_t patterns[] = { 0, 0xff, 0x2b };
    uint8_t data[] = { patterns[pattern], patterns[pattern] ^ 0xff, 0x2b, 0xa5 };
    uint8_t before[4];
    memcpy(before, data, sizeof(data));
    struct bitvec bv = { .cur_bit = cursor, .data_len = len, .data = data };
    int64_t rc;
    if (!strcmp(api, "bitvec_get_bit_pos"))
        rc = (int)bitvec_get_bit_pos(&bv, pos);
    else if (!strcmp(api, "bitvec_get_bit_pos_high"))
        rc = (int)bitvec_get_bit_pos_high(&bv, pos);
    else if (!strcmp(api, "bitvec_set_bit_pos"))
        rc = bitvec_set_bit_pos(&bv, pos, (enum bit_value)value);
    else if (!strcmp(api, "bitvec_set_bit"))
        rc = bitvec_set_bit(&bv, (enum bit_value)value);
    else if (!strcmp(api, "bitvec_get_bit_high"))
        rc = bitvec_get_bit_high(&bv);
    else if (!strcmp(api, "bitvec_set_bits"))
        rc = bitvec_set_bits(&bv, sequence, count);
    else if (!strcmp(api, "bitvec_zero")) {
        bitvec_zero(&bv);
        rc = 0;
    } else if (!strcmp(api, "bitvec_used_bytes"))
        rc = bitvec_used_bytes(&bv);
    else if (!strcmp(api, "bitvec_tailroom_bits"))
        rc = bitvec_tailroom_bits(&bv);
    else if (!strcmp(api, "bit_value_to_char"))
        rc = bit_value_to_char((enum bit_value)value);
    else
        __builtin_trap();

    if (case_index)
        puts(",");
    printf("{\"case\":%u,\"api\":\"%s\",", case_index++, api);
    printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
    printf("\"source_sha256\":\"789ef706bc0c71f497400f831e920c9efad324dfaea99b13292fca717b4f480e\",");
    printf("\"header_sha256\":\"db895f6eb8bfada1d6764248d122128b0a0bb530ec5f4a198953f56f223215b6\",");
    printf("\"test_sha256\":\"5b4a1a3f8c5bafd4113f7b4634f3c80061c274356e05c328d270fb40dfb007f5\",");
    printf("\"c_unsigned_bits\":32,\"data_len\":%u,\"pattern\":%u,", len, pattern);
    printf("\"cursor_before\":%u,\"position\":%u,\"value\":%u,\"count\":%u,", cursor, pos, value, count);
    printf("\"before_hex\":\""); hex(before);
    printf("\",\"return_code\":%lld,\"cursor_after\":%u,\"after_hex\":\"", (long long)rc, bv.cur_bit);
    hex(data); printf("\"}");
}

int main(void)
{
    puts("[");
    for (unsigned int len = 0; len <= 3; len++) {
        unsigned int capacity = len * 8;
        for (unsigned int pattern = 0; pattern < 3; pattern++) {
            unsigned int cursors[] = { 0, capacity, UINT_MAX };
            for (unsigned int ci = 0; ci < 3; ci++) {
                for (unsigned int pi = 0; pi <= capacity + 2; pi++) {
                    unsigned int pos = pi == capacity + 2 ? UINT_MAX : pi;
                    observe("bitvec_get_bit_pos", len, pattern, cursors[ci], pos, 0, 0);
                    observe("bitvec_get_bit_pos_high", len, pattern, cursors[ci], pos, 0, 0);
                    for (unsigned int value = 0; value < 4; value++)
                        observe("bitvec_set_bit_pos", len, pattern, cursors[ci], pos, value, 0);
                }
            }
            for (unsigned int ci = 0; ci <= capacity + 2; ci++) {
                unsigned int cursor = ci == capacity + 2 ? UINT_MAX : ci;
                for (unsigned int value = 0; value < 4; value++)
                    observe("bitvec_set_bit", len, pattern, cursor, 0, value, 0);
                observe("bitvec_get_bit_high", len, pattern, cursor, 0, 0, 0);
                for (unsigned int count = 0; count <= 6; count++)
                    observe("bitvec_set_bits", len, pattern, cursor, 0, 0, count);
                observe("bitvec_zero", len, pattern, cursor, 0, 0, 0);
                observe("bitvec_used_bytes", len, pattern, cursor, 0, 0, 0);
                observe("bitvec_tailroom_bits", len, pattern, cursor, 0, 0, 0);
            }
        }
    }
    for (unsigned int value = 0; value < 4; value++)
        observe("bit_value_to_char", 0, 0, 0, 0, value, 0);
    puts("\n]");
    return 0;
}
