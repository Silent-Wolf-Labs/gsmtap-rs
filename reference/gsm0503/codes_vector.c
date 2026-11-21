/*
 * Reference harness for the GSM 05.03 convolutional codes and parity descriptors.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one
 * record per descriptor or call. The first argument is upstream's
 * conv_gsm0503_test.ok; the run fails unless it names the same 45 codes in the
 * same order, with the same input and output lengths.
 *
 * Building with -DCODES_BACKEND=gen, sse or sse_avx replaces the library's
 * correlation decoder with the unchanged pinned source, forced to that backend.
 * The runner uses this only to compare backends. Fixtures come from the linked
 * library.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/coding/gsm0503_parity.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/conv.h>
#include <osmocom/core/crc16gen.h>
#include <osmocom/core/crc64gen.h>
#include <osmocom/core/crc8gen.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm0503.h>

#ifdef CODES_BACKEND
#include "conv_acc.c"
#define FORCE_(simd) INIT_POINTERS(simd)
#define FORCE(simd) FORCE_(simd)
#endif

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 4
#define FILL 0xa5
/* Larger than every code's input and output, including the flush bits. */
#define MAX_BITS 2048
#define CRC_MAX_BITS 64

static uint32_t seed;

static uint32_t next_random(void)
{
	seed = 1664525u * seed + 1013904223u;
	return seed >> 8;
}

#define CODE(name) { #name, &name }
static const struct {
	const char *name;
	const struct osmo_conv_code *code;
} codes[] = {
	CODE(gsm0503_xcch),
	CODE(gsm0503_tch_f24),
	CODE(gsm0503_tch_h24),
	CODE(gsm0503_tch_f48),
	CODE(gsm0503_tch_f96),
	CODE(gsm0503_tch_f144),
	CODE(gsm0503_rach),
	CODE(gsm0503_rach_ext),
	CODE(gsm0503_sch),
	CODE(gsm0503_cs2),
	CODE(gsm0503_cs3),
	CODE(gsm0503_cs2_np),
	CODE(gsm0503_cs3_np),
	CODE(gsm0503_tch_afs_12_2),
	CODE(gsm0503_tch_afs_10_2),
	CODE(gsm0503_tch_afs_7_95),
	CODE(gsm0503_tch_afs_7_4),
	CODE(gsm0503_tch_afs_6_7),
	CODE(gsm0503_tch_afs_5_9),
	CODE(gsm0503_tch_afs_5_15),
	CODE(gsm0503_tch_afs_4_75),
	CODE(gsm0503_tch_fr),
	CODE(gsm0503_tch_hr),
	CODE(gsm0503_tch_ahs_7_95),
	CODE(gsm0503_tch_ahs_7_4),
	CODE(gsm0503_tch_ahs_6_7),
	CODE(gsm0503_tch_ahs_5_9),
	CODE(gsm0503_tch_ahs_5_15),
	CODE(gsm0503_tch_ahs_4_75),
	CODE(gsm0503_tch_axs_sid_update),
	CODE(gsm0503_mcs1_dl_hdr),
	CODE(gsm0503_mcs1_ul_hdr),
	CODE(gsm0503_mcs1),
	CODE(gsm0503_mcs2),
	CODE(gsm0503_mcs3),
	CODE(gsm0503_mcs4),
	CODE(gsm0503_mcs5_dl_hdr),
	CODE(gsm0503_mcs5_ul_hdr),
	CODE(gsm0503_mcs5),
	CODE(gsm0503_mcs6),
	CODE(gsm0503_mcs7_dl_hdr),
	CODE(gsm0503_mcs7_ul_hdr),
	CODE(gsm0503_mcs7),
	CODE(gsm0503_mcs8),
	CODE(gsm0503_mcs9),
};

static void hex(const char *field, const void *data, size_t len)
{
	const uint8_t *bytes = data;
	size_t i;

	printf(",\"%s\":\"", field);
	for (i = 0; i < len; i++)
		printf("%02x", bytes[i]);
	printf("\"");
}

