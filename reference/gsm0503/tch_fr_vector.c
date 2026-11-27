/*
 * Reference harness for libosmocoding's GSM 05.03 TCH/FS, TCH/EFS and FACCH/F coding.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one
 * record per table or call. The first argument is upstream's coding_test.ok; its
 * test_fr and FACCH/F frames are replayed and the run fails if a result differs
 * from the file.
 *
 * Building with -DTCH_FR_BACKEND=gen, sse or sse_avx replaces the library's decoder
 * with the unchanged pinned source, forced to that backend. The runner uses this
 * only to compare backends. Fixtures come from the linked library.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/codec/codec.h>
#include <osmocom/coding/gsm0503_coding.h>
#include <osmocom/coding/gsm0503_interleaving.h>
#include <osmocom/coding/gsm0503_mapping.h>
#include <osmocom/coding/gsm0503_tables.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/conv.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/protocol/gsm_04_08.h>

#ifdef TCH_FR_BACKEND
#include "conv_acc.c"
#define FORCE_(simd) INIT_POINTERS(simd)
#define FORCE(simd) FORCE_(simd)
#endif

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define BURST_BITS 116
#define BURST_DATA_BITS 114
#define BURSTS_LEN (8 * BURST_BITS)
#define INTERLEAVED_LEN (8 * BURST_DATA_BITS)
#define CODED_LEN 456
#define MAX_FRAME GSM_FR_BYTES
#define GUARD 4
#define FILL 0xa5
#define UPSTREAM_FILL 0x23
#define MAX_UPSTREAM 8
#define SEEDS 2

static uint32_t seed;

static uint32_t next_random(void)
{
	seed = 1664525u * seed + 1013904223u;
	return seed >> 8;
}

static void hex(const char *field, const void *data, size_t len)
{
	const uint8_t *bytes = data;
	size_t i;

	printf(",\"%s\":\"", field);
	for (i = 0; i < len; i++)
		printf("%02x", bytes[i]);
	printf("\"");
}

static void table_record(const char *name, const void *values, size_t count, int wide)
{
	size_t i;

	printf("{\"kind\":\"table\",\"name\":\"%s\",\"values\":\"", name);
	for (i = 0; i < count; i++)
		printf("%s%u", i ? " " : "",
		       wide ? ((const uint16_t *)values)[i] : ((const uint8_t *)values)[i]);
	printf("\"}\n");
}

static void tables(void)
{
	table_record("gsm610_bitorder", gsm610_bitorder, 260, 1);
	table_record("gsm660_bitorder", gsm660_bitorder, 260, 1);
	table_record("gsm0503_gsm_fr_map", gsm0503_gsm_fr_map, ARRAY_SIZE(gsm0503_gsm_fr_map), 0);
	table_record("gsm0503_gsm_efr_protected_bits", gsm0503_gsm_efr_protected_bits,
		     ARRAY_SIZE(gsm0503_gsm_efr_protected_bits), 0);
}

static void interleave_records(void)
{
	/* The low and high bytes of each position together give the whole permutation. */
	uint8_t coded[CODED_LEN], interleaved[INTERLEAVED_LEN];
	uint8_t coded_out[CODED_LEN + GUARD], interleaved_out[INTERLEAVED_LEN + GUARD];
	unsigned int pass, i;

	for (pass = 0; pass < 3; pass++) {
		const char *name = pass == 0 ? "position_low" : pass == 1 ? "position_high" : "random";

		for (i = 0; i < CODED_LEN; i++)
			coded[i] = pass == 0 ? i & 0xff : pass == 1 ? i >> 8 : next_random() & 0xff;
		memset(interleaved_out, FILL, sizeof(interleaved_out));
		gsm0503_tch_fr_interleave(coded, interleaved_out);
		printf("{\"kind\":\"interleave\",\"case\":\"%s\"", name);
		hex("in", coded, sizeof(coded));
		hex("out", interleaved_out, sizeof(interleaved_out));
		printf("}\n");

		for (i = 0; i < INTERLEAVED_LEN; i++)
			interleaved[i] = pass == 0 ? i & 0xff : pass == 1 ? i >> 8 : next_random() & 0xff;
		memset(coded_out, FILL, sizeof(coded_out));
		gsm0503_tch_fr_deinterleave((sbit_t *)coded_out, (const sbit_t *)interleaved);
		printf("{\"kind\":\"deinterleave\",\"case\":\"%s\"", name);
		hex("in", interleaved, sizeof(interleaved));
		hex("out", coded_out, sizeof(coded_out));
		printf("}\n");
	}
}

