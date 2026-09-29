/* Direct C observations for the libosmocore BCD conversion family.
 * Reference revision: 950430e829a3dc1d162aa241bc0505745c5a7311.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <osmocom/core/utils.h>

enum api { BCD2CHAR, CHAR2BCD, BCD2STR, STR2BCD };

struct vector_case {
	const char *name;
	enum api api;
	const uint8_t *src;
	size_t src_len;
	size_t dst_len;
	int start;
	int end;
	bool allow_hex;
};

#define DEC(name, src, cap, start, end, hex) \
	{ name, BCD2STR, src, sizeof(src), cap, start, end, hex }
#define ENC(name, src, cap, start, end, hex) \
	{ name, STR2BCD, (const uint8_t *)src, sizeof(src), cap, start, end, hex }

static const uint8_t decimal[] = { 0x1a, 0x32, 0x54, 0x76, 0x98, 0xf0 };
static const uint8_t hexadecimal[] = { 0x1a, 0x32, 0xa4, 0xcb, 0x9d, 0xf0 };
static const uint8_t simple[] = { 0x21, 0xfa };
static const uint8_t single[] = { 0x21 };

static const struct vector_case cases[] = {
	DEC("decode_low_first", simple, 5, 0, 4, true),
	DEC("decode_odd_start", decimal, 12, 1, 11, false),
	DEC("decode_hex_allowed", hexadecimal, 12, 1, 11, true),
	DEC("decode_hex_disallowed", hexadecimal, 12, 1, 11, false),
	DEC("decode_filler_disallowed", decimal, 12, 1, 12, false),
	DEC("decode_exact_fit", decimal, 11, 1, 11, false),
	DEC("decode_oversized", decimal, 16, 1, 11, false),
	DEC("decode_truncated", decimal, 5, 1, 11, false),
	DEC("decode_invalid_after_truncation", simple, 3, 0, 4, false),
	DEC("decode_invalid_before_truncation", simple, 4, 0, 4, false),
	DEC("decode_one_byte", simple, 1, 0, 4, false),
	DEC("decode_zero_capacity", simple, 0, 0, 4, false),
	DEC("decode_empty_range", single, 4, 1, 1, false),
	DEC("decode_inverted_range", single, 4, 2, 1, false),
	DEC("decode_last_nibble", single, 2, 1, 2, false),
	ENC("encode_auto_even", "12", 4, 0, -1, false),
	ENC("encode_auto_odd_filler", "123", 4, 0, -1, false),
	ENC("encode_prefix_auto_filler", "1", 4, 1, -1, false),
	ENC("encode_explicit_filler", "1", 4, 0, 4, false),
	ENC("encode_explicit_odd_end", "123", 4, 0, 3, false),
	ENC("encode_empty_range", "12", 4, 1, 1, false),
	ENC("encode_inverted_range", "12", 4, 2, 1, false),
	ENC("encode_upper_hex", "ABCDEF", 4, 0, -1, true),
	ENC("encode_lower_hex", "abcdef", 4, 0, -1, true),
	ENC("encode_hex_disallowed_first", "A1", 4, 0, -1, false),
	ENC("encode_hex_disallowed_after_write", "1A", 4, 0, -1, false),
	ENC("encode_invalid_first", "x1", 4, 0, -1, true),
	ENC("encode_invalid_after_write", "12x", 4, 0, -1, true),
	ENC("encode_embedded_nul", "12\0x", 4, 0, 4, false),
	ENC("encode_exact_fit", "1234", 2, 0, -1, false),
	ENC("encode_oversized", "1234", 4, 0, -1, false),
	ENC("encode_too_small", "1234", 1, 0, -1, false),
	ENC("encode_zero_capacity", "1", 0, 0, -1, false),
	ENC("encode_last_nibble", "1", 2, 3, 4, false),
	ENC("encode_empty_auto", "", 2, 0, -1, false),
};

static void print_hex(const uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; i++)
		printf("%02x", bytes[i]);
}

static void run_case(const struct vector_case *vector, bool trailing_comma)
{
	uint8_t dst[32];
	uint8_t before[32];
	int rc;
	const char *api = vector->api == BCD2STR ? "osmo_bcd2str" : "osmo_str2bcd";

	for (size_t i = 0; i < sizeof(dst); i++)
		dst[i] = (uint8_t)(0xa5 ^ i);
	memcpy(before, dst, sizeof(dst));
	if (vector->api == BCD2STR)
		rc = osmo_bcd2str((char *)dst, vector->dst_len, vector->src,
			vector->start, vector->end, vector->allow_hex);
	else
		rc = osmo_str2bcd(dst, vector->dst_len, (const char *)vector->src,
			vector->start, vector->end, vector->allow_hex);

	printf("{\"case\":\"%s\",\"api\":\"%s\",", vector->name, api);
	printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
	printf("\"src_hex\":\""); print_hex(vector->src, vector->src_len);
	printf("\",\"start_nibble\":%d,\"end_nibble\":%d,\"allow_hex\":%s,",
		vector->start, vector->end, vector->allow_hex ? "true" : "false");
	printf("\"dst_len\":%zu,\"dst_before_hex\":\"", vector->dst_len);
	print_hex(before, vector->dst_len);
	printf("\",\"return_code\":%d,\"dst_after_hex\":\"", rc);
	print_hex(dst, vector->dst_len);
	printf("\"}%s\n", trailing_comma ? "," : "");
}

static void run_digits(bool trailing_comma)
{
	for (unsigned int i = 0; i < 16; i++) {
		char name[32];
		uint8_t bcd = i;
		uint8_t character = i < 10 ? '0' + i : 'A' + i - 10;
		int result;
		snprintf(name, sizeof(name), "bcd2char_%x", i);
		result = osmo_bcd2char(bcd);
		printf("{\"case\":\"%s\",\"api\":\"osmo_bcd2char\",", name);
		printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
		printf("\"src_hex\":\"%02x\",\"return_code\":%d}%s\n", bcd, result, ",");
		snprintf(name, sizeof(name), "char2bcd_%x", i);
		result = osmo_char2bcd(character);
		printf("{\"case\":\"%s\",\"api\":\"osmo_char2bcd\",", name);
		printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
		printf("\"src_hex\":\"%02x\",\"return_code\":%d}%s\n", character, result, ",");
	}
	for (unsigned int i = 10; i < 16; i++) {
		uint8_t character = 'a' + i - 10;
		printf("{\"case\":\"char2bcd_lower_%x\",\"api\":\"osmo_char2bcd\",", i);
		printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
		printf("\"src_hex\":\"%02x\",\"return_code\":%u},\n", character,
			osmo_char2bcd(character));
	}
	const uint8_t invalid[] = { '!', 0, 0xff };
	const char *names[] = { "char2bcd_invalid", "char2bcd_nul", "char2bcd_high_bit" };
	for (size_t i = 0; i < sizeof(invalid); i++) {
		printf("{\"case\":\"%s\",\"api\":\"osmo_char2bcd\",", names[i]);
		printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
		printf("\"src_hex\":\"%02x\",\"return_code\":%u}%s\n", invalid[i],
			osmo_char2bcd(invalid[i]), i + 1 < sizeof(invalid) || trailing_comma ? "," : "");
	}
}

int main(void)
{
	puts("[");
	run_digits(true);
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		run_case(&cases[i], i + 1 < sizeof(cases) / sizeof(cases[0]));
	puts("]");
	return 0;
}
