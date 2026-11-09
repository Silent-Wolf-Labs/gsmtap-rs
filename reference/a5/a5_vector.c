/*
 * Reference harness for libosmogsm's A5/0 to A5/4 cipher streams.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one
 * record per observation. The first argument is upstream's a5_test.ok, whose A5/0,
 * A5/1 and A5/2 lines are replayed, and the run fails if a result differs.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/bits.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/a5.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define BITS 114
#define GUARD 0x55
#define UPSTREAM_LINES 6

/* The key and frame number of upstream's a5_test.c. */
static const uint8_t upstream_key[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };
static const uint32_t upstream_fn = 123456;

/* Frame number where the shift in osmo_a5_fn_count() overflows an int. */
#define FN_COUNT_LIMIT (1326u << 20)

static const uint32_t frames[] = {
	0, 1, 25, 26, 50, 51, 1325, 1326, 123456, 2715647, 2715648, 16777216, FN_COUNT_LIMIT - 1,
};

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

static void print_bits(const char *key, const ubit_t *bits)
{
	printf("\"%s\":\"", key);
	for (size_t i = 0; i < BITS; i++)
		printf("%u", bits[i]);
	printf("\"");
}

static void fn_counts(void)
{
	uint32_t state = 0x3a5;

	for (size_t i = 0; i < ARRAY_SIZE(frames); i++)
		printf("{\"kind\":\"fn_count\",\"fn\":%u,\"count\":%u}\n", frames[i],
		       osmo_a5_fn_count(frames[i]));
	for (unsigned int i = 0; i < 64; i++) {
		uint32_t frame = next_random(&state) % FN_COUNT_LIMIT;

		printf("{\"kind\":\"fn_count\",\"fn\":%u,\"count\":%u}\n", frame,
		       osmo_a5_fn_count(frame));
	}
}

/* A5/4 takes a 16-byte key, the others 8 bytes. */
static size_t key_len(int n)
{
	return n == 4 ? 16 : 8;
}

static void stream_record(const char *source, int n, const uint8_t *key, uint32_t frame)
{
	ubit_t dl[BITS], ul[BITS], only[BITS];

	if (osmo_a5(n, key, frame, dl, ul))
		fail("osmo_a5 returned an error");
	/* A direction that isn't asked for has no effect on the other. */
	if (osmo_a5(n, key, frame, only, NULL) || memcmp(only, dl, BITS))
		fail("downlink alone differs");
	if (osmo_a5(n, key, frame, NULL, only) || memcmp(only, ul, BITS))
		fail("uplink alone differs");
	printf("{\"kind\":\"stream\",\"source\":\"%s\",\"n\":%d,\"key\":\"", source, n);
	for (size_t i = 0; i < key_len(n); i++)
		printf("%02x", key[i]);
	printf("\",\"fn\":%u,", frame);
	print_bits("dl", dl);
	printf(",");
	print_bits("ul", ul);
	printf("}\n");
}

static void streams(void)
{
	static const uint8_t keys[][16] = {
		{ 0 },
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10 },
		{ 0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe },
	};
	uint32_t state = 0x51a5;
	uint8_t key[16];

	for (int n = 0; n <= 4; n++) {
		/* Upstream's A5/3 and A5/4 vectors take a raw count, so the KASUMI harness
		 * replays them. */
		if (n <= 2)
			stream_record("upstream", n, upstream_key, upstream_fn);
		for (size_t i = 0; i < ARRAY_SIZE(keys); i++)
			for (size_t j = 0; j < ARRAY_SIZE(frames); j++)
				stream_record("sweep", n, keys[i], frames[j]);
		if (n == 0)
			continue;
		/* Each key bit and each frame count bit alone. */
		for (unsigned int bit = 0; bit < key_len(n) * 8; bit++) {
			memset(key, 0, sizeof(key));
			key[bit / 8] = (uint8_t)(0x80 >> (bit % 8));
			stream_record("sweep", n, key, 0);
		}
		for (unsigned int bit = 0; bit < 22; bit++)
			stream_record("sweep", n, keys[0], 1u << bit);
		for (unsigned int i = 0; i < 64; i++) {
			for (size_t j = 0; j < key_len(n); j++)
				key[j] = (uint8_t)next_random(&state);
			stream_record("sweep", n, key, next_random(&state) % FN_COUNT_LIMIT);
		}
	}
}

/* The key has 16 bytes, so A5/4 reads only bytes it has. */
static void results(void)
{
	static const uint8_t key[16] = {
		0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
	};
	static const int values[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 255, -1, -2147483647 - 1, 2147483647 };
	ubit_t dl[BITS], ul[BITS], guard[BITS];

	memset(guard, GUARD, sizeof(guard));
	for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
		int rc;

		memset(dl, GUARD, sizeof(dl));
		memset(ul, GUARD, sizeof(ul));
		rc = osmo_a5(values[i], key, upstream_fn, dl, ul);
		if (rc && (memcmp(dl, guard, BITS) || memcmp(ul, guard, BITS)))
			fail("osmo_a5 wrote output and returned an error");
		printf("{\"kind\":\"result\",\"n\":%d,\"rc\":%d}\n", values[i], rc);
	}
	/* A5/0 is the only one that needs no key. */
	if (osmo_a5(0, NULL, upstream_fn, dl, ul))
		fail("A5/0 without a key");
}

static void replay_upstream_output(const char *ok_path)
{
	FILE *file = fopen(ok_path, "r");
	char line[256], expected[256], bits[BITS + 1];
	unsigned int seen = 0;

	if (!file) {
		perror(ok_path);
		exit(EXIT_FAILURE);
	}
	while (fgets(line, sizeof(line), file) && seen < UPSTREAM_LINES) {
		int n = (int)(seen / 2);
		const char *direction = seen % 2 ? "UL" : "DL";
		ubit_t out[BITS];

		if (seen % 2)
			osmo_a5(n, upstream_key, upstream_fn, NULL, out);
		else
			osmo_a5(n, upstream_key, upstream_fn, out, NULL);
		for (size_t i = 0; i < BITS; i++)
			bits[i] = (char)('0' + out[i]);
		bits[BITS] = 0;
		snprintf(expected, sizeof(expected), "A5/%d - %s: %s => OK\n", n, direction, bits);
		if (strcmp(line, expected))
			fail(line);
		seen++;
	}
	fclose(file);
	if (seen != UPSTREAM_LINES)
		fail("expected 6 upstream lines");
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s a5_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}

	replay_upstream_output(argv[1]);
	printf("{\"schema\":\"a5-v2\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	fn_counts();
	streams();
	results();
	return EXIT_SUCCESS;
}