static void burst_records(void)
{
	static const int flags[] = { -1, 0, 1 };
	ubit_t in[BURST_DATA_BITS], out[BURST_BITS + GUARD];
	uint8_t soft_in[BURST_BITS], soft_out[BURST_DATA_BITS + GUARD];
	int odd;
	size_t i, j;

	for (odd = 0; odd < 2; odd++) {
		for (i = 0; i < ARRAY_SIZE(flags); i++) {
			ubit_t h = flags[i];

			for (j = 0; j < sizeof(in); j++)
				in[j] = next_random() & 1;
			memset(out, FILL, sizeof(out));
			gsm0503_tch_burst_map(in, out, flags[i] < 0 ? NULL : &h, odd);
			printf("{\"kind\":\"map\",\"odd\":%d,\"h\":%d", odd, flags[i]);
			hex("in", in, sizeof(in));
			hex("out", out, sizeof(out));
			printf("}\n");
		}
		for (i = 0; i < 3; i++) {
			sbit_t h = 0x55;

			for (j = 0; j < sizeof(soft_in); j++)
				soft_in[j] = next_random() & 0xff;
			memset(soft_out, FILL, sizeof(soft_out));
			gsm0503_tch_burst_unmap((sbit_t *)soft_out, (const sbit_t *)soft_in, &h, odd);
			printf("{\"kind\":\"unmap\",\"odd\":%d,\"h\":%d", odd, h);
			hex("in", soft_in, sizeof(soft_in));
			hex("out", soft_out, sizeof(soft_out));
			printf("}\n");
		}
	}
}

/* Encodes into bursts that start as FILL and records the call. */
static int encode_record(const char *name, const uint8_t *data, int len, int net_order, ubit_t *bursts)
{
	ubit_t recorded[BURSTS_LEN + GUARD];
	int rc;

	memset(recorded, FILL, sizeof(recorded));
	rc = gsm0503_tch_fr_encode(recorded, data, len, net_order);
	printf("{\"kind\":\"encode\",\"case\":\"%s\",\"net_order\":%d,\"len\":%d", name, net_order, len);
	hex("data", data, len);
	printf(",\"rc\":%d", rc);
	hex("bursts", recorded, sizeof(recorded));
	printf("}\n");
	if (bursts)
		memcpy(bursts, recorded, BURSTS_LEN);
	return rc;
}

static void decode_record(const char *name, const sbit_t *soft, int net_order, int efr)
{
	uint8_t data[MAX_FRAME + GUARD];
	int n_errors = -1, n_bits_total = -1, rc;

	memset(data, FILL, sizeof(data));
	rc = gsm0503_tch_fr_decode(data, soft, net_order, efr, &n_errors, &n_bits_total);
	printf("{\"kind\":\"decode\",\"case\":\"%s\",\"net_order\":%d,\"efr\":%d", name, net_order, efr);
	hex("input", soft, BURSTS_LEN);
	printf(",\"rc\":%d,\"n_errors\":%d,\"n_bits_total\":%d", rc, n_errors, n_bits_total);
	hex("data", data, sizeof(data));
	printf("}\n");
}

static void facch_encode_record(const char *name, const uint8_t *l2, ubit_t *bursts)
{
	ubit_t recorded[BURSTS_LEN + GUARD];
	int rc;

	memset(recorded, FILL, sizeof(recorded));
	rc = gsm0503_tch_fr_facch_encode(recorded, l2);
	printf("{\"kind\":\"facch_encode\",\"case\":\"%s\"", name);
	hex("data", l2, GSM_MACBLOCK_LEN);
	printf(",\"rc\":%d", rc);
	hex("bursts", recorded, sizeof(recorded));
	printf("}\n");
	if (bursts)
		memcpy(bursts, recorded, BURSTS_LEN);
}

