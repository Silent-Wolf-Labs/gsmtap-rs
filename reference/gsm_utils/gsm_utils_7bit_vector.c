/*
 * Reference harness for the 7-bit functions in libosmogsm's gsm_utils: septet
 * encoding and packing, octet lengths, and SMS, USSD and user data header coding.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one
 * record per observation. The first argument is upstream's sms_test.ok. The checks
 * upstream's sms_test.c makes on these functions are replayed, with its data below,
 * and the run fails if a result or an output line differs.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm_utils.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0xa5
#define BUFFER_SIZE 4096
#define WRAP_INPUT_SIZE 512

/* Data from upstream's sms_test.c. */
static const char simple_text[] = "test text";
#define simple_septet_length 9
static const uint8_t simple_enc[] = {
	0xf4, 0xf2, 0x9c, 0x0e, 0xa2, 0x97, 0xf1, 0x74
};

static const char escape_text[] = "!$ a more#^- complicated test@@?_%! case";
#define escape_septet_length 41 /* note: the ^ counts as two, because it is a extension character */
static const uint8_t escape_enc[] = {
	0x21, 0x01, 0x28, 0x0c, 0x6a, 0xbf, 0xe5, 0xe5, 0xd1,
	0x86, 0xd2, 0x02, 0x8d, 0xdf, 0x6d, 0x38, 0x3b, 0x3d,
	0x0e, 0xd3, 0xcb, 0x64, 0x10, 0xbd, 0x3c, 0xa7, 0x03,
	0x00, 0xbf, 0x48, 0x29, 0x04, 0x1a, 0x87, 0xe7, 0x65,
};

static const char enhanced_text[] = "enhanced ^ {][} test |+~ ^ test";
#define enhanced_septet_length 39 /* note: the characters { } [ ] ^ | ~ count as two (each of them), because they are extension characters */
static const uint8_t enhanced_enc[] = {
	0x65, 0x37, 0x3A, 0xEC, 0x1E, 0x97, 0xC9, 0xA0, 0x0D,
	0x05, 0xB4, 0x41, 0x6D, 0x7C, 0x1B, 0xDE, 0x26, 0x05,
	0xA2, 0x97, 0xE7, 0x74, 0xD0, 0x06, 0xB8, 0xDA, 0xF4,
	0x40, 0x1B, 0x0A, 0x88, 0x5E, 0x9E, 0xD3, 0x01,
};

static const char enhancedV2_text[] = "enhanced ^ {][} test |+~ ^ tests";
#define enhancedV2_septet_length 40 /* note: number of octets are equal to the enhanced_text! */
static const uint8_t enhancedV2_enc[] = {
	0x65, 0x37, 0x3A, 0xEC, 0x1E, 0x97, 0xC9, 0xA0, 0x0D,
	0x05, 0xB4, 0x41, 0x6D, 0x7C, 0x1B, 0xDE, 0x26, 0x05,
	0xA2, 0x97, 0xE7, 0x74, 0xD0, 0x06, 0xB8, 0xDA, 0xF4,
	0x40, 0x1B, 0x0A, 0x88, 0x5E, 0x9E, 0xD3, 0xE7,
};



static const char concatenated_text[] =
		"this is a testmessage. this is a testmessage. this is a testmessage. this is a testmessage. "
		"this is a testmessage. this is a testmessage. cut here .....: this is a second testmessage. end here.";

static const char splitted_text_part1[] =
		"this is a testmessage. this is a testmessage. this is a testmessage. this is a testmessage. "
		"this is a testmessage. this is a testmessage. cut here .....:";
