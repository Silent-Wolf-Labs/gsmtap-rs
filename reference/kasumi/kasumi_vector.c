/*
 * Reference harness for libosmogsm's KASUMI block cipher, its KGCORE keystream
 * function and the count-based A5/3 and A5/4 streams built on it.
 *
 * These functions are internal to libosmogsm: they're in no exported symbol list, so
 * the harness links the internal libgsmint archive, as upstream's tests do. It writes
 * NDJSON: a metadata line, then one record per observation.
 *
 * The arguments are upstream's kasumi_test.c, kasumi_test.ok, a5_test.c and
 * a5_test.ok. The harness reads every vector from the two test sources, rebuilds
 * kasumi_test.ok and the A5/3 and A5/4 lines of a5_test.ok from its own calls, and
 * fails if a line differs.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/bits.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/kasumi.h>

/* Internal to a5.c and declared in no header, as in upstream's a5_test.c. */
void _a5_3(const uint8_t *key, uint32_t fn, ubit_t *dl, ubit_t *ul, bool fn_correct);
void _a5_4(const uint8_t *key, uint32_t fn, ubit_t *dl, ubit_t *ul, bool fn_correct);

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55
#define GUARD_BYTES 4
#define STREAM_BITS 114
#define MAX_TEXT 65536
#define MAX_OUT (65535 / 8 + 1 + GUARD_BYTES)