static void facch_decode_record(const char *name, const sbit_t *soft)
{
	uint8_t data[GSM_MACBLOCK_LEN + GUARD];
	int n_errors = -1, n_bits_total = -1, rc;

	memset(data, FILL, sizeof(data));
	rc = gsm0503_tch_fr_facch_decode(data, soft, &n_errors, &n_bits_total);
	printf("{\"kind\":\"facch_decode\",\"case\":\"%s\"", name);
	hex("input", soft, BURSTS_LEN);
	printf(",\"rc\":%d,\"n_errors\":%d,\"n_bits_total\":%d", rc, n_errors, n_bits_total);
	hex("data", data, sizeof(data));
	printf("}\n");
}

/*
 * Turns encoded bursts into soft bits. Positions the encoder left as FILL belong to
 * the neighbouring blocks and get random soft bits.
 */
static void soften(sbit_t *soft, const ubit_t *bursts, int level)
{
	unsigned int i;

	for (i = 0; i < BURSTS_LEN; i++) {
		if (bursts[i] == FILL)
			soft[i] = (int)(next_random() % 255) - 127;
		else
			soft[i] = bursts[i] ? -level : level;
	}
}

static int parse_hex(const char *text, uint8_t *data, size_t max)
{
	int len = osmo_hexparse(text, data, max);

	if (len < 0) {
		fprintf(stderr, "bad upstream data: %s\n", text);
		exit(EXIT_FAILURE);
	}
	return len;
}

static void fail_upstream(const char *what, size_t index)
{
	fprintf(stderr, "differs from upstream output: %s %zu\n", what, index);
	exit(EXIT_FAILURE);
}

/*
 * Reads the burst rows printed by coding_test.c's dump_ubits() and returns how many
 * there were. The line after the last row is consumed; it is never one of interest.
 */
static unsigned int parse_rows(FILE *file, ubit_t *bursts)
{
	char line[8192];
	unsigned int row, bit;

	for (row = 0; row < 8; row++) {
		char head[58], tail[58];
		unsigned int hl, hn;

		if (!fgets(line, sizeof(line), file) ||
		    sscanf(line, "%57s %x %x %57s", head, &hl, &hn, tail) != 4 || strlen(head) != 57 ||
		    strlen(tail) != 57)
			break;
		for (bit = 0; bit < 57; bit++) {
			bursts[row * BURST_BITS + bit] = head[bit] == 'E' ? UPSTREAM_FILL : head[bit] - '0';
			bursts[row * BURST_BITS + 59 + bit] = tail[bit] == 'E' ? UPSTREAM_FILL : tail[bit] - '0';
		}
		bursts[row * BURST_BITS + 57] = hl;
		bursts[row * BURST_BITS + 58] = hn;
	}
	return row;
}

struct upstream {
	uint8_t data[MAX_FRAME];
	int len;
};

/* Replays upstream's test_fr() frame and checks every printed value. */
static void replay_fr(const struct upstream *frame, const ubit_t *expected_bits, const uint8_t *expected,
		      int n_errors, int n_bits_total, size_t index)
{
	ubit_t bursts[BURSTS_LEN];
	sbit_t soft[BURSTS_LEN];
	uint8_t decoded[MAX_FRAME];
	int rc, actual_errors, actual_total;

	memset(bursts, UPSTREAM_FILL, sizeof(bursts));
	if (gsm0503_tch_fr_encode(bursts, frame->data, frame->len, 1) != 0 ||
	    memcmp(bursts, expected_bits, BURSTS_LEN))
		fail_upstream("test_fr bursts", index);
	encode_record("upstream", frame->data, frame->len, 1, NULL);

	osmo_ubit2sbit(soft, bursts, BURSTS_LEN);
	memset(soft + 6, 0, 20);
	decode_record("upstream", soft, 1, frame->len == GSM_EFR_BYTES);
	rc = gsm0503_tch_fr_decode(decoded, soft, 1, frame->len == GSM_EFR_BYTES, &actual_errors,
				   &actual_total);
	if (rc != frame->len || actual_errors != n_errors || actual_total != n_bits_total ||
	    memcmp(decoded, expected, frame->len) || memcmp(decoded, frame->data, frame->len))
		fail_upstream("test_fr decode", index);
}