#define concatenated_part1_septet_length_with_header 160
#define concatenated_part1_septet_length 153
static const uint8_t concatenated_part1_enc[] = {
		0x05, 0x00, 0x03, 0x6f, 0x02, 0x01,
		0xe8, 0xe8, 0xf4, 0x1c, 0x94, 0x9e, 0x83, 0xc2,
		0x20, 0x7a, 0x79, 0x4e, 0x6f, 0x97, 0xe7, 0xf3,
		0xf0, 0xb9, 0xec, 0x02, 0xd1, 0xd1, 0xe9, 0x39,
		0x28, 0x3d, 0x07, 0x85, 0x41, 0xf4, 0xf2, 0x9c,
		0xde, 0x2e, 0xcf, 0xe7, 0xe1, 0x73, 0xd9, 0x05,
		0xa2, 0xa3, 0xd3, 0x73, 0x50, 0x7a, 0x0e, 0x0a,
		0x83, 0xe8, 0xe5, 0x39, 0xbd, 0x5d, 0x9e, 0xcf,
		0xc3, 0xe7, 0xb2, 0x0b, 0x44, 0x47, 0xa7, 0xe7,
		0xa0, 0xf4, 0x1c, 0x14, 0x06, 0xd1, 0xcb, 0x73,
		0x7a, 0xbb, 0x3c, 0x9f, 0x87, 0xcf, 0x65, 0x17,
		0x88, 0x8e, 0x4e, 0xcf, 0x41, 0xe9, 0x39, 0x28,
		0x0c, 0xa2, 0x97, 0xe7, 0xf4, 0x76, 0x79, 0x3e,
		0x0f, 0x9f, 0xcb, 0x2e, 0x10, 0x1d, 0x9d, 0x9e,
		0x83, 0xd2, 0x73, 0x50, 0x18, 0x44, 0x2f, 0xcf,
		0xe9, 0xed, 0xf2, 0x7c, 0x1e, 0x3e, 0x97, 0x5d,
		0xa0, 0x71, 0x9d, 0x0e, 0x42, 0x97, 0xe5, 0x65,
		0x90, 0xcb, 0xe5, 0x72, 0xb9, 0x74,
};

static const char splitted_text_part2[] = " this is a second testmessage. end here.";
#define concatenated_part2_septet_length_with_header 47
#define concatenated_part2_septet_length 40
static const uint8_t concatenated_part2_enc[] = {
		0x05, 0x00, 0x03, 0x6f, 0x02, 0x02,
		0x40, 0x74, 0x74, 0x7a, 0x0e, 0x4a, 0xcf, 0x41,
		0x61, 0xd0, 0xbc, 0x3c, 0x7e, 0xbb, 0xc9, 0x20,
		0x7a, 0x79, 0x4e, 0x6f, 0x97, 0xe7, 0xf3, 0xf0,
		0xb9, 0xec, 0x02, 0x95, 0xdd, 0x64, 0x10, 0xba,
		0x2c, 0x2f, 0xbb, 0x00,
};

static const struct {
	const char *text;
	const uint8_t *encoded;
	int octets;
	int septets;
} upstream_encode[] = {
	{ simple_text, simple_enc, sizeof(simple_enc), simple_septet_length },
	{ escape_text, escape_enc, sizeof(escape_enc), escape_septet_length },
	{ enhanced_text, enhanced_enc, sizeof(enhanced_enc), enhanced_septet_length },
	{ enhancedV2_text, enhancedV2_enc, sizeof(enhancedV2_enc), enhancedV2_septet_length },
};

static const struct {
	const uint8_t *input;
	int input_length;
	const char *text;
	int septets;
	uint8_t hdr;
} upstream_decode[] = {
	{ simple_enc, sizeof(simple_enc), simple_text, simple_septet_length, 0 },
	{ escape_enc, sizeof(escape_enc), escape_text, escape_septet_length, 0 },
	{ enhanced_enc, sizeof(enhanced_enc), enhanced_text, enhanced_septet_length, 0 },
	{ enhancedV2_enc, sizeof(enhancedV2_enc), enhancedV2_text, enhancedV2_septet_length, 0 },
	{ concatenated_part1_enc, sizeof(concatenated_part1_enc), splitted_text_part1,
	  concatenated_part1_septet_length_with_header, 1 },
	{ concatenated_part2_enc, sizeof(concatenated_part2_enc), splitted_text_part2,
	  concatenated_part2_septet_length_with_header, 1 },
};

/* Lines upstream's test prints for these functions, in order. */
static char replay_lines[64][160];
static unsigned int replay_count;

static void fail(const char *what)
{
	fprintf(stderr, "reference expectation failed: %s\n", what);
	exit(EXIT_FAILURE);
}

