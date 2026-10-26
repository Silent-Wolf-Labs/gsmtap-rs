/*
 * Reference harness for libosmocore's osmo_hexparse() buffer contract.
 * Source files match upstream revision 950430e829a3dc1d162aa241bc0505745c5a7311.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <osmocom/core/utils.h>

#define SOURCE(name, text, capacity) { name, text, sizeof(text), capacity }

struct vector_case {
	const char *name;
	const char *src;
	size_t src_len;
	unsigned int dst_len;
};

static const struct vector_case cases[] = {
	SOURCE("empty", "", 4),
	SOURCE("empty_zero_capacity", "", 0),
	SOURCE("whitespace_only", " \t\n\r ", 4),
	SOURCE("zero_capacity_whitespace", " \t\n\r", 0),
	SOURCE("zero_capacity_digit", "0", 0),
	SOURCE("lowercase", "deadbeef", 4),
	SOURCE("uppercase", "DEADBEEF", 4),
	SOURCE("mixed_case", "DeAdBeEf", 4),
	SOURCE("whitespace_between_digits", "d e\ta\nD\rb e e f", 4),
	SOURCE("whitespace_after_full_buffer", "aa \t\n\r", 1),
	SOURCE("exact_fit", "01ab", 2),
	SOURCE("oversized_destination", "01ab", 6),
	SOURCE("too_small_destination", "01abff", 2),
	SOURCE("odd_single_digit", "a", 2),
	SOURCE("odd_after_complete_byte", "ab c", 3),
	SOURCE("invalid_first_character", "xabc", 4),
	SOURCE("invalid_after_complete_byte", "abx0", 4),
	SOURCE("invalid_after_high_nibble", "ab cX", 4),
	SOURCE("invalid_after_capacity", "abx", 1),
	SOURCE("embedded_nul", "ab\0cd", 4),
	SOURCE("high_bit_character", "ab\xff", 4),
};

static void print_hex(const uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; i++)
		printf("%02x", bytes[i]);
}

static void run_case(const struct vector_case *vector, int trailing_comma)
{
	uint8_t dst[16];
	uint8_t before[16];
	int rc;

	for (size_t i = 0; i < sizeof(dst); i++)
		dst[i] = (uint8_t)(0xa5 ^ i);
	memcpy(before, dst, sizeof(dst));
	rc = osmo_hexparse(vector->src, dst, vector->dst_len);

	printf("{\"case\":\"%s\",", vector->name);
	printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
	printf("\"src_hex\":\"");
	print_hex((const uint8_t *)vector->src, vector->src_len);
	printf("\",\"dst_len\":%u,\"dst_before_hex\":\"", vector->dst_len);
	print_hex(before, vector->dst_len);
	printf("\",\"return_code\":%d,\"dst_after_hex\":\"", rc);
	print_hex(dst, vector->dst_len);
	printf("\"}%s\n", trailing_comma ? "," : "");
}

int main(int argc, char **argv)
{
	if (argc == 1) {
		puts("[");
		for (size_t i = 0; i < ARRAY_SIZE(cases); i++)
			run_case(&cases[i], i + 1 < ARRAY_SIZE(cases));
		puts("]");
		return 0;
	}
	if (argc == 3 && strcmp(argv[1], "--case") == 0) {
		for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
			if (strcmp(argv[2], cases[i].name) == 0) {
				run_case(&cases[i], 0);
				return 0;
			}
		}
	}
	fputs("usage: hexparse_vector [--case NAME]\n", stderr);
	return 2;
}