/* Replays upstream's test_facch() for FACCH/F: three blocks, four bursts apart. */
static void replay_facch(const uint8_t *l2, int expected[3][2], uint8_t decoded_expected[3][GSM_MACBLOCK_LEN],
			 size_t index)
{
	ubit_t bursts[3 * 4 * BURST_BITS + 4 * BURST_BITS] = { 0 };
	sbit_t soft[sizeof(bursts)];
	unsigned int i;

	for (i = 0; i < 3; i++)
		if (gsm0503_tch_fr_facch_encode(&bursts[4 * BURST_BITS * i], l2) != 0)
			fail_upstream("test_facch encode", index);
	printf("{\"kind\":\"facch_offsets\"");
	hex("data", l2, GSM_MACBLOCK_LEN);
	hex("bursts", bursts, sizeof(bursts));
	printf("}\n");

	osmo_ubit2sbit(soft, bursts, sizeof(soft));
	for (i = 0; i < 3; i++) {
		uint8_t decoded[GSM_MACBLOCK_LEN];
		int n_errors, n_bits_total;

		facch_decode_record("upstream_offset", &soft[4 * BURST_BITS * i]);
		if (gsm0503_tch_fr_facch_decode(decoded, &soft[4 * BURST_BITS * i], &n_errors, &n_bits_total) !=
			    GSM_MACBLOCK_LEN ||
		    n_errors != expected[i][0] || n_bits_total != expected[i][1] ||
		    memcmp(decoded, decoded_expected[i], GSM_MACBLOCK_LEN) || memcmp(decoded, l2, GSM_MACBLOCK_LEN))
			fail_upstream("test_facch decode", index);
	}
}

static void upstream_frames(const char *path, struct upstream *frames, size_t *frame_count,
			    uint8_t facch[][GSM_MACBLOCK_LEN], size_t *facch_count)
{
	static const char facch_encoding[] = "test_facch(FACCH/F): encoding: ";
	static const char facch_decoded[] = "test_facch(FACCH/F): decoded (BER=";
	char line[8192];
	FILE *file = fopen(path, "r");
	struct upstream frame = { .len = -1 };
	ubit_t expected_bits[BURSTS_LEN];
	uint8_t expected[MAX_FRAME], facch_l2[GSM_MACBLOCK_LEN], facch_decoded_data[3][GSM_MACBLOCK_LEN];
	int facch_expected[3][2], have_bits = 0, facch_decodes = -1;

	if (!file) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	*frame_count = *facch_count = 0;
	while (fgets(line, sizeof(line), file)) {
		int n_errors, n_bits_total, offset;

		line[strcspn(line, "\n")] = '\0';
		if (!strncmp(line, "Encoding: ", 10)) {
			/* Other tests encode longer blocks; only their length matters here. */
			uint8_t any[256];

			frame.len = parse_hex(line + 10, any, sizeof(any));
			if (frame.len <= MAX_FRAME)
				memcpy(frame.data, any, frame.len);
			have_bits = 0;
		} else if (!strcmp(line, "U-Bits:")) {
			/* Only the TCH/F dumps have eight rows. */
			have_bits = parse_rows(file, expected_bits) == 8;
		} else if (!strncmp(line, "Decoded: ", 9) && frame.len >= 0 && frame.len <= MAX_FRAME) {
			parse_hex(line + 9, expected, sizeof(expected));
		} else if (sscanf(line, "tch_fr_decode: n_errors=%d n_bits_total=%d", &n_errors, &n_bits_total) ==
			   2) {
			if (!have_bits || *frame_count == MAX_UPSTREAM)
				fail_upstream("test_fr layout", *frame_count);
			replay_fr(&frame, expected_bits, expected, n_errors, n_bits_total, *frame_count);
			frames[(*frame_count)++] = frame;
		} else if (!strncmp(line, facch_encoding, strlen(facch_encoding))) {
			if (parse_hex(line + strlen(facch_encoding), facch_l2, sizeof(facch_l2)) != GSM_MACBLOCK_LEN)
				fail_upstream("test_facch layout", *facch_count);
			facch_decodes = 0;
		} else if (!strncmp(line, facch_decoded, strlen(facch_decoded)) && facch_decodes >= 0 &&
			   sscanf(line + strlen(facch_decoded), "%d/%d): %n", &n_errors, &n_bits_total, &offset) == 2) {
			if (parse_hex(line + strlen(facch_decoded) + offset, facch_decoded_data[facch_decodes],
				      GSM_MACBLOCK_LEN) != GSM_MACBLOCK_LEN)
				fail_upstream("test_facch layout", *facch_count);
			facch_expected[facch_decodes][0] = n_errors;
			facch_expected[facch_decodes][1] = n_bits_total;
			if (++facch_decodes == 3) {
				if (*facch_count == MAX_UPSTREAM)
					fail_upstream("test_facch layout", *facch_count);
				replay_facch(facch_l2, facch_expected, facch_decoded_data, *facch_count);
				memcpy(facch[(*facch_count)++], facch_l2, GSM_MACBLOCK_LEN);
				facch_decodes = -1;
			}
		}
	}
	fclose(file);
	if (*frame_count != 5 || *facch_count != 3) {
		fprintf(stderr, "expected 5 test_fr and 3 FACCH/F frames in %s\n", path);
		exit(EXIT_FAILURE);
	}
}