static uint32_t next_random(uint32_t *state)
{
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

static void fill_random(uint8_t *out, size_t len, uint32_t *state)
{
	for (size_t i = 0; i < len; i++)
		out[i] = (uint8_t)next_random(state);
}

static void print_hex(const char *key, const void *data, size_t len)
{
	const uint8_t *bytes = data;

	printf("\"%s\":\"", key);
	for (size_t i = 0; i < len; i++)
		printf("%02x", bytes[i]);
	printf("\"");
}

static void check_guard(const uint8_t *buffer, size_t from, size_t to, const char *what)
{
	for (size_t i = from; i < to; i++)
		if (buffer[i] != GUARD)
			fail(what);
}

static void replay_line(const char *format, long a, long b, long c, long d, long e, long f)
{
	if (replay_count == ARRAY_SIZE(replay_lines))
		fail("too many replay lines");
	snprintf(replay_lines[replay_count++], sizeof(replay_lines[0]), format, a, b, c, d, e, f);
}

static void octet_lengths(void)
{
	for (unsigned int septets = 0; septets <= 255; septets++)
		printf("{\"kind\":\"octet_len\",\"septets\":%u,\"octets\":%u}\n", septets,
		       gsm_get_octet_len((uint8_t)septets));
}

static void septet_record(const char *text)
{
	static uint8_t result[BUFFER_SIZE];
	int count;

	memset(result, GUARD, sizeof(result));
	count = gsm_septet_encode(result, text);
	check_guard(result, (size_t)count, sizeof(result), "gsm_septet_encode wrote too much");
	printf("{\"kind\":\"septet\",");
	print_hex("text", text, strlen(text));
	printf(",\"count\":%d,", count);
	print_hex("septets", result, (size_t)count);
	printf("}\n");
}

static void septets(void)
{
	/* 0xff isn't recorded: it indexes one past the end of C's table. */
	for (unsigned int byte = 1; byte <= 0xfe; byte++) {
		const char text[2] = { (char)byte, 0 };

		septet_record(text);
	}
	septet_record("");
	septet_record(escape_text);
	septet_record(enhanced_text);
	septet_record("^{}\\[~]|\f");
}

static int pack_record(const char *source, const uint8_t *data, size_t len, uint8_t padding,
		       uint8_t *result, size_t result_size)
{
	int octets;

	memset(result, GUARD, result_size);
	octets = gsm_septet_pack(result, data, len, padding);
	check_guard(result, (size_t)octets, result_size, "gsm_septet_pack wrote too much");
	printf("{\"kind\":\"pack\",\"source\":\"%s\",", source);
	print_hex("septets", data, len);
	printf(",\"padding\":%u,\"count\":%d,", padding, octets);
	print_hex("octets", result, (size_t)octets);
	printf("}\n");
	return octets;
}

static void upstream_pack(void)
{
	static uint8_t septet_data[BUFFER_SIZE], result[BUFFER_SIZE], joined[BUFFER_SIZE];
	const struct {
		const uint8_t *data;
		size_t len;
		const uint8_t *expected;
	} parts[] = {
		{ septet_data, concatenated_part1_septet_length, concatenated_part1_enc },
		{ septet_data + concatenated_part1_septet_length, concatenated_part2_septet_length,
		  concatenated_part2_enc },
	};

	gsm_septet_encode(septet_data, concatenated_text);
	for (size_t i = 0; i < ARRAY_SIZE(parts); i++) {
		int octets = pack_record("upstream", parts[i].data, parts[i].len, 1, result,
					 sizeof(result));
		size_t header = parts[i].expected[0] + 1u;

		memcpy(joined, parts[i].expected, header);
		memcpy(joined + header, result, (size_t)octets);
		if (memcmp(joined, parts[i].expected, (size_t)octets))
			fail("gsm_septet_pack of a concatenated part");
	}
}

static void packs(void)
{
	static uint8_t data[BUFFER_SIZE], result[BUFFER_SIZE];
	uint32_t state = 0x7b17;

	upstream_pack();
	/* Padding above 7 isn't recorded: it makes C shift by a negative amount. */
	for (uint8_t padding = 0; padding <= 7; padding++) {
		for (size_t len = 0; len <= 24; len++) {
			fill_random(data, len, &state);
			pack_record("sweep", data, len, padding, result, sizeof(result));
		}
		fill_random(data, 300, &state);
		pack_record("sweep", data, 300, padding, result, sizeof(result));
	}
}

static void encode_record(const char *kind, const char *source, const char *text, size_t n,
			  int (*encode)(uint8_t *, size_t, const char *, int *))
{
	static uint8_t result[BUFFER_SIZE];
	int octets = -1, count;

	if (n >= sizeof(result))
		fail("output size");
	memset(result, GUARD, sizeof(result));
	count = encode(result, n, text, &octets);
	if (octets < 0 || (size_t)octets > n)
		fail("encoder went past its output size");
	check_guard(result, (size_t)octets, sizeof(result), "encoder wrote too much");
	printf("{\"kind\":\"%s\",\"source\":\"%s\",", kind, source);
	print_hex("text", text, strlen(text));
	printf(",\"n\":%zu,\"count\":%d,\"written\":%d,", n, count, octets);
	print_hex("octets", result, (size_t)octets);
	printf("}\n");
}

static void upstream_encode_checks(void)
{
	uint8_t coded[256];
	int octets, count;

	for (size_t i = 0; i < ARRAY_SIZE(upstream_encode); i++) {
		count = (uint8_t)gsm_7bit_encode_n(coded, sizeof(coded), upstream_encode[i].text,
						   &octets);
		replay_line("Encode case %ld: Octet length %ld (expected %ld, computed %ld), "
			    "septet length %ld (expected %ld)\n",
			    (long)i, octets, upstream_encode[i].octets,
			    gsm_get_octet_len((uint8_t)count), count, upstream_encode[i].septets);
		if (octets != upstream_encode[i].octets || count != upstream_encode[i].septets ||
		    memcmp(coded, upstream_encode[i].encoded, (size_t)octets))
			fail("gsm_7bit_encode_n of an upstream text");
		for (size_t n = 0; n < (size_t)upstream_encode[i].octets + 1u; n++)
			encode_record("encode", "upstream", upstream_encode[i].text, n,
				      gsm_7bit_encode_n);
	}
}

static void upstream_large_message(void)
{
	static const size_t lengths[] = { 2048, 1024, 555, 512, 260, 255, 250 };
	static const uint8_t chunk[] = { 0xc1, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x83 };
	static uint8_t encoded[2048 * 7 / 8];
	static char text[2048 + 1];
	int octets = 0, count;

	memset(text, 'A', sizeof(text) - 1);
	for (size_t i = 0; i < ARRAY_SIZE(lengths); i++) {
		int expected = (int)(lengths[i] * 7 / 8) + (lengths[i] % 8 != 0);

		text[lengths[i]] = 0;
		memset(encoded, 0, sizeof(encoded));
		count = gsm_7bit_encode_n(encoded, sizeof(encoded), text, &octets);
		replay_line("gsm_7bit_encode_n(len=%ld) processed %ld septets (expected %ld): OK\n",
			    (long)lengths[i], count, (long)lengths[i], 0, 0, 0);
		replay_line("gsm_7bit_encode_n(len=%ld) used %ld octets in the buffer (expected %ld): OK\n",
			    (long)lengths[i], octets, expected, 0, 0, 0);
		if (count != (int)lengths[i] || octets != expected)
			fail("gsm_7bit_encode_n of a large message");
		for (int j = 0; j < expected; j += (int)sizeof(chunk)) {
			size_t len = OSMO_MIN((size_t)(expected - j), sizeof(chunk));

			if (lengths[i] % 8 != 0)
				len--;
			if (memcmp(&encoded[j], chunk, len))
				fail("chunk of a large message");
		}
		encode_record("encode", "upstream", text, sizeof(encoded), gsm_7bit_encode_n);
	}
}

static void repeat_text(char *text, char byte, size_t len)
{
	memset(text, byte, len);
	text[len] = 0;
}

static void encodes(void)
{
	static const char *const texts[] = {
		"", "a", "@", "$_", "test1234", "^", "^^^^^^^^^", "aaaaaa^", "aaaaaaa^",
		"\xa3\xa5\xe8\xe9\xf9\xec\xf2\xc7\xd8\xf8\xc5\xe5\xc4\xd6\xd1\xdc\xa7\xbf\xe4\xf6\xf1\xfc\xe0",
		"\x01\x7f\x80\x93\xa7\xfe`",
	};
	int octets;
	uint8_t scratch[256];

	gsm_7bit_encode_n(scratch, sizeof(scratch), "test1234", &octets);
	replay_line("SEPTETS: %ld OCTETS: %ld\n",
		    gsm_7bit_encode_n(scratch, sizeof(scratch), "test1234", &octets), octets, 0, 0, 0, 0);
	upstream_encode_checks();
	upstream_large_message();
	for (size_t i = 0; i < ARRAY_SIZE(texts); i++)
		for (size_t n = 0; n <= strlen(texts[i]) * 2 + 1; n++)
			encode_record("encode", "sweep", texts[i], n, gsm_7bit_encode_n);
}

static void ussd_encodes(void)
{
	static const char *const texts[] = {
		"", "aaaaaaa\r", "aaaaaaaaaaaaaaa\r", "aaaaaaaaaaaaaaaa\r", "aaaaaa^\r", "\r",
		"aaaaaa\r", "\r\r\r\r\r\r\r\r", "a\ra\ra\r\r", simple_text, escape_text,
	};
	char text[32];

	for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
		size_t len = strlen(texts[i]);

		for (size_t n = 0; n <= len + 4; n++)
			encode_record("encode_ussd", "sweep", texts[i], n, gsm_7bit_encode_n_ussd);
	}
	for (size_t len = 1; len <= 24; len++) {
		repeat_text(text, 'a', len);
		for (size_t n = 0; n <= len + 2; n++)
			encode_record("encode_ussd", "sweep", text, n, gsm_7bit_encode_n_ussd);
	}
}

