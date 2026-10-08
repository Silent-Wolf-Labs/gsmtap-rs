/* Direct C observations for full-buffer copies, hex loading and formatting. */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <osmocom/core/bitvec.h>

_Static_assert(sizeof(unsigned) * CHAR_BIT == 32, "32-bit unsigned ABI");
static unsigned case_index;
static const uint8_t seeds[] = { 0, 0xff, 0x2b, 0x80, 0xfe, 0x7f, 0x55, 0xa5 };
struct hex_case { const char *name; const unsigned char *src; unsigned len; };
#define TEXT(name, src) { name, (const unsigned char *)src, sizeof(src) }
static const struct hex_case texts[] = {
    TEXT("empty", ""), TEXT("whitespace", "\t \n\r"),
    TEXT("lower", "a1b2"), TEXT("upper", "A1B2"), TEXT("mixed", "aB01Cf"),
    TEXT("spaced", "a1 b2\tc3\r\n"), TEXT("odd_first", "a"), TEXT("odd_prefix", "12a"),
    TEXT("invalid_first", "x"), TEXT("invalid_prefix", "12x"), TEXT("invalid_half", "1x"),
    TEXT("embedded_nul", "12\0ffff"), TEXT("high_bit", "\x80"), TEXT("high_after", "12\x80"),
    TEXT("eight_bytes", "0001020304050607"), TEXT("nine_bytes", "000102030405060708"),
    TEXT("sixteen_bytes", "000102030405060708090a0b0c0d0e0f"),
    TEXT("trailing_spaces", "12 \t\n\r"), TEXT("full_with_spaces", "0001020304050607 \t")
};

static void hex(const uint8_t *data, unsigned n)
{
    for (unsigned i = 0; i < n; i++) printf("%02x", data[i]);
}

static void observe(const char *api, unsigned len, unsigned pattern, unsigned cursor,
                    unsigned io_len, const char *input_name, const unsigned char *src, unsigned src_len)
{
    uint8_t seed = seeds[pattern];
    uint8_t data[] = { seed, seed, seed ^ 0xff, 0x2b, 0xa5, 0, 0xff, 0x80 };
    uint8_t before[8], io[80], io_before[80];
    memcpy(before, data, 8);
    for (unsigned i = 0; i < sizeof(io); i++) io[i] = 0xd3 ^ i;
    io[0] = 0x5a; io[79] = 0xa5; memcpy(io_before, io, sizeof(io));
    struct bitvec bv = { .cur_bit = cursor, .data_len = len, .data = data };
    int rc;
    if (!strcmp(api, "bitvec_pack")) rc = bitvec_pack(&bv, io + 1);
    else if (!strcmp(api, "bitvec_unpack")) rc = bitvec_unpack(&bv, io + 1);
    else if (!strcmp(api, "bitvec_unhex")) rc = bitvec_unhex(&bv, (const char *)src);
    else if (!strcmp(api, "bitvec_to_string_r")) { bitvec_to_string_r(&bv, (char *)io + 1); rc = 0; }
    else __builtin_trap();
    if (case_index) puts(",");
    printf("{\"case\":%u,\"api\":\"%s\",", case_index++, api);
    printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
    printf("\"source_sha256\":\"789ef706bc0c71f497400f831e920c9efad324dfaea99b13292fca717b4f480e\",");
    printf("\"header_sha256\":\"db895f6eb8bfada1d6764248d122128b0a0bb530ec5f4a198953f56f223215b6\",");
    printf("\"test_sha256\":\"5b4a1a3f8c5bafd4113f7b4634f3c80061c274356e05c328d270fb40dfb007f5\",");
    printf("\"hex_source_sha256\":\"a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4\",");
    printf("\"hex_header_sha256\":\"3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d\",");
    printf("\"hex_test_sha256\":\"7666972f9755fc95c69b2740beb6292a9ca28d00dc7f4cc1f55b83c9615bc60d\",");
    printf("\"c_unsigned_bits\":32,\"data_len\":%u,\"pattern\":%u,\"cursor_before\":%u,", len, pattern, cursor);
    printf("\"io_len\":%u,\"input\":\"%s\",\"src_hex\":\"", io_len, input_name); hex(src, src_len);
    printf("\",\"before_hex\":\""); hex(before, 8);
    printf("\",\"io_before_hex\":\""); hex(io_before, 80);
    printf("\",\"return_code\":%d,\"cursor_after\":%u,\"after_hex\":\"", rc, bv.cur_bit); hex(data, 8);
    printf("\",\"io_after_hex\":\""); hex(io, 80); printf("\"}");
}

int main(void)
{
    puts("[");
    for (unsigned len = 0; len <= 8; len++)
        for (unsigned pattern = 0; pattern < 3; pattern++) {
            unsigned cursors[] = { 0, len * 8, UINT_MAX };
            for (unsigned ci = 0; ci < 3; ci++)
                for (unsigned extra = 0; extra <= 2; extra += 2) {
                    observe("bitvec_pack", len, pattern, cursors[ci], len + extra, "none", NULL, 0);
                    observe("bitvec_unpack", len, pattern, cursors[ci], len + extra, "none", NULL, 0);
                }
        }
    for (unsigned li = 0; li < 5; li++) {
        unsigned len = (unsigned[]){ 0, 1, 2, 4, 8 }[li];
        for (unsigned pattern = 0; pattern < 3; pattern++) {
            unsigned cursors[] = { 0, 1, 7, len * 8, UINT_MAX };
            for (unsigned ci = 0; ci < 5; ci++)
                for (unsigned si = 0; si < sizeof(texts)/sizeof(*texts); si++)
                    observe("bitvec_unhex", len, pattern, cursors[ci], 0, texts[si].name, texts[si].src, texts[si].len);
        }
    }
    for (unsigned byte = 0; byte < 256; byte++) {
        unsigned char src[] = { byte, 0 };
        char name[16]; snprintf(name, sizeof(name), "byte_%02x", byte);
        observe("bitvec_unhex", 2, 2, 7, 0, name, src, sizeof(src));
    }
    for (unsigned len = 0; len <= 8; len++)
        for (unsigned pattern = 0; pattern < 8; pattern++)
            for (unsigned cursor = 0; cursor <= len * 8; cursor++) {
                unsigned required = cursor + (cursor / 8 + (cursor % 8 ? 1 : 0)) + 1;
                for (unsigned extra = 0; extra <= 3; extra += 3)
                    observe("bitvec_to_string_r", len, pattern, cursor, required + extra, "none", NULL, 0);
            }
    puts("\n]");
    return 0;
}