static void encode_cases(void)
{
	static const int invalid[] = { 1, 22, 24, 30, 32, 34 };
	uint8_t data[MAX_FRAME];
	unsigned int bit, i, j;
	int net_order;

	/* Every FR bit in both orders, every EFR bit, including the four header bits. */
	for (net_order = 1; net_order >= 0; net_order--)
		for (bit = 0; bit < 8 * GSM_FR_BYTES; bit++) {
			memset(data, 0, sizeof(data));
			data[bit / 8] = 0x80 >> (bit % 8);
			encode_record("walking_bit_fr", data, GSM_FR_BYTES, net_order, NULL);
		}
	for (bit = 0; bit < 8 * GSM_EFR_BYTES; bit++) {
		memset(data, 0, sizeof(data));
		data[bit / 8] = 0x80 >> (bit % 8);
		encode_record("walking_bit_efr", data, GSM_EFR_BYTES, 1, NULL);
	}
	for (i = 0; i < 4; i++) {
		for (j = 0; j < sizeof(data); j++)
			data[j] = next_random() & 0xff;
		encode_record("random_fr", data, GSM_FR_BYTES, 1, NULL);
		encode_record("random_fr", data, GSM_FR_BYTES, 0, NULL);
		encode_record("random_efr", data, GSM_EFR_BYTES, 1, NULL);
		encode_record("random_efr", data, GSM_EFR_BYTES, 0, NULL);
		encode_record("random_facch", data, GSM_MACBLOCK_LEN, 1, NULL);
		facch_encode_record("random", data, NULL);
	}
	encode_record("bfi", data, 0, 1, NULL);
	encode_record("bfi", data, 0, 0, NULL);
	for (i = 0; i < ARRAY_SIZE(invalid); i++)
		encode_record("invalid_length", data, invalid[i], 1, NULL);
}