typedef int (*decode_fn)(char *, size_t, const uint8_t *, uint8_t);

static void decode_output(const char *kind, const char *source, const uint8_t *input,
			  size_t input_len, unsigned int length, unsigned int hdr, size_t n,
			  const uint8_t *text, int count)
{
	check_guard(text, n, BUFFER_SIZE, "decoder went past its output size");
	if (count < 0 || (size_t)count >= n || text[count] != 0 ||
	    strlen((const char *)text) != (size_t)count)
		fail("decoder result isn't a terminated string");
	printf("{\"kind\":\"%s\",\"source\":\"%s\",", kind, source);
	print_hex("input", input, input_len);
	printf(",\"length\":%u,", length);
	if (hdr != UINT_MAX)
		printf("\"hdr\":%u,", hdr);
	printf("\"n\":%zu,\"count\":%d,", n, count);
	print_hex("text", text, (size_t)count);
	printf("}\n");
}

static void decode_record(const char *kind, const uint8_t *input, size_t input_len,
			  unsigned int length, size_t n, decode_fn decode)
{
	static uint8_t text[BUFFER_SIZE];
	int count;

	memset(text, GUARD, sizeof(text));
	count = decode((char *)text, n, input, (uint8_t)length);
	decode_output(kind, "sweep", input, input_len, length, UINT_MAX, n, text, count);
}