static void pairs(const char *field, const uint8_t (*table)[2], unsigned int states)
{
	unsigned int i;

	printf(",\"%s\":\"", field);
	for (i = 0; i < states; i++)
		printf("%s%u %u", i ? " " : "", table[i][0], table[i][1]);
	printf("\"");
}

static void singles(const char *field, const uint8_t *table, unsigned int states)
{
	unsigned int i;

	if (!table) {
		printf(",\"%s\":null", field);
		return;
	}
	printf(",\"%s\":\"", field);
	for (i = 0; i < states; i++)
		printf("%s%u", i ? " " : "", table[i]);
	printf("\"");
}

static void code_record(const char *name, const struct osmo_conv_code *code)
{
	unsigned int states = 1u << (code->K - 1), i;

	printf("{\"kind\":\"code\",\"code\":\"%s\",\"n\":%d,\"k\":%d,\"len\":%d,\"term\":%d", name,
	       code->N, code->K, code->len, (int)code->term);
	pairs("next_output", code->next_output, states);
	pairs("next_state", code->next_state, states);
	singles("next_term_output", code->next_term_output, states);
	singles("next_term_state", code->next_term_state, states);
	if (code->puncture) {
		printf(",\"puncture\":\"");
		for (i = 0; code->puncture[i] >= 0; i++)
			printf("%s%d", i ? " " : "", code->puncture[i]);
		printf("\"");
	} else {
		printf(",\"puncture\":null");
	}
	printf(",\"input_length\":%d,\"output_length\":%d}\n", osmo_conv_get_input_length(code, 0),
	       osmo_conv_get_output_length(code, 0));
}

/* Returns the number of coded bits, which every decode of this code reads. */
static int encode_record(const char *name, const struct osmo_conv_code *code, const char *which,
			 const ubit_t *input, ubit_t *coded)
{
	ubit_t output[MAX_BITS + GUARD];
	int rc;

	memset(output, FILL, sizeof(output));
	rc = osmo_conv_encode(code, input, output);
	if (rc <= 0 || rc > MAX_BITS || rc != osmo_conv_get_output_length(code, 0)) {
		fprintf(stderr, "%s: unexpected encoded length %d\n", name, rc);
		exit(EXIT_FAILURE);
	}
	printf("{\"kind\":\"encode\",\"code\":\"%s\",\"case\":\"%s\"", name, which);
	hex("in", input, code->len);
	printf(",\"rc\":%d", rc);
	hex("out", output, rc + GUARD);
	printf("}\n");
	if (coded)
		memcpy(coded, output, rc);
	return rc;
}

static void decode_record(const char *name, const struct osmo_conv_code *code, const char *which,
			  const sbit_t *input, int coded_len)
{
	ubit_t output[MAX_BITS + GUARD];
	int rc;

	memset(output, FILL, sizeof(output));
	rc = osmo_conv_decode(code, input, output);
	printf("{\"kind\":\"decode\",\"code\":\"%s\",\"case\":\"%s\"", name, which);
	hex("in", input, coded_len);
	printf(",\"rc\":%d", rc);
	hex("out", output, code->len + GUARD);
	printf("}\n");
}

static void soften(sbit_t *soft, const ubit_t *bits, int len, int level)
{
	int i;

	for (i = 0; i < len; i++)
		soft[i] = bits[i] ? -level : level;
}

static void flip(sbit_t *soft, int len, int count)
{
	uint8_t flipped[MAX_BITS] = { 0 };
	int done = 0, position;

	while (done < count) {
		position = next_random() % len;
		if (flipped[position])
			continue;
		flipped[position] = 1;
		soft[position] = -soft[position];
		done++;
	}
}

static void add_noise(sbit_t *soft, int len, int amplitude)
{
	int i, value;

	for (i = 0; i < len; i++) {
		value = soft[i] + (int)(next_random() % (2 * amplitude + 1)) - amplitude;
		soft[i] = value > 127 ? 127 : value < -127 ? -127 : value;
	}
}

