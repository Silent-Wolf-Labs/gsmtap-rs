/*
 * Reference harness for libosmocoding's GSM 05.03 xCCH coding.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one
 * record per call. The first argument is upstream's coding_test.ok; its xCCH
 * frames are replayed and the run fails if a result differs from the file.
 *
 * Building with -DXCCH_BACKEND=gen, sse or sse_avx replaces the library's decoder
 * with the unchanged pinned source, forced to that backend. The runner uses this
 * only to compare backends. Fixtures come from the linked library.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/coding/gsm0503_coding.h>
#include <osmocom/coding/gsm0503_interleaving.h>
#include <osmocom/coding/gsm0503_mapping.h>
#include <osmocom/coding/gsm0503_parity.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/conv.h>
#include <osmocom/core/crc64gen.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm0503.h>

#ifdef XCCH_BACKEND
#include "conv_acc.c"
#define FORCE_(simd) INIT_POINTERS(simd)
#define FORCE(simd) FORCE_(simd)
#endif

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define L2_LEN 23
#define BURST_BITS 116
#define BURSTS_LEN (4 * BURST_BITS)
#define CODED_LEN 456
#define GUARD 4
#define FILL 0xa5
#define MAX_UPSTREAM 8
#define SEEDS 3

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

static void code_record(void)
{
	const struct osmo_conv_code *code = &gsm0503_xcch;
	unsigned int states = 1u << (code->K - 1), i;

	printf("{\"kind\":\"code\",\"n\":%d,\"k\":%d,\"len\":%d,\"term\":%d,\"next_output\":\"",
	       code->N, code->K, code->len, (int)code->term);
	for (i = 0; i < states; i++)
		printf("%s%u %u", i ? " " : "", code->next_output[i][0], code->next_output[i][1]);
	printf("\",\"next_state\":\"");
	for (i = 0; i < states; i++)
		printf("%s%u %u", i ? " " : "", code->next_state[i][0], code->next_state[i][1]);
	printf("\"}\n");
	printf("{\"kind\":\"crc\",\"bits\":%d,\"poly\":%" PRIu64 ",\"init\":%" PRIu64
	       ",\"remainder\":%" PRIu64 "}\n",
	       gsm0503_fire_crc40.bits, gsm0503_fire_crc40.poly, gsm0503_fire_crc40.init,
	       gsm0503_fire_crc40.remainder);
}

static void interleave_records(void)
{
	/* The low and high bytes of each position together give the whole permutation. */
	ubit_t in[CODED_LEN], out[CODED_LEN + GUARD];
	unsigned int pass, i;

	for (pass = 0; pass < 3; pass++) {
		const char *name = pass == 0 ? "position_low" : pass == 1 ? "position_high" : "random";

		for (i = 0; i < CODED_LEN; i++)
			in[i] = pass == 0 ? i & 0xff : pass == 1 ? i >> 8 : next_random() & 0xff;

		memset(out, FILL, sizeof(out));
		gsm0503_xcch_interleave(in, out);
		printf("{\"kind\":\"interleave\",\"case\":\"%s\"", name);
		hex("in", in, CODED_LEN);
		hex("out", out, sizeof(out));
		printf("}\n");

		memset(out, FILL, sizeof(out));
		gsm0503_xcch_deinterleave((sbit_t *)out, (const sbit_t *)in);
		printf("{\"kind\":\"deinterleave\",\"case\":\"%s\"", name);
		hex("in", in, CODED_LEN);
		hex("out", out, sizeof(out));
		printf("}\n");
	}
}

static void burst_records(void)
{
	static const int flags[][2] = { { -1, -1 }, { 0, -1 }, { -1, 1 }, { 0, 1 }, { 1, 0 } };
	ubit_t in[BURST_BITS - 2], out[BURST_BITS + GUARD], soft_in[BURST_BITS];
	sbit_t soft_out[BURST_BITS - 2 + GUARD], hl, hn;
	size_t i;
	unsigned int j;

	for (i = 0; i < ARRAY_SIZE(flags); i++) {
		ubit_t hl_value = flags[i][0], hn_value = flags[i][1];

		for (j = 0; j < sizeof(in); j++)
			in[j] = next_random() & 1;
		memset(out, FILL, sizeof(out));
		gsm0503_xcch_burst_map(in, out, flags[i][0] < 0 ? NULL : &hl_value,
				       flags[i][1] < 0 ? NULL : &hn_value);
		printf("{\"kind\":\"map\",\"hl\":%d,\"hn\":%d", flags[i][0], flags[i][1]);
		hex("in", in, sizeof(in));
		hex("out", out, sizeof(out));
		printf("}\n");
	}
	for (i = 0; i < 4; i++) {
		for (j = 0; j < sizeof(soft_in); j++)
			soft_in[j] = next_random() & 0xff;
		memset(soft_out, FILL, sizeof(soft_out));
		gsm0503_xcch_burst_unmap(soft_out, (const sbit_t *)soft_in, &hl, &hn);
		printf("{\"kind\":\"unmap\",\"hl\":%d,\"hn\":%d", hl, hn);
		hex("in", soft_in, sizeof(soft_in));
		hex("out", soft_out, sizeof(soft_out));
		printf("}\n");
	}
}