struct schedule {
	uint16_t kl1[8], kl2[8], ko1[8], ko2[8], ko3[8], ki1[8], ki2[8], ki3[8];
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

static void random_bytes(uint32_t *state, uint8_t *out, size_t len)
{
	for (size_t i = 0; i < len; i++)
		out[i] = (uint8_t)next_random(state);
}

static void print_hex(const char *key, const uint8_t *bytes, size_t len)
{
	printf("\"%s\":\"", key);
	for (size_t i = 0; i < len; i++)
		printf("%02x", bytes[i]);
	printf("\"");
}

static void print_words(const char *key, const uint16_t *words)
{
	printf(",\"%s\":\"", key);
	for (size_t i = 0; i < 8; i++)
		printf("%04x", words[i]);
	printf("\"");
}

static void print_bits(const char *key, const ubit_t *bits)
{
	printf(",\"%s\":\"", key);
	for (size_t i = 0; i < STREAM_BITS; i++)
		printf("%u", bits[i]);
	printf("\"");
}

static void expand(const uint8_t *key, struct schedule *s)
{
	_kasumi_key_expand(key, s->kl1, s->kl2, s->ko1, s->ko2, s->ko3, s->ki1, s->ki2, s->ki3);
}

static uint64_t encrypt(const struct schedule *s, uint64_t block)
{
	return _kasumi(block, s->kl1, s->kl2, s->ko1, s->ko2, s->ko3, s->ki1, s->ki2, s->ki3);
}

static char *read_text(const char *path)
{
	static char texts[4][MAX_TEXT];
	static int used;
	FILE *file = fopen(path, "r");
	size_t len;

	if (!file || used == 4) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	len = fread(texts[used], 1, MAX_TEXT - 1, file);
	if (ferror(file) || !feof(file))
		fail("test source too long or unreadable");
	fclose(file);
	texts[used][len] = 0;
	return texts[used++];
}

/* ------------------------------------------------------------------------ */
/* Upstream kasumi_test                                                     */
/* ------------------------------------------------------------------------ */

#define UPSTREAM_BLOCK_SETS 3
#define UPSTREAM_KGCORE_SETS 5
#define CHAIN_ROUNDS 50
/* Hex literals in kasumi_test.c: three block sets of key, 64 subkeys, ciphertext and
 * plaintext; set 4's key, input and result; and five KGCORE sets of key, gamma, CA and
 * COUNT. */
#define UPSTREAM_HEX (3 * (16 + 64 + 2) + (16 + 2) + 5 * (16 + 32 + 2))

static uint64_t upstream_hex[UPSTREAM_HEX];

struct block_set {
	uint8_t key[16];
	uint16_t subkeys[8][8];
	uint64_t plain, cipher;
};

struct kgcore_set {
	uint8_t key[16], gamma[32];
	uint8_t ca;
	uint32_t cc;
};

static struct block_set block_sets[UPSTREAM_BLOCK_SETS];
static uint8_t chain_key[16];
static uint64_t chain_input, chain_result;
static struct kgcore_set kgcore_sets[UPSTREAM_KGCORE_SETS];

static void parse_kasumi_test(const char *path)
{
	const char *text = read_text(path), *p = text;
	size_t n = 0, at = 0;

	while ((p = strstr(p, "0x"))) {
		char *end;

		if (p > text && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) {
			p += 2;
			continue;
		}
		if (n == UPSTREAM_HEX)
			fail("more hex literals in kasumi_test.c than expected");
		upstream_hex[n++] = strtoull(p, &end, 16);
		p = end;
	}
	if (n != UPSTREAM_HEX)
		fail("fewer hex literals in kasumi_test.c than expected");
	if (!strstr(text, "for (i = 0; i < 50; i++)"))
		fail("test set 4 no longer chains 50 rounds");
	for (size_t s = 0; s < UPSTREAM_BLOCK_SETS; s++) {
		for (size_t i = 0; i < 16; i++)
			block_sets[s].key[i] = (uint8_t)upstream_hex[at++];
		for (size_t a = 0; a < 8; a++)
			for (size_t i = 0; i < 8; i++)
				block_sets[s].subkeys[a][i] = (uint16_t)upstream_hex[at++];
		block_sets[s].cipher = upstream_hex[at++];
		block_sets[s].plain = upstream_hex[at++];
	}
	for (size_t i = 0; i < 16; i++)
		chain_key[i] = (uint8_t)upstream_hex[at++];
	chain_input = upstream_hex[at++];
	chain_result = upstream_hex[at++];
	for (size_t s = 0; s < UPSTREAM_KGCORE_SETS; s++) {
		for (size_t i = 0; i < 16; i++)
			kgcore_sets[s].key[i] = (uint8_t)upstream_hex[at++];
		for (size_t i = 0; i < 32; i++)
			kgcore_sets[s].gamma[i] = (uint8_t)upstream_hex[at++];
		kgcore_sets[s].ca = (uint8_t)upstream_hex[at++];
		kgcore_sets[s].cc = (uint32_t)upstream_hex[at++];
	}
}

static bool schedule_matches(const struct schedule *s, uint16_t subkeys[8][8])
{
	const uint16_t *arrays[8] = { s->kl1, s->kl2, s->ko1, s->ko2, s->ko3, s->ki1, s->ki2, s->ki3 };

	for (size_t a = 0; a < 8; a++)
		if (memcmp(arrays[a], subkeys[a], sizeof(subkeys[a])))
			return false;
	return true;
}

static void replay_kasumi_test(const char *ok_path)
{
	char expected[1024] = "testing KASUMI key expansion and encryption (ETSI TS 135 203):\n";
	const char *ok = read_text(ok_path);
	struct schedule s;
	uint64_t chained = chain_input;
	uint8_t gamma[32];

	for (size_t i = 0; i < UPSTREAM_BLOCK_SETS; i++) {
		char line[64];

		expand(block_sets[i].key, &s);
		snprintf(line, sizeof(line), "KASUMI Test Set %zu...%s%s\n", i + 1,
			 schedule_matches(&s, block_sets[i].subkeys) ? " OK. " : "FAILED!",
			 encrypt(&s, block_sets[i].plain) == block_sets[i].cipher ? "OK." : "FAILED!");
		strcat(expected, line);
	}
	expand(chain_key, &s);
	for (int i = 0; i < CHAIN_ROUNDS; i++)
		chained = encrypt(&s, chained);
	strcat(expected, chained == chain_result ? "KASUMI Test Set 4... OK.\n" : "KASUMI Test Set 4...FAILED!");
	for (size_t i = 0; i < UPSTREAM_KGCORE_SETS; i++) {
		char line[64];

		_kasumi_kgcore(kgcore_sets[i].ca, 0, kgcore_sets[i].cc, 0, kgcore_sets[i].key, gamma, 32 * 8);
		snprintf(line, sizeof(line), "KGCORE Test Set %zu: %d\n", i + 1,
			 !memcmp(gamma, kgcore_sets[i].gamma, sizeof(gamma)));
		strcat(expected, line);
	}
	if (strcmp(ok, expected))
		fail("kasumi_test.ok differs from the harness's calls");
}

/* ------------------------------------------------------------------------ */
/* Upstream a5_test, A5/3 and A5/4 lines                                    */
/* ------------------------------------------------------------------------ */

#define UPSTREAM_A5_VECTORS 12
#define A5_ZERO_TO_TWO_LINES 6

struct a5_vector {
	int n;
	uint8_t key[16];
	uint32_t count;
	uint8_t dl[15], ul[15];
};

static struct a5_vector a5_vectors[UPSTREAM_A5_VECTORS];

static void parse_hex(const char *text, uint8_t *out, size_t len)
{
	if (strlen(text) != len * 2 || osmo_hexparse(text, out, len) != (int)len)
		fail("bad hex in a5_test.c");
}

static void parse_a5_test(const char *path)
{
	const char *p = read_text(path);
	size_t n = 0;

	while ((p = strstr(p, "\ttest_a5"))) {
		struct a5_vector *v;
		char key[33], dl[31], ul[31];

		if (n == UPSTREAM_A5_VECTORS)
			fail("more A5/3 and A5/4 vectors than expected");
		v = &a5_vectors[n++];
		if (sscanf(p, "\ttest_a5%d(\"%32[0-9A-F]\", 0x%" SCNx32 ", \"%30[0-9A-F]\", \"%30[0-9A-F]\");",
			   &v->n, key, &v->count, dl, ul) != 5 || (v->n != 3 && v->n != 4))
			fail("unexpected A5/3 or A5/4 vector");
		parse_hex(key, v->key, v->n == 3 ? 8 : 16);
		parse_hex(dl, v->dl, sizeof(v->dl));
		parse_hex(ul, v->ul, sizeof(v->ul));
		p++;
	}
	if (n != UPSTREAM_A5_VECTORS)
		fail("fewer A5/3 and A5/4 vectors than expected");
}

static void a5_count_stream(int n, const uint8_t *key, uint32_t count, ubit_t *dl, ubit_t *ul)
{
	ubit_t only[STREAM_BITS];

	if (n == 3)
		_a5_3(key, count, dl, ul, false);
	else
		_a5_4(key, count, dl, ul, false);
	/* A direction that isn't asked for has no effect on the other. */
	if (n == 3)
		_a5_3(key, count, only, NULL, false);
	else
		_a5_4(key, count, only, NULL, false);
	if (memcmp(only, dl, STREAM_BITS))
		fail("downlink alone differs");
	if (n == 3)
		_a5_3(key, count, NULL, only, false);
	else
		_a5_4(key, count, NULL, only, false);
	if (memcmp(only, ul, STREAM_BITS))
		fail("uplink alone differs");
}

static void append_a5_line(char *expected, int n, const char *dir, const ubit_t *bits, const uint8_t *packed)
{
	uint8_t got[15];
	char line[256];

	osmo_ubit2pbit(got, bits, STREAM_BITS);
	snprintf(line, sizeof(line), "A5/%d - %s: %s => %s\n", n, dir, osmo_ubit_dump(bits, STREAM_BITS),
		 memcmp(got, packed, sizeof(got)) ? "FAIL" : "OK");
	strcat(expected, line);
}

static void replay_a5_test(const char *ok_path)
{
	static char expected[MAX_TEXT];
	const char *ok = read_text(ok_path);

	for (int i = 0; i < A5_ZERO_TO_TWO_LINES; i++) {
		ok = strchr(ok, '\n');
		if (!ok)
			fail("a5_test.ok is missing its A5/0 to A5/2 lines");
		ok++;
	}
	for (size_t i = 0; i < UPSTREAM_A5_VECTORS; i++) {
		const struct a5_vector *v = &a5_vectors[i];
		ubit_t dl[STREAM_BITS], ul[STREAM_BITS];

		a5_count_stream(v->n, v->key, v->count, dl, ul);
		append_a5_line(expected, v->n, "DL", dl, v->dl);
		append_a5_line(expected, v->n, "UL", ul, v->ul);
	}
	if (strcmp(ok, expected))
		fail("the A5/3 and A5/4 lines of a5_test.ok differ from the harness's calls");
}

/* ------------------------------------------------------------------------ */
/* Records                                                                  */
/* ------------------------------------------------------------------------ */

static const uint8_t pattern_keys[][16] = {
	{ 0 },
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10 },
	{ 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 },
	{ 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa },
};