static void code_cases(const char *name, const struct osmo_conv_code *code)
{
	ubit_t input[MAX_BITS], coded[MAX_BITS];
	sbit_t soft[MAX_BITS];
	int len = code->len, coded_len, i, run;
	char which[32];

	code_record(name, code);

	memset(input, 0, len);
	coded_len = encode_record(name, code, "zeros", input, coded);
	soften(soft, coded, coded_len, 127);
	decode_record(name, code, "zeros_clean", soft, coded_len);
	memset(input, 1, len);
	encode_record(name, code, "ones", input, coded);
	soften(soft, coded, coded_len, 127);
	decode_record(name, code, "ones_clean", soft, coded_len);
	for (i = 0; i < len; i++)
		input[i] = i & 1;
	encode_record(name, code, "alternating", input, NULL);
	memset(input, 0, len);
	input[0] = 1;
	encode_record(name, code, "first_bit", input, NULL);
	memset(input, 0, len);
	input[len - 1] = 1;
	encode_record(name, code, "last_bit", input, NULL);
	for (run = 0; run < 3; run++) {
		for (i = 0; i < len; i++)
			input[i] = next_random() & 1;
		encode_record(name, code, "random", input, NULL);
	}

	/* Decodes of one more random frame. */
	for (i = 0; i < len; i++)
		input[i] = next_random() & 1;
	encode_record(name, code, "decoded_frame", input, coded);
	soften(soft, coded, coded_len, 127);
	decode_record(name, code, "clean_127", soft, coded_len);
	soften(soft, coded, coded_len, 1);
	decode_record(name, code, "clean_1", soft, coded_len);
	for (run = 0; run < 4; run++) {
		int count = run < 2 ? coded_len / 50 + 1 : coded_len / 12;

		soften(soft, coded, coded_len, 127);
		flip(soft, coded_len, count);
		snprintf(which, sizeof(which), "flips_%d", count);
		decode_record(name, code, which, soft, coded_len);
	}
	for (run = 0; run < 4; run++) {
		int amplitude = run < 2 ? 64 : 128;

		soften(soft, coded, coded_len, 64);
		add_noise(soft, coded_len, amplitude);
		snprintf(which, sizeof(which), "noise_%d", amplitude);
		decode_record(name, code, which, soft, coded_len);
	}
	soften(soft, coded, coded_len, 127);
	for (i = 0; i < coded_len / 4; i++)
		soft[next_random() % coded_len] = 0;
	decode_record(name, code, "erasures", soft, coded_len);
	soften(soft, coded, coded_len, 127);
	memset(soft, 0, coded_len / 4);
	decode_record(name, code, "erased_prefix", soft, coded_len);

	memset(soft, 0, coded_len);
	decode_record(name, code, "all_zero", soft, coded_len);
	for (run = 0; run < 2; run++) {
		for (i = 0; i < coded_len; i++)
			soft[i] = (int)(next_random() % 255) - 127;
		decode_record(name, code, "random_soft", soft, coded_len);
	}
}

/* Stops at the first failure, like upstream's test. */
static void check_upstream(const char *path)
{
	char line[512], name[128];
	FILE *file = fopen(path, "r");
	size_t index = 0;
	int ret, expected;

	if (!file) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	while (fgets(line, sizeof(line), file)) {
		const struct osmo_conv_code *code;

		if (sscanf(line, "[+] Testing: %127s", name) == 1) {
			if (index == ARRAY_SIZE(codes) || strcmp(name, codes[index].name)) {
				fprintf(stderr, "upstream code %zu is %s\n", index, name);
				exit(EXIT_FAILURE);
			}
			index++;
			continue;
		}
		if (!index)
			continue;
		code = codes[index - 1].code;
		if (sscanf(line, "[.] Input length : ret = %d exp = %d", &ret, &expected) == 2) {
			if (ret != expected || ret != osmo_conv_get_input_length(code, 0)) {
				fprintf(stderr, "%s: input length differs from upstream\n", name);
				exit(EXIT_FAILURE);
			}
		} else if (sscanf(line, "[.] Output length : ret = %d exp = %d", &ret, &expected) == 2) {
			if (ret != expected || ret != osmo_conv_get_output_length(code, 0)) {
				fprintf(stderr, "%s: output length differs from upstream\n", name);
				exit(EXIT_FAILURE);
			}
		}
	}
	fclose(file);
	if (index != ARRAY_SIZE(codes)) {
		fprintf(stderr, "upstream names %zu codes\n", index);
		exit(EXIT_FAILURE);
	}
}