static void encode_frame(const char *name, const uint8_t *l2, ubit_t *bursts)
{
	ubit_t recorded[BURSTS_LEN + GUARD];

	memset(recorded, FILL, sizeof(recorded));
	if (gsm0503_xcch_encode(recorded, l2) != 0) {
		fprintf(stderr, "encoding failed\n");
		exit(EXIT_FAILURE);
	}
	printf("{\"kind\":\"encode\",\"case\":\"%s\"", name);
	hex("l2", l2, L2_LEN);
	hex("bursts", recorded, sizeof(recorded));
	printf("}\n");
	memcpy(bursts, recorded, BURSTS_LEN);
}

static void decode_record(const char *name, const sbit_t *soft)
{
	uint8_t l2[L2_LEN + GUARD];
	int n_errors = -1, n_bits_total = -1, rc;

	memset(l2, FILL, sizeof(l2));
	rc = gsm0503_xcch_decode(l2, soft, &n_errors, &n_bits_total);
	printf("{\"kind\":\"decode\",\"case\":\"%s\"", name);
	hex("input", soft, BURSTS_LEN);
	printf(",\"rc\":%d,\"n_errors\":%d,\"n_bits_total\":%d", rc, n_errors, n_bits_total);
	hex("l2", l2, sizeof(l2));
	printf("}\n");
}

static void soften(sbit_t *soft, const ubit_t *bursts, int level)
{
	unsigned int i;

	for (i = 0; i < BURSTS_LEN; i++)
		soft[i] = bursts[i] ? -level : level;
}

static void parse_l2(const char *text, uint8_t *l2)
{
	if (osmo_hexparse(text, l2, L2_LEN) != L2_LEN) {
		fprintf(stderr, "bad upstream L2 data: %s\n", text);
		exit(EXIT_FAILURE);
	}
}

/* Replays the frames in upstream's output and returns how many there were. */
static size_t upstream_frames(const char *path, uint8_t frames[][L2_LEN])
{
	char line[8192];
	FILE *file = fopen(path, "r");
	uint8_t l2[L2_LEN] = { 0 }, expected[L2_LEN] = { 0 };
	ubit_t expected_bits[BURSTS_LEN] = { 0 };
	size_t count = 0;

	if (!file) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	while (fgets(line, sizeof(line), file)) {
		int n_errors, n_bits_total;
		float ber;

		line[strcspn(line, "\n")] = '\0';
		if (!strncmp(line, "Encoding: ", 10) && strlen(line) == 10 + 3 * L2_LEN) {
			parse_l2(line + 10, l2);
		} else if (!strcmp(line, "U-Bits:")) {
			unsigned int row, bit;

			for (row = 0; row < 4; row++) {
				char head[58], tail[58], hl[3], hn[3];

				if (!fgets(line, sizeof(line), file) ||
				    sscanf(line, "%57s %2s %2s %57s", head, hl, hn, tail) != 4)
					break;
				for (bit = 0; bit < 57 && head[bit] && tail[bit]; bit++) {
					expected_bits[row * BURST_BITS + bit] = head[bit] - '0';
					expected_bits[row * BURST_BITS + 59 + bit] = tail[bit] - '0';
				}
				expected_bits[row * BURST_BITS + 57] = atoi(hl);
				expected_bits[row * BURST_BITS + 58] = atoi(hn);
			}
		} else if (!strncmp(line, "Decoded: ", 9) && strlen(line) == 9 + 3 * L2_LEN) {
			parse_l2(line + 9, expected);
		} else if (sscanf(line, "xcch_decode: n_errors=%d n_bits_total=%d ber=%f", &n_errors,
				  &n_bits_total, &ber) == 3) {
			ubit_t bursts[BURSTS_LEN];
			sbit_t soft[BURSTS_LEN];
			uint8_t decoded[L2_LEN];
			int actual_errors, actual_total, rc;

			encode_frame("upstream", l2, bursts);
			osmo_ubit2sbit(soft, bursts, BURSTS_LEN);
			memset(soft, 0, 30);
			memset(soft + BURST_BITS, 0, 30);
			decode_record("upstream", soft);
			rc = gsm0503_xcch_decode(decoded, soft, &actual_errors, &actual_total);
			if (rc || actual_errors != n_errors || actual_total != n_bits_total ||
			    memcmp(decoded, expected, L2_LEN) || memcmp(l2, expected, L2_LEN) ||
			    memcmp(bursts, expected_bits, BURSTS_LEN) || count == MAX_UPSTREAM) {
				fprintf(stderr, "differs from upstream output, frame %zu\n", count);
				exit(EXIT_FAILURE);
			}
			memcpy(frames[count++], l2, L2_LEN);
		}
	}
	fclose(file);
	if (!count) {
		fprintf(stderr, "no upstream xCCH frames in %s\n", path);
		exit(EXIT_FAILURE);
	}
	return count;
}

