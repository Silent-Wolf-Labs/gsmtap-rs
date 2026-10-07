/*
 * Reference harness for libosmocore's Base64 buffer APIs.
 *
 * Each invocation emits one JSON object used as a committed Rust conformance
 * fixture.  The harness records destination bytes and the caller-owned olen
 * value so successful and failing calls are both observable.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <osmocom/core/base64.h>

enum operation { ENCODE, DECODE };

struct vector_case {
	const char *name;
	enum operation operation;
	const unsigned char *src;
	size_t src_len;
	size_t dst_len;
	int dst_is_null;
};

static const unsigned char empty[] = {};
static const unsigned char one[] = { 'f' };
static const unsigned char two[] = { 'f', 'o' };
static const unsigned char three[] = { 'f', 'o', 'o' };
static const unsigned char multi[] = { 'f', 'o', 'o', 'b', 'a', 'r' };
static const unsigned char upstream[] = {
	0x24, 0x48, 0x6e, 0x56, 0x87, 0x62, 0x5a, 0xbd, 0xbf, 0x17, 0xd9, 0xa2,
	0xc4, 0x17, 0x1a, 0x01, 0x94, 0xed, 0x8f, 0x1e, 0x11, 0xb3, 0xd7, 0x09,
	0x0c, 0xb6, 0xe9, 0x10, 0x6f, 0x22, 0xee, 0x13, 0xca, 0xb3, 0x07, 0x05,
	0x76, 0xc9, 0xfa, 0x31, 0x6c, 0x08, 0x34, 0xff, 0x8d, 0xc2, 0x6c, 0x38,
	0x00, 0x43, 0xe9, 0x54, 0x97, 0xaf, 0x50, 0x4b, 0xd1, 0x41, 0xba, 0x95,
	0x31, 0x5a, 0x0b, 0x97
};
static const unsigned char padded[] = "TWE=";
static const unsigned char unpadded[] = "TWE";
static const unsigned char trailing_space[] = "TWE=  ";
static const unsigned char newline[] = "T\nWE=";
static const unsigned char crlf[] = "T\r\nWE=";
static const unsigned char inside_space[] = "TW E=";
static const unsigned char invalid[] = "TW$=";
static const unsigned char misplaced_padding[] = "T=WE";
static const unsigned char excessive_padding[] = "T===";
static const unsigned char high_bit[] = { 'T', 'W', 0xff, '=' };
static const unsigned char only_spaces[] = "   ";
static const unsigned char only_newlines[] = "\r\n\n";

static const struct vector_case cases[] = {
	{ "encode_empty", ENCODE, empty, 0, 1, 0 },
	{ "encode_one_byte_padding", ENCODE, one, sizeof(one), 5, 0 },
	{ "encode_two_byte_padding", ENCODE, two, sizeof(two), 5, 0 },
	{ "encode_three_bytes", ENCODE, three, sizeof(three), 5, 0 },
	{ "encode_multi_block", ENCODE, multi, sizeof(multi), 9, 0 },
	{ "encode_upstream_test_vector", ENCODE, upstream, sizeof(upstream), 89, 0 },
	{ "encode_size_probe", ENCODE, three, sizeof(three), 0, 0 },
	{ "encode_too_small", ENCODE, three, sizeof(three), 4, 0 },
	{ "decode_padded", DECODE, padded, sizeof(padded) - 1, 2, 0 },
	{ "decode_unpadded", DECODE, unpadded, sizeof(unpadded) - 1, 3, 0 },
	{ "decode_trailing_spaces", DECODE, trailing_space, sizeof(trailing_space) - 1, 2, 0 },
	{ "decode_newline", DECODE, newline, sizeof(newline) - 1, 2, 0 },
	{ "decode_crlf", DECODE, crlf, sizeof(crlf) - 1, 2, 0 },
	{ "decode_space_inside_quartet", DECODE, inside_space, sizeof(inside_space) - 1, 2, 0 },
	{ "decode_invalid_character", DECODE, invalid, sizeof(invalid) - 1, 2, 0 },
	{ "decode_misplaced_padding", DECODE, misplaced_padding, sizeof(misplaced_padding) - 1, 2, 0 },
	{ "decode_excessive_padding", DECODE, excessive_padding, sizeof(excessive_padding) - 1, 2, 0 },
	{ "decode_high_bit_character", DECODE, high_bit, sizeof(high_bit), 2, 0 },
	{ "decode_null_size_probe", DECODE, padded, sizeof(padded) - 1, 0, 1 },
	{ "decode_zero_size_probe", DECODE, padded, sizeof(padded) - 1, 0, 0 },
	{ "decode_too_small", DECODE, padded, sizeof(padded) - 1, 1, 0 },
	{ "decode_empty", DECODE, empty, 0, 2, 0 },
	{ "decode_spaces_only", DECODE, only_spaces, sizeof(only_spaces) - 1, 2, 0 },
	{ "decode_newlines_only", DECODE, only_newlines, sizeof(only_newlines) - 1, 2, 0 },
};

static void print_hex(const unsigned char *data, size_t length)
{
	size_t i;
	for (i = 0; i < length; i++)
		printf("%02x", data[i]);
}

static const struct vector_case *find_case(const char *name)
{
	size_t i;
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		if (strcmp(cases[i].name, name) == 0)
			return &cases[i];
	return NULL;
}

int main(int argc, char **argv)
{
	const struct vector_case *vector;
	unsigned char dst[128];
	size_t olen = 0xdecafbad;
	int rc;

	if (argc == 2 && strcmp(argv[1], "--list") == 0) {
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
			puts(cases[i].name);
		return 0;
	}

	if (argc != 3 || strcmp(argv[1], "--case") != 0)
		return 2;
	vector = find_case(argv[2]);
	if (vector == NULL)
		return 2;

	memset(dst, 0xa5, sizeof(dst));
	if (vector->operation == ENCODE)
		rc = osmo_base64_encode(dst, vector->dst_len, &olen, vector->src, vector->src_len);
	else
		rc = osmo_base64_decode(vector->dst_is_null ? NULL : dst, vector->dst_len,
			&olen, vector->src, vector->src_len);

	printf("{\"case\":\"%s\",\"operation\":\"%s\",", vector->name,
		vector->operation == ENCODE ? "encode" : "decode");
	printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
	printf("\"src_hex\":\""); print_hex(vector->src, vector->src_len);
	printf("\",\"dst_present\":%s,\"dst_len\":%zu,", vector->dst_is_null ? "false" : "true", vector->dst_len);
	printf("\"return_code\":%d,\"olen\":%zu,\"dst_after_hex\":\"", rc, olen);
	if (!vector->dst_is_null)
		print_hex(dst, vector->dst_len);
	puts("\"}");
	return 0;
}