static void key_expand_record(const char *source, const uint8_t *key)
{
	struct schedule s;

	expand(key, &s);
	printf("{\"kind\":\"key_expand\",\"source\":\"%s\",", source);
	print_hex("key", key, 16);
	print_words("kl1", s.kl1);
	print_words("kl2", s.kl2);
	print_words("ko1", s.ko1);
	print_words("ko2", s.ko2);
	print_words("ko3", s.ko3);
	print_words("ki1", s.ki1);
	print_words("ki2", s.ki2);
	print_words("ki3", s.ki3);
	printf("}\n");
}

static void key_expansions(void)
{
	uint32_t state = 0x6b5e;
	uint8_t key[16];

	for (size_t i = 0; i < UPSTREAM_BLOCK_SETS; i++)
		key_expand_record("upstream", block_sets[i].key);
	key_expand_record("upstream", chain_key);
	for (size_t i = 0; i < UPSTREAM_KGCORE_SETS; i++)
		key_expand_record("upstream", kgcore_sets[i].key);
	for (size_t i = 0; i < ARRAY_SIZE(pattern_keys); i++)
		key_expand_record("sweep", pattern_keys[i]);
	for (unsigned int bit = 0; bit < 128; bit++) {
		memset(key, 0, sizeof(key));
		key[bit / 8] = (uint8_t)(0x80 >> (bit % 8));
		key_expand_record("sweep", key);
	}
	for (int i = 0; i < 32; i++) {
		random_bytes(&state, key, sizeof(key));
		key_expand_record("sweep", key);
	}
}