static int hdr_record(const char *source, const uint8_t *input, size_t input_len,
		      unsigned int length, unsigned int hdr, size_t n, uint8_t *text)
{
	int count;

	memset(text, GUARD, BUFFER_SIZE);
	count = gsm_7bit_decode_n_hdr((char *)text, n, input, (uint8_t)length, (uint8_t)hdr);
	decode_output("decode_hdr", source, input, input_len, length, hdr, n, text, count);
	return count;
}

static void upstream_decode_checks(void)
{
	static uint8_t text[BUFFER_SIZE];

	for (size_t i = 0; i < ARRAY_SIZE(upstream_decode); i++) {
		int count = hdr_record("upstream", upstream_decode[i].input,
				       (size_t)upstream_decode[i].input_length,
				       (unsigned int)upstream_decode[i].septets,
				       upstream_decode[i].hdr, 256, text);

		replay_line("Decode case %ld: return value %ld (expected %ld)\n", (long)i, count,
			    (long)strlen((const char *)text), 0, 0, 0);
		if (strcmp((const char *)text, upstream_decode[i].text))
			fail("gsm_7bit_decode_n_hdr of an upstream message");
		for (size_t n = 1; n < (size_t)upstream_decode[i].septets + 1u; n++)
			hdr_record("upstream", upstream_decode[i].input,
				   (size_t)upstream_decode[i].input_length,
				   (unsigned int)upstream_decode[i].septets,
				   upstream_decode[i].hdr, n, text);
	}
}

