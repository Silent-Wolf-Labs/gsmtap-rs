/*
 * Reference harness for libosmogsm's COMP128 v1, v2 and v3 and XOR-2G (A3/A8).
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one record
 * per observation. The arguments are upstream's comp128_test.c, comp128_test.ok and
 * xor2g_test.ok. The harness replays the vectors in comp128_test.c and the output of
 * both tests, and the run fails if a result differs.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/utils.h>
#include <osmocom/crypt/auth.h>
#include <osmocom/gsm/comp128.h>
#include <osmocom/gsm/comp128v23.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55
#define UPSTREAM_VECTORS 1024

typedef void (*direct_fn)(const uint8_t *ki, const uint8_t *rand, uint8_t *sres, uint8_t *kc);

static const uint8_t upstream_ki[16] = {
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
};

static const uint8_t patterns[][16] = {
	{ 0 },
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	{ 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa },
	{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
	{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
	  0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10 },
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

/*
 * libosmogsm doesn't export comp128v1(), only the deprecated comp128(), which calls it.
 * The comparison with osmo_auth_gen_vec2() covers comp128v1() through the auth API.
 */
static void v1(const uint8_t *ki, const uint8_t *rand, uint8_t *sres, uint8_t *kc)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	comp128(ki, rand, sres, kc);
#pragma GCC diagnostic pop
}

/* comp128v2() and comp128v3() return an int, which is always 0. */
static void v2(const uint8_t *ki, const uint8_t *rand, uint8_t *sres, uint8_t *kc)
{
	if (comp128v2(ki, rand, sres, kc))
		fail("comp128v2 returned an error");
}

static void v3(const uint8_t *ki, const uint8_t *rand, uint8_t *sres, uint8_t *kc)
{
	if (comp128v3(ki, rand, sres, kc))
		fail("comp128v3 returned an error");
}

static const direct_fn versions[4] = { NULL, v1, v2, v3 };
static const enum osmo_auth_algo version_algos[4] = {
	OSMO_AUTH_ALG_NONE, OSMO_AUTH_ALG_COMP128v1, OSMO_AUTH_ALG_COMP128v2,
	OSMO_AUTH_ALG_COMP128v3,
};

/* Runs osmo_auth_gen_vec2() and checks it writes only RAND, SRES, Kc and the types. */
static void gen_vec(enum osmo_auth_algo algo, const uint8_t *ki, const uint8_t *rand,
		    struct osmo_auth_vector *vec)
{
	struct osmo_sub_auth_data2 aud;
	struct osmo_auth_vector guard;
	int rc;

	memset(&aud, 0, sizeof(aud));
	aud.type = OSMO_AUTH_TYPE_GSM;
	aud.algo = algo;
	memcpy(aud.u.gsm.ki, ki, 16);
	memset(vec, GUARD, sizeof(*vec));
	memset(&guard, GUARD, sizeof(guard));
	rc = osmo_auth_gen_vec2(vec, &aud, rand);
	if (rc)
		fail("osmo_auth_gen_vec2 returned an error");
	if (memcmp(vec->rand, rand, 16))
		fail("osmo_auth_gen_vec2 didn't copy RAND");
	if (vec->auth_types != OSMO_AUTH_TYPE_GSM)
		fail("osmo_auth_gen_vec2 types");
	if (memcmp(vec->autn, guard.autn, sizeof(vec->autn)) ||
	    memcmp(vec->ck, guard.ck, sizeof(vec->ck)) ||
	    memcmp(vec->ik, guard.ik, sizeof(vec->ik)) ||
	    memcmp(vec->res, guard.res, sizeof(vec->res)) || vec->res_len != guard.res_len)
		fail("osmo_auth_gen_vec2 wrote a UMTS field");
}

static void comp128_record(const char *source, int version, const uint8_t *ki,
			   const uint8_t *rand)
{
	uint8_t sres[4], kc[8];
	struct osmo_auth_vector vec;

	memset(sres, GUARD, sizeof(sres));
	memset(kc, GUARD, sizeof(kc));
	versions[version](ki, rand, sres, kc);
	/* The generic API gives the same result. */
	gen_vec(version_algos[version], ki, rand, &vec);
	if (memcmp(vec.sres, sres, 4) || memcmp(vec.kc, kc, 8))
		fail("osmo_auth_gen_vec2 differs from the direct call");
	printf("{\"kind\":\"comp128\",\"source\":\"%s\",\"version\":%d,", source, version);
	print_hex("ki", ki, 16);
	printf(",");
	print_hex("rand", rand, 16);
	printf(",");
	print_hex("sres", sres, 4);
	printf(",");
	print_hex("kc", kc, 8);
	printf("}\n");
}

static void xor2g_record(const uint8_t *ki, const uint8_t *rand)
{
	struct osmo_auth_vector vec;

	gen_vec(OSMO_AUTH_ALG_XOR_2G, ki, rand, &vec);
	printf("{\"kind\":\"xor_2g\",");
	print_hex("ki", ki, 16);
	printf(",");
	print_hex("rand", rand, 16);
	printf(",");
	print_hex("sres", vec.sres, 4);
	printf(",");
	print_hex("kc", vec.kc, 8);
	printf("}\n");
}

typedef void (*record_fn)(int version, const uint8_t *ki, const uint8_t *rand);

static void sweep_comp128(int version, const uint8_t *ki, const uint8_t *rand)
{
	comp128_record("sweep", version, ki, rand);
}

static void sweep_xor2g(int version, const uint8_t *ki, const uint8_t *rand)
{
	(void)version;
	xor2g_record(ki, rand);
}