static void block_record(const char *source, const uint8_t *key, uint64_t input)
{
	struct schedule s;

	expand(key, &s);
	printf("{\"kind\":\"block\",\"source\":\"%s\",", source);
	print_hex("key", key, 16);
	printf(",\"in\":\"%016" PRIx64 "\",\"out\":\"%016" PRIx64 "\"}\n", input, encrypt(&s, input));
}

static void blocks(void)
{
	static const uint64_t inputs[] = {
		0, UINT64_MAX, 0x0123456789abcdef, 0x8000000000000000, 1, 0xffffffff00000000, 0x00000000ffffffff,
	};
	uint32_t state = 0xb10c;
	struct schedule s;
	uint64_t chained = chain_input;
	uint8_t key[16];

	for (size_t i = 0; i < UPSTREAM_BLOCK_SETS; i++)
		block_record("upstream", block_sets[i].key, block_sets[i].plain);
	/* Test set 4 feeds each output back in. */
	expand(chain_key, &s);
	for (int i = 0; i < CHAIN_ROUNDS; i++) {
		block_record("upstream", chain_key, chained);
		chained = encrypt(&s, chained);
	}
	for (size_t i = 0; i < ARRAY_SIZE(pattern_keys); i++)
		for (size_t j = 0; j < ARRAY_SIZE(inputs); j++)
			block_record("sweep", pattern_keys[i], inputs[j]);
	for (unsigned int bit = 0; bit < 64; bit++)
		block_record("sweep", pattern_keys[2], (uint64_t)1 << bit);
	for (unsigned int bit = 0; bit < 128; bit++) {
		memset(key, 0, sizeof(key));
		key[bit / 8] = (uint8_t)(0x80 >> (bit % 8));
		block_record("sweep", key, 0);
	}
	for (int i = 0; i < 64; i++) {
		uint64_t input;

		random_bytes(&state, key, sizeof(key));
		input = (uint64_t)next_random(&state) << 32 | next_random(&state);
		block_record("sweep", key, input);
	}
}

static void kgcore_record(const char *source, uint8_t ca, uint8_t cb, uint32_t cc, uint8_t cd,
			  const uint8_t *key, uint16_t cl)
{
	static uint8_t out[MAX_OUT];
	size_t written = cl / 8 + (cl % 8 ? 1 : 0);

	memset(out, GUARD, sizeof(out));
	_kasumi_kgcore(ca, cb, cc, cd, key, out, cl);
	for (size_t i = written; i < sizeof(out); i++)
		if (out[i] != GUARD)
			fail("KGCORE wrote past the bits asked for");
	printf("{\"kind\":\"kgcore\",\"source\":\"%s\",\"ca\":%u,\"cb\":%u,\"cc\":%u,\"cd\":%u,", source, ca,
	       cb, cc, cd);
	print_hex("key", key, 16);
	printf(",\"cl\":%u,", cl);
	print_hex("out", out, written + GUARD_BYTES);
	printf("}\n");
}

