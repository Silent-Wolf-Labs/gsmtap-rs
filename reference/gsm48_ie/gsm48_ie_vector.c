/*
 * Reference harness for libosmogsm's 3GPP TS 24.008 BCD numbers: the called, calling and
 * connected party numbers of gsm48_ie.c, encoded by gsm48_encode_bcd_number() and decoded
 * by gsm48_decode_bcd_number2() and the legacy gsm48_decode_bcd_number().
 *
 * Calls the API directly and writes NDJSON: a metadata line, then one section per kind of
 * record. The first argument is upstream's gsm0408_test.ok. Upstream's
 * test_bcd_number_encode_decode() is replayed with the same calls, and the run fails unless
 * its output matches that part of the file byte for byte.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm48_ie.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55
/* Larger than anything a call here may write: 1 + 255 header and digit bytes, or the
 * 301 bytes C writes when the length byte wraps, or 510 digits and a NUL. */
#define BUF_SIZE 1024
/* The output length the harness passes for "large enough". */
#define DEC_LARGE 600

enum section { ENCODE, DECODE, DECODE_LEGACY, SECTION_COUNT };

static const char *const kind_names[SECTION_COUNT] = { "encode", "decode", "decode_legacy" };

static FILE *sections[SECTION_COUNT];
static char *section_text[SECTION_COUNT];
static size_t section_size[SECTION_COUNT];
static const char *source = "sweep";

static void fail(const char *what)
{
	fprintf(stderr, "reference expectation failed: %s\n", what);
	exit(EXIT_FAILURE);
}

#define CHECK(condition) do { if (!(condition)) fail(#condition); } while (0)

static void hex_bytes(FILE *f, const char *key, const void *data, size_t length)
{
	fprintf(f, ",\"%s\":\"", key);
	for (size_t i = 0; i < length; i++)
		fprintf(f, "%02x", ((const unsigned char *)data)[i]);
	fprintf(f, "\"");
}

static void start(enum section section)
{
	fprintf(sections[section], "{\"kind\":\"%s\",\"source\":\"%s\"", kind_names[section], source);
}

static void guard_untouched(const uint8_t *buf, size_t from)
{
	for (size_t i = from; i < BUF_SIZE; i++)
		CHECK(buf[i] == GUARD);
}

static size_t parse_hex(const char *hex, uint8_t *out, size_t out_size)
{
	int len = osmo_hexparse(hex, out, out_size);
	CHECK(len >= 0);
	return len;
}

/* ---- Encoding ---- */

/* The bytes C may write for `input`: the length byte, the header and the digits. */
static size_t encode_extent(size_t input_len, int h_len)
{
	return 1 + h_len + (input_len + 1) / 2;
}

/* Records the length byte, the header bytes C leaves alone, the digits and one byte past
 * them, and checks nothing further changed. */
static void record_encode(const char *input, int h_len, uint8_t max_len)
{
	uint8_t buf[BUF_SIZE];
	size_t extent = encode_extent(strlen(input), h_len);
	int rc;

	CHECK(extent + 1 <= BUF_SIZE);
	memset(buf, GUARD, sizeof(buf));
	rc = gsm48_encode_bcd_number(buf, max_len, h_len, input);
	CHECK(rc <= (int)extent);
	guard_untouched(buf, extent);
	start(ENCODE);
	hex_bytes(sections[ENCODE], "input", input, strlen(input));
	fprintf(sections[ENCODE], ",\"h_len\":%d,\"max_len\":%u,\"rc\":%d", h_len, max_len, rc);
	hex_bytes(sections[ENCODE], "out", buf, extent + 1);
	fprintf(sections[ENCODE], "}\n");
}

static const char *const encode_inputs[] = {
	"",
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "#", "a", "b", "c",
	"A", "B", "C", "d", "e", "f", "F", "+", " ", "-", ".", "x", "\x01", "\x0f", "\x7f", "\x80",
	"\xff",
	"12", "123", "1234", "12345", "123456", "123456789", "0123456789*#abc", "*#", "*123#",
	"#31#0170123456", "123456789012345", "1234567890123456", "12345678901234567890",
	"123456789012345678901", "1x", "x1", "12x", "123x", "1234x5", "+4912345678", "12 34",
	"12A4",
};

/* The length byte C computes before it wraps. */
static int length_byte(size_t input_len, int h_len)
{
	return input_len / 2 + input_len % 2 + h_len;
}

static void sweep_encode_limits(const char *input, int h_len)
{
	int length = length_byte(strlen(input), h_len);
	int limits[] = { 0, length - 1, length, length + 1, 255 };
	bool seen[256] = {};

	for (size_t i = 0; i < ARRAY_SIZE(limits); i++) {
		if (limits[i] < 0 || limits[i] > 255 || seen[limits[i]])
			continue;
		seen[limits[i]] = true;
		record_encode(input, h_len, limits[i]);
	}
}