/* Every pattern pair, each Ki bit and each RAND bit alone, then random pairs. */
static void sweep(record_fn record, int version, uint32_t seed, unsigned int randoms)
{
	uint8_t ki[16], rand[16];

	for (size_t i = 0; i < ARRAY_SIZE(patterns); i++)
		for (size_t j = 0; j < ARRAY_SIZE(patterns); j++)
			record(version, patterns[i], patterns[j]);
	for (unsigned int bit = 0; bit < 128; bit++) {
		memset(ki, 0, sizeof(ki));
		ki[bit / 8] = (uint8_t)(0x80 >> (bit % 8));
		record(version, ki, patterns[0]);
	}
	for (unsigned int bit = 0; bit < 128; bit++) {
		memset(rand, 0, sizeof(rand));
		rand[bit / 8] = (uint8_t)(0x80 >> (bit % 8));
		record(version, upstream_ki, rand);
	}
	for (unsigned int i = 0; i < randoms; i++) {
		random_bytes(&seed, ki, sizeof(ki));
		random_bytes(&seed, rand, sizeof(rand));
		record(version, ki, rand);
	}
}

static FILE *open_or_fail(const char *path)
{
	FILE *file = fopen(path, "r");

	if (!file) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	return file;
}

/*
 * comp128_test.c holds 1024 calls each of test_comp128v2("RAND", "SRES Kc") and
 * test_comp128v3(...), with Ki 0xaa repeated. Each is checked and recorded.
 */
static void replay_upstream_vectors(const char *test_path)
{
	FILE *file = open_or_fail(test_path);
	char line[256], rand_hex[33], result_hex[25];
	unsigned int seen[4] = { 0 };
	int version;

	while (fgets(line, sizeof(line), file)) {
		uint8_t rand[16], result[12], sres[4], kc[8];

		if (sscanf(line, "test_comp128v%d(\"%32[0-9A-Fa-f]\", \"%24[0-9A-Fa-f]\");", &version,
			   rand_hex, result_hex) != 3)
			continue;
		if ((version != 2 && version != 3) || strlen(rand_hex) != 32 ||
		    strlen(result_hex) != 24)
			fail(line);
		if (osmo_hexparse(rand_hex, rand, sizeof(rand)) != 16 ||
		    osmo_hexparse(result_hex, result, sizeof(result)) != 12)
			fail(line);
		versions[version](upstream_ki, rand, sres, kc);
		if (memcmp(sres, result, 4) || memcmp(kc, result + 4, 8))
			fail(line);
		comp128_record("upstream", version, upstream_ki, rand);
		seen[version]++;
	}
	fclose(file);
	if (seen[2] != UPSTREAM_VECTORS || seen[3] != UPSTREAM_VECTORS)
		fail("expected 1024 upstream vectors for each of v2 and v3");
}

static void expect_line(FILE *file, const char *expected)
{
	char line[256];

	if (!fgets(line, sizeof(line), file) || strcmp(line, expected))
		fail(expected);
}

static void expect_end(FILE *file, const char *path)
{
	char line[256];

	if (fgets(line, sizeof(line), file))
		fail(path);
	fclose(file);
}

/* Every check in comp128_test.c passes, so its output is the support lines and OKs. */
static void replay_comp128_output(const char *ok_path)
{
	FILE *file = open_or_fail(ok_path);
	char expected[64];

	snprintf(expected, sizeof(expected), "COMP128v2 support: %d\n",
		 osmo_auth_supported(osmo_auth_alg_parse("COMP128v2")));
	expect_line(file, expected);
	snprintf(expected, sizeof(expected), "COMP128v3 support: %d\n",
		 osmo_auth_supported(osmo_auth_alg_parse("COMP128v3")));
	expect_line(file, expected);
	for (unsigned int i = 0; i < 2 * UPSTREAM_VECTORS; i++)
		expect_line(file, "0 OK\n");
	expect_end(file, ok_path);
}

/* xor2g_test.c prints RAND, SRES and Kc for an all-zero RAND, then RAND bytes i << 4. */
static void replay_xor2g_output(const char *ok_path)
{
	static const uint8_t ki[16] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	};
	FILE *file = open_or_fail(ok_path);
	struct osmo_auth_vector vec;
	uint8_t rand[16];
	char expected[128];

	for (int pass = 0; pass < 2; pass++) {
		for (size_t i = 0; i < sizeof(rand); i++)
			rand[i] = pass ? (uint8_t)(i << 4) : 0;
		gen_vec(OSMO_AUTH_ALG_XOR_2G, ki, rand, &vec);
		snprintf(expected, sizeof(expected), "RAND:\t%s\n", osmo_hexdump(vec.rand, 16));
		expect_line(file, expected);
		snprintf(expected, sizeof(expected), "SRES:\t%s\n", osmo_hexdump(vec.sres, 4));
		expect_line(file, expected);
		snprintf(expected, sizeof(expected), "Kc:\t%s\n", osmo_hexdump(vec.kc, 8));
		expect_line(file, expected);
	}
	expect_end(file, ok_path);
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "usage: %s comp128_test.c comp128_test.ok xor2g_test.ok\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	replay_comp128_output(argv[2]);
	replay_xor2g_output(argv[3]);
	printf("{\"schema\":\"comp128-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	replay_upstream_vectors(argv[1]);
	for (int version = 1; version <= 3; version++)
		sweep(sweep_comp128, version, 0xc128u + (uint32_t)version, 64);
	sweep(sweep_xor2g, 0, 0x2f02u, 16);
	return EXIT_SUCCESS;
}