/* Packs septets with no padding, for inputs named by septet value. */
static size_t packed(uint8_t *out, const uint8_t *septets, size_t len)
{
	return (size_t)gsm_septet_pack(out, septets, len, 0);
}

static void decodes(void)
{
	static uint8_t input[BUFFER_SIZE];
	uint8_t septets[8];
	uint32_t state = 0x51c3;
	size_t len;

	/* Each septet alone, then after an escape. */
	for (unsigned int c7 = 0; c7 < 128; c7++) {
		septets[0] = (uint8_t)c7;
		len = packed(input, septets, 1);
		decode_record("decode", input, len, 1, 16, gsm_7bit_decode_n);
		septets[0] = 0x1b;
		septets[1] = (uint8_t)c7;
		len = packed(input, septets, 2);
		decode_record("decode", input, len, 2, 16, gsm_7bit_decode_n);
	}
	/* Escapes at the end, and in a row. */
	memcpy(septets, "\x41\x1b\x1b\x1b\x28\x1b", 6);
	for (unsigned int count = 0; count <= 6; count++) {
		len = packed(input, septets, count);
		decode_record("decode", input, len, count, 16, gsm_7bit_decode_n);
	}
	/* Random data at every small length, with output limits. */
	for (unsigned int length = 0; length <= 40; length++) {
		len = gsm_get_octet_len((uint8_t)length);
		fill_random(input, len, &state);
		decode_record("decode", input, len, length, 256, gsm_7bit_decode_n);
		for (size_t n = 1; n <= length + 1u; n += 3)
			decode_record("decode", input, len, length, n, gsm_7bit_decode_n);
	}
	len = gsm_get_octet_len(255);
	fill_random(input, len, &state);
	decode_record("decode", input, len, 255, 256, gsm_7bit_decode_n);
	decode_record("decode", input, len, 255, 300, gsm_7bit_decode_n);
}

static void ussd_decodes(void)
{
	static uint8_t input[BUFFER_SIZE];
	static const char *const texts[] = {
		"", "a", "aaaaaaa", "aaaaaaa\r", "aaaaaaaa", "aaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaa",
		"aaaaaaaaaaaaaaa\r", "\r\r\r\r\r\r\r\r", simple_text, escape_text,
	};
	uint32_t state = 0x2f61;

	/* What the encoder produces, at the septet count it returned and either side. */
	for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
		int octets;
		int count = gsm_7bit_encode_n_ussd(input, 256, texts[i], &octets);

		for (int length = count > 0 ? count - 1 : 0; length <= count + 1; length++) {
			size_t len = OSMO_MAX((size_t)octets, (size_t)gsm_get_octet_len((uint8_t)length));

			if (len > (size_t)octets)
				memset(input + octets, 0, len - (size_t)octets);
			decode_record("decode_ussd", input, len, (unsigned int)length, 256,
				      gsm_7bit_decode_n_ussd);
			decode_record("decode_ussd", input, len, (unsigned int)length, 3,
				      gsm_7bit_decode_n_ussd);
		}
	}
	/* Random data, then again with the last octet holding <CR> in its top bits. */
	for (unsigned int length = 0; length <= 40; length++) {
		size_t len = gsm_get_octet_len((uint8_t)length);

		fill_random(input, len, &state);
		decode_record("decode_ussd", input, len, length, 256, gsm_7bit_decode_n_ussd);
		if (len) {
			input[len - 1] = (uint8_t)('\r' << 1) | (input[len - 1] & 1);
			decode_record("decode_ussd", input, len, length, 256, gsm_7bit_decode_n_ussd);
			decode_record("decode_ussd", input, len, length, 1, gsm_7bit_decode_n_ussd);
		}
	}
}