static void sweep_encode(void)
{
	static const int h_lens[] = { 0, 1, 2, 4 };
	static const size_t long_lens[] = { 506, 507, 508, 509, 510, 511, 512, 513, 600 };
	char digits[700];

	for (size_t i = 0; i < ARRAY_SIZE(encode_inputs); i++)
		for (size_t h = 0; h < ARRAY_SIZE(h_lens); h++)
			sweep_encode_limits(encode_inputs[i], h_lens[h]);

	/* Up to and past a length byte of 255, where it wraps. */
	for (size_t i = 0; i < ARRAY_SIZE(long_lens); i++) {
		for (size_t j = 0; j < long_lens[i]; j++)
			digits[j] = '0' + (j % 10);
		digits[long_lens[i]] = '\0';
		record_encode(digits, 0, 255);
		record_encode(digits, 1, 255);
		record_encode(digits, 0, 254);
	}
}

/* ---- Decoding ---- */

/* How much of an output buffer a decode record holds: up to `output_len`, but no further
 * than two digits for each input byte and a NUL, then one byte more. Nothing past it may
 * change. */
static size_t decode_extent(size_t input_len, size_t output_len)
{
	size_t most = 2 * input_len + 1;

	return (output_len < most ? output_len : most) + 1;
}

/* `gsm48_decode_bcd_number2()` on `input_len` of the `len` bytes, and the legacy decoder
 * on the same bytes when its length byte stays within them. */
static void record_decode(const uint8_t *bytes, size_t len, size_t input_len, size_t h_len,
			  size_t output_len)
{
	uint8_t input[BUF_SIZE];
	char out[BUF_SIZE];
	int rc;

	CHECK(len <= BUF_SIZE && input_len <= len && output_len < BUF_SIZE);
	memset(input, GUARD, sizeof(input));
	memcpy(input, bytes, len);
	memset(out, GUARD, sizeof(out));
	rc = gsm48_decode_bcd_number2(out, output_len, input, input_len, h_len);
	guard_untouched((const uint8_t *)out, decode_extent(input_len, output_len) - 1);
	start(DECODE);
	hex_bytes(sections[DECODE], "input", bytes, len);
	fprintf(sections[DECODE], ",\"input_len\":%zu,\"h_len\":%zu,\"output_len\":%zu,\"rc\":%d",
		input_len, h_len, output_len, rc);
	hex_bytes(sections[DECODE], "out", out, decode_extent(input_len, output_len));
	fprintf(sections[DECODE], "}\n");

	if (input_len != len || len < 1 || (size_t)bytes[0] + 1 > len)
		return;
	memset(out, GUARD, sizeof(out));
	rc = gsm48_decode_bcd_number(out, output_len, input, h_len);
	guard_untouched((const uint8_t *)out, decode_extent(len, output_len) - 1);
	start(DECODE_LEGACY);
	hex_bytes(sections[DECODE_LEGACY], "input", bytes, len);
	fprintf(sections[DECODE_LEGACY], ",\"h_len\":%zu,\"output_len\":%zu,\"rc\":%d", h_len, output_len,
		rc);
	hex_bytes(sections[DECODE_LEGACY], "out", out, decode_extent(len, output_len));
	fprintf(sections[DECODE_LEGACY], "}\n");
}

/* Every output length from 0 to one past the digits and their NUL. */
static void sweep_output_lens(const char *hex, size_t h_len)
{
	uint8_t bytes[BUF_SIZE];
	size_t len = parse_hex(hex, bytes, sizeof(bytes));
	size_t digits = len ? 2 * (len - 1) : 0;

	for (size_t output_len = 0; output_len <= digits + 2; output_len++)
		record_decode(bytes, len, len, h_len, output_len);
	record_decode(bytes, len, len, h_len, DEC_LARGE);
}

static const char *const truncation_inputs[] = {
	"00", "0121", "01f1", "022143", "0221f3", "032143f5", "0321436f", "0321f3ff",
	"0521436587f9", "0a21436587092143658709", "0821436587092143f5", "02ff21", "02f1f2",
	"03f12143", "0321f143", "03ff2143", "02ffff", "01ff", "0100", "03abcdef", "02a0b0",
};

static const char *const header_inputs[] = {
	"07deadbeef214365", "0221f3", "0400112233", "0312345f", "01ff",
};