static void walking_bits(void)
{
	ubit_t bursts[BURSTS_LEN];
	unsigned int bit;

	for (bit = 0; bit < 8 * L2_LEN; bit++) {
		uint8_t l2[L2_LEN] = { 0 };

		l2[bit / 8] = 1u << (bit % 8);
		encode_frame("walking_bit", l2, bursts);
	}
}

static void decode_frame(const uint8_t *l2)
{
	static const int levels[] = { 127, 64, 8, 1 };
	static const unsigned int erased[][4] = {
		{ 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 },
		{ 1, 1, 0, 0 }, { 0, 0, 1, 1 }, { 1, 1, 1, 0 },
	};
	static const unsigned int flips[] = { 1, 2, 4, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 96, 120, 160, 232 };
	static const int noise[] = { 8, 32, 64, 96, 128, 192, 255 };
	static const unsigned int erasures[] = { 100, 200, 300, 400 };
	static const int flag_values[] = { -127, 0, 127 };
	ubit_t bursts[BURSTS_LEN];
	sbit_t soft[BURSTS_LEN], clean[BURSTS_LEN];
	char name[32];
	size_t i, j;
	unsigned int run, count, position;

	encode_frame("decoded_frame", l2, bursts);
	soften(clean, bursts, 127);

	for (i = 0; i < ARRAY_SIZE(levels); i++) {
		soften(soft, bursts, levels[i]);
		snprintf(name, sizeof(name), "clean_%d", levels[i]);
		decode_record(name, soft);
	}
	for (i = 0; i < ARRAY_SIZE(erased); i++) {
		memcpy(soft, clean, sizeof(soft));
		for (j = 0; j < 4; j++)
			if (erased[i][j])
				memset(soft + j * BURST_BITS, 0, BURST_BITS);
		snprintf(name, sizeof(name), "erased_bursts_%zu", i);
		decode_record(name, soft);
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
			snprintf(name, sizeof(name), "flips_%u", flips[i]);
			decode_record(name, soft);
		}
	for (i = 0; i < ARRAY_SIZE(noise); i++)
		for (run = 0; run < SEEDS; run++) {
			soften(soft, bursts, 64);
			for (position = 0; position < BURSTS_LEN; position++) {
				int value = soft[position] +
					    (int)(next_random() % (2 * noise[i] + 1)) - noise[i];

				soft[position] = value > 127 ? 127 : value < -127 ? -127 : value;
			}
			snprintf(name, sizeof(name), "noise_%d", noise[i]);
			decode_record(name, soft);
		}
	for (i = 0; i < ARRAY_SIZE(erasures); i++)
		for (run = 0; run < 2; run++) {
			memcpy(soft, clean, sizeof(soft));
			for (count = 0; count < erasures[i]; count++)
				soft[next_random() % BURSTS_LEN] = 0;
			snprintf(name, sizeof(name), "erasures_%u", erasures[i]);
			decode_record(name, soft);
		}
	for (i = 0; i < ARRAY_SIZE(flag_values); i++) {
		memcpy(soft, clean, sizeof(soft));
		for (j = 0; j < 4; j++) {
			soft[j * BURST_BITS + 57] = flag_values[i];
			soft[j * BURST_BITS + 58] = flag_values[i];
		}
		snprintf(name, sizeof(name), "stealing_flags_%zu", i);
		decode_record(name, soft);
	}
}

static void frames_and_decodes(const uint8_t upstream[][L2_LEN], size_t count)
{
	uint8_t l2[L2_LEN];
	size_t i, j;

	for (i = 0; i < count; i++)
		decode_frame(upstream[i]);
	memset(l2, 0, sizeof(l2));
	decode_frame(l2);
	memset(l2, 0xff, sizeof(l2));
	decode_frame(l2);
	for (i = 0; i < 8; i++) {
		for (j = 0; j < L2_LEN; j++)
			l2[j] = next_random() & 0xff;
		decode_frame(l2);
	}
}

static void no_signal(void)
{
	sbit_t soft[BURSTS_LEN];
	unsigned int run, position;

	memset(soft, 0, sizeof(soft));
	decode_record("all_zero", soft);
	memset(soft, 127, sizeof(soft));
	decode_record("all_positive", soft);
	memset(soft, -127, sizeof(soft));
	decode_record("all_negative", soft);
	for (run = 0; run < 20; run++) {
		for (position = 0; position < BURSTS_LEN; position++)
			soft[position] = (int)(next_random() % 255) - 127;
		decode_record("random_soft", soft);
	}
}

int main(int argc, char **argv)
{
	uint8_t upstream[MAX_UPSTREAM][L2_LEN];
	size_t count;

	if (argc != 2) {
		fprintf(stderr, "usage: %s coding_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
#ifdef XCCH_BACKEND
	osmo_conv_init();
	FORCE(XCCH_BACKEND);
#endif
	seed = 0x05030503;

	printf("{\"schema\":\"gsm0503-xcch-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	code_record();
	interleave_records();
	burst_records();
	count = upstream_frames(argv[1], upstream);
	walking_bits();
	frames_and_decodes((const uint8_t (*)[L2_LEN])upstream, count);
	no_signal();
	return EXIT_SUCCESS;
}