static void hdr_decodes(void)
{
	static uint8_t input[BUFFER_SIZE], text[BUFFER_SIZE];
	static const uint8_t indicators[] = { 0, 1, 2, 255 };
	uint32_t state = 0x8d05;

	upstream_decode_checks();
	for (size_t i = 0; i < ARRAY_SIZE(indicators); i++) {
		fill_random(input, 60, &state);
		input[0] = 5;
		hdr_record("sweep", input, 60, 40, indicators[i], 256, text);
	}
	/* Each small header length against each septet count around where it ends. */
	for (unsigned int header = 0; header <= 12; header++) {
		unsigned int shift = ((header + 1) * 8 + 6) / 7;

		for (unsigned int length = shift; length <= shift + 9; length++) {
			size_t len = gsm_get_octet_len((uint8_t)length);

			fill_random(input, len, &state);
			input[0] = (uint8_t)header;
			hdr_record("sweep", input, len, length, 1, 256, text);
			hdr_record("sweep", input, len, length, 1, 4, text);
		}
	}
	/*
	 * A header longer than the message makes C's septet count wrap, and it then reads
	 * past the octets the count implies. WRAP_INPUT_SIZE covers its furthest read.
	 */
	fill_random(input, WRAP_INPUT_SIZE, &state);
	static const uint8_t headers[] = { 1, 5, 13, 40, 200, 255 };
	static const uint8_t lengths[] = { 0, 1, 3, 255 };

	for (size_t i = 0; i < ARRAY_SIZE(headers); i++)
		for (size_t j = 0; j < ARRAY_SIZE(lengths); j++) {
			if (((headers[i] + 1u) * 8 + 6) / 7 <= lengths[j])
				continue;
			input[0] = headers[i];
			hdr_record("wrap", input, WRAP_INPUT_SIZE, lengths[j], 1, 600, text);
		}
}

static void replay_upstream_output(const char *ok_path)
{
	FILE *file = fopen(ok_path, "r");
	char line[256];
	unsigned int encode = 0, decode = 0, seen = 0;
	/* Upstream prints its encode lines, decode lines, septet line, then large messages. */
	static char ordered[64][160];
	unsigned int ordered_count = 0;

	if (!file) {
		perror(ok_path);
		exit(EXIT_FAILURE);
	}
	for (unsigned int pass = 0; pass < 4; pass++)
		for (unsigned int i = 0; i < replay_count; i++) {
			const char *candidate = replay_lines[i];
			unsigned int which = !strncmp(candidate, "Encode", 6) ? 0 :
					     !strncmp(candidate, "Decode", 6) ? 1 :
					     !strncmp(candidate, "SEPTETS", 7) ? 2 : 3;

			if (which == pass)
				strcpy(ordered[ordered_count++], candidate);
		}
	while (fgets(line, sizeof(line), file)) {
		if (strncmp(line, "Encode case", 11) && strncmp(line, "Decode case", 11) &&
		    strncmp(line, "SEPTETS:", 8) && strncmp(line, "gsm_7bit_encode_n(", 18))
			continue;
		encode += !strncmp(line, "Encode case", 11);
		decode += !strncmp(line, "Decode case", 11);
		if (seen >= ordered_count || strcmp(line, ordered[seen]))
			fail(line);
		seen++;
	}
	fclose(file);
	if (seen != ordered_count || seen != 25 || encode != 4 || decode != 6)
		fail("expected 25 upstream lines");
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s sms_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}

	printf("{\"schema\":\"gsm-utils-7bit-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	octet_lengths();
	septets();
	packs();
	encodes();
	ussd_encodes();
	decodes();
	ussd_decodes();
	hdr_decodes();
	replay_upstream_output(argv[1]);
	return EXIT_SUCCESS;
}