static void sweep_decode(void)
{
	uint8_t bytes[BUF_SIZE];
	size_t len;

	/* Every byte as the only digit byte, so every nibble in each position. */
	for (unsigned value = 0; value <= 0xff; value++) {
		bytes[0] = 1;
		bytes[1] = value;
		record_decode(bytes, 2, 2, 0, DEC_LARGE);
		record_decode(bytes, 2, 2, 0, 2);
	}

	for (size_t i = 0; i < ARRAY_SIZE(truncation_inputs); i++)
		sweep_output_lens(truncation_inputs[i], 0);

	/* Header lengths up to past the IE. */
	for (size_t i = 0; i < ARRAY_SIZE(header_inputs); i++) {
		len = parse_hex(header_inputs[i], bytes, sizeof(bytes));
		for (size_t h_len = 0; h_len <= len + 1; h_len++)
			record_decode(bytes, len, len, h_len, DEC_LARGE);
		record_decode(bytes, len, len, 255, DEC_LARGE);
		record_decode(bytes, len, len, 1000, DEC_LARGE);
	}
	sweep_output_lens("07deadbeef214365", 4);
	sweep_output_lens("0321f3ff", 1);

	/* Input lengths up to the bytes there are, which is one past the IE. */
	len = parse_hex("0521436587f9aa", bytes, sizeof(bytes));
	for (size_t input_len = 0; input_len <= len; input_len++)
		record_decode(bytes, len, input_len, 0, DEC_LARGE);
	len = parse_hex("ff21436587", bytes, sizeof(bytes));
	for (size_t input_len = 0; input_len <= len; input_len++)
		record_decode(bytes, len, input_len, 0, DEC_LARGE);

	/* The longest IE: 255 bytes of digits. */
	bytes[0] = 0xff;
	for (size_t i = 1; i <= 255; i++)
		bytes[i] = 0x21;
	for (size_t output_len = 509; output_len <= 512; output_len++)
		record_decode(bytes, 256, 256, 0, output_len);
	record_decode(bytes, 256, 256, 0, DEC_LARGE);
	bytes[255] = 0xf1;
	for (size_t output_len = 509; output_len <= 512; output_len++)
		record_decode(bytes, 256, 256, 0, output_len);
	record_decode(bytes, 256, 255, 0, DEC_LARGE);
}

/* ---- Upstream's test, replayed ---- */

static const struct bcd_number_test {
	const char *test_name;
	const char *enc_ascii;
	const char *enc_hex;
	uint8_t enc_h_len;
	int enc_rc;
	const char *dec_hex;
	const char *dec_ascii;
	uint8_t dec_h_len;
	int dec_rc;
	size_t enc_buf_lim;
	size_t dec_buf_lim;
} bcd_number_test_set[] = {
	{
		.test_name = "regular 9-digit MSISDN",
		.enc_ascii = "123456789",
		.enc_hex   = "0521436587f9",
		.enc_rc    = 6,
		.dec_hex   = "0521436587f9",
		.dec_ascii = "123456789",
	},
	{
		.test_name = "regular 6-digit MSISDN with optional header (LHV)",
		.enc_ascii = "123456",
		.enc_hex   = "07ffffffff214365",
		.enc_h_len = 4,
		.enc_rc    = 4 + 4,
		.dec_hex   = "07deadbeef214365",
		.dec_ascii = "123456",
		.dec_h_len = 4,
	},
	{
		.test_name = "long 15-digit (maximum) MSISDN",
		.enc_ascii = "123456789012345",
		.enc_hex   = "0821436587092143f5",
		.enc_rc    = 9,
		.dec_hex   = "0821436587092143f5",
		.dec_ascii = "123456789012345",
	},
	{
		.test_name = "long 15-digit (maximum) MSISDN, limited buffer",
		.enc_ascii = "123456789012345",
		.enc_hex   = "0821436587092143f5",
		.enc_rc    = 9,
		.dec_hex   = "0821436587092143f5",
		.dec_ascii = "123456789012345",
		.dec_buf_lim = 15 + 1,
		.enc_buf_lim = 9,
	},
	{
		.test_name = "to be truncated 20-digit MSISDN",
		.enc_ascii = "12345678901234567890",
		.enc_hex   = "",
		.enc_rc    = -EIO,
		.dec_hex   = "0a21436587092143658709",
		.dec_ascii = "123456789012345",
		.dec_rc    = -ENOSPC,
		.dec_buf_lim = 15 + 1,
		.enc_buf_lim = 9,
	},
	{
		.test_name = "LV incorrect length",
		.dec_hex   = "05214365",
		.dec_ascii = "(none)",
		.dec_rc    = -EINVAL,
	},
	{
		.test_name = "empty input buffer",
		.enc_ascii = "",
		.enc_hex   = "00",
		.enc_rc    = 1,
		.dec_hex   = "",
		.dec_ascii = "(none)",
		.dec_rc = -EIO,
	},
	{
		.test_name = "decoding buffer is one byte too small (OS#4049)",
		.dec_hex   = "022143",
		.dec_ascii = "123",
		.dec_rc    = -ENOSPC,
		.dec_buf_lim = 4,
	},
};