/* Decodes one block in many conditions, with the given and the other decoder settings. */
static void decode_block(const char *kind, const uint8_t *data, int len, int net_order)
{
	static const int levels[] = { 127, 64, 8, 1 };
	static const unsigned int erased[][8] = {
		{ 1, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 1 }, { 1, 1, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 1, 1, 1, 1 }, { 1, 0, 1, 0, 1, 0, 1, 0 },
	};
	static const unsigned int flips[] = { 1, 4, 16, 32, 48, 64, 96, 160 };
	static const int noise[] = { 32, 96, 192 };
	static const int flag_values[] = { -127, -1, 0, 1, 127 };
	ubit_t bursts[BURSTS_LEN];
	sbit_t soft[BURSTS_LEN], clean[BURSTS_LEN];
	char name[48];
	int efr = len == GSM_EFR_BYTES;
	size_t i, j;
	unsigned int run, count, position;

	snprintf(name, sizeof(name), "%s_source", kind);
	if (encode_record(name, data, len, net_order, bursts) != 0) {
		fprintf(stderr, "encoding failed\n");
		exit(EXIT_FAILURE);
	}
	soften(clean, bursts, 127);

	for (i = 0; i < ARRAY_SIZE(levels); i++) {
		soften(soft, bursts, levels[i]);
		snprintf(name, sizeof(name), "%s_clean_%d", kind, levels[i]);
		decode_record(name, soft, net_order, efr);
	}
	snprintf(name, sizeof(name), "%s_other_settings", kind);
	decode_record(name, clean, !net_order, efr);
	decode_record(name, clean, net_order, !efr);
	decode_record(name, clean, !net_order, !efr);
	for (i = 0; i < ARRAY_SIZE(erased); i++) {
		memcpy(soft, clean, sizeof(soft));
		for (j = 0; j < 8; j++)
			if (erased[i][j])
				memset(soft + j * BURST_BITS, 0, BURST_BITS);
		snprintf(name, sizeof(name), "%s_erased_bursts_%zu", kind, i);
		decode_record(name, soft, net_order, efr);
	}
	for (i = 0; i < ARRAY_SIZE(flips); i++)
		for (run = 0; run < SEEDS; run++) {
			uint8_t flipped[BURSTS_LEN] = { 0 };

			memcpy(soft, clean, sizeof(soft));
			for (count = 0; count < flips[i];) {
				position = next_random() % BURSTS_LEN;
				if (flipped[position])
					continue;
				flipped[position] = 1;
				soft[position] = -soft[position];
				count++;
			}
			snprintf(name, sizeof(name), "%s_flips_%u", kind, flips[i]);
			decode_record(name, soft, net_order, efr);
		}
	for (i = 0; i < ARRAY_SIZE(noise); i++)
		for (run = 0; run < SEEDS; run++) {
			soften(soft, bursts, 64);
			for (position = 0; position < BURSTS_LEN; position++) {
				int value = soft[position] + (int)(next_random() % (2 * noise[i] + 1)) - noise[i];

				soft[position] = value > 127 ? 127 : value < -127 ? -127 : value;
			}
			snprintf(name, sizeof(name), "%s_noise_%d", kind, noise[i]);
			decode_record(name, soft, net_order, efr);
		}
	for (run = 0; run < SEEDS; run++) {
		memcpy(soft, clean, sizeof(soft));
		for (count = 0; count < 300; count++)
			soft[next_random() % BURSTS_LEN] = 0;
		snprintf(name, sizeof(name), "%s_erasures_300", kind);
		decode_record(name, soft, net_order, efr);
	}
	/* The FACCH decision sums hu(B) of the first four and hl(B) of the last four bursts. */
	for (i = 0; i < ARRAY_SIZE(flag_values); i++) {
		memcpy(soft, clean, sizeof(soft));
		for (j = 0; j < 8; j++)
			soft[j * BURST_BITS + (j < 4 ? 58 : 57)] = flag_values[i];
		snprintf(name, sizeof(name), "%s_stealing_flags_%zu", kind, i);
		decode_record(name, soft, net_order, efr);
	}
	memcpy(soft, clean, sizeof(soft));
	for (j = 0; j < 8; j++)
		soft[j * BURST_BITS + (j < 4 ? 57 : 58)] = -127;
	snprintf(name, sizeof(name), "%s_unused_flags", kind);
	decode_record(name, soft, net_order, efr);
}

/* Decodes a FACCH/F block with gsm0503_tch_fr_facch_decode() in many conditions. */
static void facch_decode_block(const char *kind, const uint8_t *l2)
{
	static const unsigned int flips[] = { 1, 16, 48, 96 };
	static const int sums[][8] = {
		{ 0, 0, 0, 0, 0, 0, 0, 0 },	   { -1, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, -1 },	   { 1, 0, 0, 0, 0, 0, 0, 0 },
		{ -127, 127, 0, 0, 0, 0, 0, 0 },   { -127, 126, 0, 0, 0, 0, 0, 0 },
		{ 127, 127, 127, 127, 127, 127, 127, -127 },
	};
	ubit_t bursts[BURSTS_LEN];
	sbit_t soft[BURSTS_LEN], clean[BURSTS_LEN];
	char name[48];
	size_t i, j;
	unsigned int run, count, position;

	snprintf(name, sizeof(name), "%s_source", kind);
	facch_encode_record(name, l2, bursts);
	soften(clean, bursts, 127);
	snprintf(name, sizeof(name), "%s_clean", kind);
	facch_decode_record(name, clean);
	soften(soft, bursts, 1);
	snprintf(name, sizeof(name), "%s_clean_1", kind);
	facch_decode_record(name, soft);
	for (i = 0; i < ARRAY_SIZE(flips); i++)
		for (run = 0; run < SEEDS; run++) {
			memcpy(soft, clean, sizeof(soft));
			for (count = 0; count < flips[i]; count++) {
				position = next_random() % BURSTS_LEN;
				soft[position] = -soft[position];
			}
			snprintf(name, sizeof(name), "%s_flips_%u", kind, flips[i]);
			facch_decode_record(name, soft);
		}
	for (i = 0; i < ARRAY_SIZE(sums); i++) {
		memcpy(soft, clean, sizeof(soft));
		for (j = 0; j < 8; j++)
			soft[j * BURST_BITS + (j < 4 ? 58 : 57)] = sums[i][j];
		snprintf(name, sizeof(name), "%s_stealing_flags_%zu", kind, i);
		facch_decode_record(name, soft);
	}
}