#define CRC_CASES(width, descriptor)                                                          \
	do {                                                                                  \
		const struct osmo_crc##width##gen_code *crc = &descriptor;                     \
		printf("{\"kind\":\"crc\",\"crc\":\"%s\",\"width\":%d,\"bits\":%d,\"poly\":%" PRIu64 \
		       ",\"init\":%" PRIu64 ",\"remainder\":%" PRIu64 "}\n",                    \
		       #descriptor, width, crc->bits, (uint64_t)crc->poly, (uint64_t)crc->init,     \
		       (uint64_t)crc->remainder);                                               \
		for (i = 0; i < ARRAY_SIZE(lengths); i++) {                                     \
			int len = lengths[i].len;                                               \
			uint64_t value;                                                         \
			int good, bad;                                                          \
                                                                                              \
			for (j = 0; j < (unsigned int)len; j++)                                 \
				input[j] = lengths[i].fill < 0 ? (int)(next_random() & 1) : lengths[i].fill; \
			value = osmo_crc##width##gen_compute_bits(crc, input, len);             \
			memset(parity, FILL, sizeof(parity));                                   \
			osmo_crc##width##gen_set_bits(crc, input, len, parity);                 \
			good = osmo_crc##width##gen_check_bits(crc, input, len, parity);        \
			parity[0] ^= 1;                                                         \
			bad = osmo_crc##width##gen_check_bits(crc, input, len, parity);         \
			parity[0] ^= 1;                                                         \
			printf("{\"kind\":\"crc_bits\",\"crc\":\"%s\",\"case\":\"%s\"", #descriptor, \
			       lengths[i].name);                                                \
			hex("in", input, len);                                                  \
			printf(",\"value\":%" PRIu64, value);                                   \
			hex("set", parity, crc->bits + GUARD);                                  \
			printf(",\"check\":%d,\"check_flipped\":%d}\n", good, bad);             \
		}                                                                               \
	} while (0)

static void crc_cases(void)
{
	static const struct {
		const char *name;
		int len;
		int fill;
	} lengths[] = {
		{ "random_1", 1, -1 },	   { "random_9", 9, -1 },   { "random_50", 50, -1 },
		{ "random_184", 184, -1 }, { "random_456", 456, -1 }, { "zeros_184", 184, 0 },
		{ "ones_184", 184, 1 },
	};
	ubit_t input[MAX_BITS], parity[CRC_MAX_BITS + GUARD];
	unsigned int i, j;

	CRC_CASES(64, gsm0503_fire_crc40);
	CRC_CASES(16, gsm0503_cs234_crc16);
	CRC_CASES(8, gsm0503_mcs_crc8_hdr);
	CRC_CASES(16, gsm0503_mcs_crc12);
	CRC_CASES(8, gsm0503_rach_crc6);
	CRC_CASES(16, gsm0503_sch_crc10);
	CRC_CASES(8, gsm0503_tch_fr_crc3);
	CRC_CASES(8, gsm0503_tch_efr_crc8);
	CRC_CASES(8, gsm0503_amr_crc6);
	CRC_CASES(16, gsm0503_amr_crc14);
}

int main(int argc, char **argv)
{
	size_t i;

	if (argc != 2) {
		fprintf(stderr, "usage: %s conv_gsm0503_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
#ifdef CODES_BACKEND
	osmo_conv_init();
	FORCE(CODES_BACKEND);
#endif
	check_upstream(argv[1]);
	seed = 0x05030045;

	printf("{\"schema\":\"gsm0503-codes-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	for (i = 0; i < ARRAY_SIZE(codes); i++)
		code_cases(codes[i].name, codes[i].code);
	crc_cases();
	return EXIT_SUCCESS;
}