static void kgcores(void)
{
	static const uint16_t lengths[] = {
		0, 1, 7, 8, 9, 56, 57, 63, 64, 65, 113, 114, 120, 127, 128, 129, 228, 256, 1000, 12184, 65535,
	};
	static const uint8_t cas[] = { 0, 0x0f, 0xff, 0x80 };
	static const uint32_t ccs[] = { 0, 1, 0x3fffff, 0x400000, 0x80000000, UINT32_MAX };
	/* Bearers past 5 bits and directions past 1 bit overlap other fields. */
	static const uint8_t cbs[] = { 0, 1, 16, 31, 32, 255 };
	static const uint8_t cds[] = { 0, 1, 2, 255 };
	uint32_t state = 0x9c0e;
	uint8_t key[16];

	for (size_t i = 0; i < UPSTREAM_KGCORE_SETS; i++)
		kgcore_record("upstream", kgcore_sets[i].ca, 0, kgcore_sets[i].cc, 0, kgcore_sets[i].key, 256);
	for (size_t i = 0; i < ARRAY_SIZE(lengths); i++)
		kgcore_record("sweep", 0x0f, 0, 0x24f20f, 0, kgcore_sets[0].key, lengths[i]);
	for (size_t i = 0; i < ARRAY_SIZE(cas); i++)
		for (size_t j = 0; j < ARRAY_SIZE(ccs); j++)
			kgcore_record("sweep", cas[i], 0, ccs[j], 0, pattern_keys[2], 128);
	for (size_t i = 0; i < ARRAY_SIZE(cbs); i++)
		for (size_t j = 0; j < ARRAY_SIZE(cds); j++)
			kgcore_record("sweep", 0xff, cbs[i], 0x12345678, cds[j], pattern_keys[2], 128);
	for (size_t i = 0; i < ARRAY_SIZE(pattern_keys); i++)
		kgcore_record("sweep", 0x0f, 0, 0, 0, pattern_keys[i], 228);
	for (int i = 0; i < 32; i++) {
		uint8_t ca = (uint8_t)next_random(&state), cb = next_random(&state) % 32;
		uint32_t cc = next_random(&state);
		uint8_t cd = next_random(&state) % 2;
		uint16_t cl = next_random(&state) % 2048;

		random_bytes(&state, key, sizeof(key));
		kgcore_record("sweep", ca, cb, cc, cd, key, cl);
	}
}

static void a5_count_record(const char *source, int n, const uint8_t *key, uint32_t count)
{
	ubit_t dl[STREAM_BITS], ul[STREAM_BITS];

	a5_count_stream(n, key, count, dl, ul);
	printf("{\"kind\":\"a5_count\",\"source\":\"%s\",\"n\":%d,", source, n);
	print_hex("key", key, n == 3 ? 8 : 16);
	printf(",\"count\":%u", count);
	print_bits("dl", dl);
	print_bits("ul", ul);
	printf("}\n");
}

static void a5_counts(void)
{
	static const uint32_t counts[] = { 0, 1, 0x3fffff, 0x400000, 0x80000000, UINT32_MAX };
	uint32_t state = 0xa534;
	uint8_t key[16];

	for (size_t i = 0; i < UPSTREAM_A5_VECTORS; i++)
		a5_count_record("upstream", a5_vectors[i].n, a5_vectors[i].key, a5_vectors[i].count);
	for (int n = 3; n <= 4; n++) {
		for (size_t i = 0; i < ARRAY_SIZE(pattern_keys); i++)
			for (size_t j = 0; j < ARRAY_SIZE(counts); j++)
				a5_count_record("sweep", n, pattern_keys[i], counts[j]);
		for (unsigned int bit = 0; bit < 32; bit++)
			a5_count_record("sweep", n, pattern_keys[0], (uint32_t)1 << bit);
		for (int i = 0; i < 16; i++) {
			random_bytes(&state, key, sizeof(key));
			a5_count_record("sweep", n, key, next_random(&state));
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 5) {
		fprintf(stderr, "usage: %s kasumi_test.c kasumi_test.ok a5_test.c a5_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}

	parse_kasumi_test(argv[1]);
	replay_kasumi_test(argv[2]);
	parse_a5_test(argv[3]);
	replay_a5_test(argv[4]);
	printf("{\"schema\":\"kasumi-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	key_expansions();
	blocks();
	kgcores();
	a5_counts();
	return EXIT_SUCCESS;
}