static void decode_cases(const struct upstream *frames, size_t frame_count,
			 const uint8_t facch[][GSM_MACBLOCK_LEN], size_t facch_count)
{
	uint8_t data[MAX_FRAME];
	sbit_t soft[BURSTS_LEN];
	ubit_t bursts[BURSTS_LEN];
	size_t i, j;
	unsigned int run, position;

	for (i = 0; i < frame_count; i++) {
		const char *kind = frames[i].len == GSM_FR_BYTES	  ? "fr"
				   : frames[i].len == GSM_EFR_BYTES ? "efr"
								    : "facch";

		decode_block(kind, frames[i].data, frames[i].len, 1);
		if (frames[i].len == GSM_FR_BYTES)
			decode_block("fr_reversed", frames[i].data, frames[i].len, 0);
	}
	for (i = 0; i < 2; i++) {
		for (j = 0; j < sizeof(data); j++)
			data[j] = next_random() & 0xff;
		decode_block("fr", data, GSM_FR_BYTES, 1);
		decode_block("fr_reversed", data, GSM_FR_BYTES, 0);
		decode_block("efr", data, GSM_EFR_BYTES, 1);
	}
	decode_block("bfi", data, 0, 1);

	for (i = 0; i < facch_count; i++)
		facch_decode_block("upstream", facch[i]);
	for (j = 0; j < sizeof(data); j++)
		data[j] = next_random() & 0xff;
	facch_decode_block("random", data);

	/* Speech decoded as FACCH: no stealing flags set. */
	encode_record("speech_source", frames[0].data, frames[0].len, 1, bursts);
	soften(soft, bursts, 127);
	facch_decode_record("speech", soft);

	memset(soft, 0, sizeof(soft));
	decode_record("all_zero", soft, 1, 0);
	facch_decode_record("all_zero", soft);
	memset(soft, -127, sizeof(soft));
	decode_record("all_negative", soft, 1, 0);
	decode_record("all_negative", soft, 1, 1);
	facch_decode_record("all_negative", soft);
	for (run = 0; run < 8; run++) {
		for (position = 0; position < BURSTS_LEN; position++)
			soft[position] = (int)(next_random() % 255) - 127;
		decode_record("random_soft", soft, run & 1, run >> 2);
		facch_decode_record("random_soft", soft);
	}
}

int main(int argc, char **argv)
{
	struct upstream frames[MAX_UPSTREAM];
	uint8_t facch[MAX_UPSTREAM][GSM_MACBLOCK_LEN];
	size_t frame_count, facch_count;

	if (argc != 2) {
		fprintf(stderr, "usage: %s coding_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
#ifdef TCH_FR_BACKEND
	osmo_conv_init();
	FORCE(TCH_FR_BACKEND);
#endif
	seed = 0x05031f00;

	printf("{\"schema\":\"gsm0503-tch-fr-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	tables();
	interleave_records();
	burst_records();
	upstream_frames(argv[1], frames, &frame_count, facch, &facch_count);
	encode_cases();
	decode_cases(frames, frame_count, (const uint8_t (*)[GSM_MACBLOCK_LEN])facch, facch_count);
	return EXIT_SUCCESS;
}