static void replay(FILE *f)
{
	const struct bcd_number_test *test;
	uint8_t buf_enc[0xff] = { 0xff };
	char buf_dec[0xff] = { 0xff };
	size_t buf_len, i;
	int rc;

	fprintf(f, "BSD number encoding / decoding test\n");
	for (i = 0; i < ARRAY_SIZE(bcd_number_test_set); i++) {
		test = &bcd_number_test_set[i];
		fprintf(f, "- Running test: %s\n", test->test_name);

		if (test->enc_ascii) {
			buf_len = test->enc_buf_lim ? test->enc_buf_lim : sizeof(buf_enc);
			fprintf(f, "  - Encoding ASCII (buffer limit=%zu) '%s'...\n", test->enc_buf_lim,
				test->enc_ascii);
			rc = gsm48_encode_bcd_number(buf_enc, buf_len, test->enc_h_len, test->enc_ascii);
			fprintf(f, "    - Expected: (rc=%d) '%s'\n", test->enc_rc, test->enc_hex);
			fprintf(f, "    -   Actual: (rc=%d) '%s'\n", rc,
				osmo_hexdump_nospc(buf_enc, rc >= 0 ? rc : 0));
			record_encode(test->enc_ascii, test->enc_h_len, buf_len);
		}

		if (test->dec_hex) {
			rc = osmo_hexparse(test->dec_hex, buf_enc, sizeof(buf_enc));
			CHECK(rc >= 0);
			buf_len = test->dec_buf_lim ? test->dec_buf_lim : sizeof(buf_dec);
			fprintf(f, "  - Decoding HEX (buffer limit=%zu) '%s'...\n", test->dec_buf_lim,
				test->dec_hex);
			record_decode(buf_enc, rc, rc, test->dec_h_len, buf_len);
			rc = gsm48_decode_bcd_number2(buf_dec, buf_len, buf_enc, rc, test->dec_h_len);
			fprintf(f, "    - Expected: (rc=%d) '%s'\n", test->dec_rc, test->dec_ascii);
			fprintf(f, "    -   Actual: (rc=%d) '%s'\n", rc,
				(rc == 0 || rc == -ENOSPC) ? buf_dec : "(none)");
		}

		memset(buf_enc, 0xff, sizeof(buf_enc));
		memset(buf_dec, 0xff, sizeof(buf_dec));
	}
	fprintf(f, "\n");
}

/* The text of the .ok file from the line that starts with `from` to the line that starts
 * with `to`, not including it. */
static void ok_part(FILE *expected, const char *ok, const char *from, const char *to)
{
	const char *begin = strstr(ok, from);
	const char *end;

	CHECK(begin && (begin == ok || begin[-1] == '\n'));
	end = strstr(begin, to);
	CHECK(end && end[-1] == '\n');
	fwrite(begin, 1, end - begin, expected);
}

static void upstream_replay(const char *ok_path)
{
	FILE *file, *replayed_out, *expected_out;
	char *ok, *replayed, *expected;
	size_t ok_size, replayed_size, expected_size;

	replayed_out = open_memstream(&replayed, &replayed_size);
	CHECK(replayed_out);
	source = "upstream";
	replay(replayed_out);
	source = "sweep";
	fclose(replayed_out);

	file = fopen(ok_path, "r");
	CHECK(file);
	CHECK(fseek(file, 0, SEEK_END) == 0);
	ok_size = ftell(file);
	CHECK(fseek(file, 0, SEEK_SET) == 0);
	ok = calloc(1, ok_size + 1);
	CHECK(ok && fread(ok, 1, ok_size, file) == ok_size);
	fclose(file);

	expected_out = open_memstream(&expected, &expected_size);
	CHECK(expected_out);
	ok_part(expected_out, ok, "BSD number encoding / decoding test", "Constructed RA:");
	fclose(expected_out);
	if (replayed_size != expected_size || memcmp(replayed, expected, expected_size)) {
		fwrite(replayed, 1, replayed_size, stderr);
		fail("replayed output differs from gsm0408_test.ok");
	}
	free(expected);
	free(ok);
	free(replayed);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s gsm0408_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
	for (size_t i = 0; i < SECTION_COUNT; i++) {
		sections[i] = open_memstream(&section_text[i], &section_size[i]);
		CHECK(sections[i]);
	}

	upstream_replay(argv[1]);
	sweep_encode();
	sweep_decode();

	printf("{\"schema\":\"gsm48_ie-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	for (size_t i = 0; i < SECTION_COUNT; i++) {
		fclose(sections[i]);
		fwrite(section_text[i], 1, section_size[i], stdout);
		free(section_text[i]);
	}
	return EXIT_SUCCESS;
}
