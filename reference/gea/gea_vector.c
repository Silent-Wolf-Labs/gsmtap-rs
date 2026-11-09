/*
 * Reference harness for libosmogsm's GPRS ciphering API: GEA3 and GEA4 through
 * gprs_cipher_run(), the input helpers, the key lengths and the cipher names.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one record
 * per observation. The arguments are upstream's gea_test.c and gea_test.ok. The
 * harness reads every vector from gea_test.c, rebuilds gea_test.ok from its own
 * calls, and fails if a line differs.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/utils.h>
#include <osmocom/crypt/gprs_cipher.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55
#define GUARD_BYTES 4
#define MAX_TEXT 65536
#define MAX_LEN 1600
#define UPSTREAM_TESTS 9
#define UPSTREAM_REAL 4

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
	printf(",\"%s\":\"", key);
	for (size_t i = 0; i < len; i++)
		printf("%02x", bytes[i]);
	printf("\"");
}

static char *read_text(const char *path)
{
	static char texts[2][MAX_TEXT];
	static int used;
	FILE *file = fopen(path, "r");
	size_t len;

	if (!file || used == 2) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	len = fread(texts[used], 1, MAX_TEXT - 1, file);
	if (ferror(file) || !feof(file))
		fail("test file too long or unreadable");
	fclose(file);
	texts[used][len] = 0;
	return texts[used++];
}

/* gprs_cipher_run() with the key in a 16-byte buffer, as upstream's test passes it, so
 * GEA3 reads only the first 8. Records the output and its guard bytes. */
static int run(uint8_t *out, uint16_t len, int algo, const uint8_t *key, size_t key_len, uint32_t iv,
	       int dir)
{
	uint8_t kc[16] = { 0 };
	int rc;

	memcpy(kc, key, key_len);
	memset(out, GUARD, MAX_LEN + GUARD_BYTES);
	rc = gprs_cipher_run(out, len, (enum gprs_ciph_algo)algo, kc, iv, (enum gprs_cipher_direction)dir);
	for (size_t i = rc ? 0 : len; i < MAX_LEN + GUARD_BYTES; i++)
		if (out[i] != GUARD)
			fail(rc ? "gprs_cipher_run wrote output and returned an error"
				: "gprs_cipher_run wrote past the length");
	return rc;
}

/* ------------------------------------------------------------------------ */
/* Upstream gea_test                                                        */
/* ------------------------------------------------------------------------ */

struct upstream {
	bool v4;
	uint8_t key[16];
	uint32_t iv;
	int dir;
	uint16_t len;
	uint8_t expected[256];
};

static struct upstream upstream[UPSTREAM_TESTS + UPSTREAM_REAL];

static void parse_hex(const char *text, uint8_t *out, size_t len)
{
	if (strlen(text) != len * 2 || osmo_hexparse(text, out, len) != (int)len)
		fail("bad hex in gea_test.c");
}

static void parse_gea_test(const char *path)
{
	const char *text = read_text(path), *p = text;
	static char key[64], res[512];
	size_t n = 0;

	while ((p = strstr(p, "    test_gea("))) {
		struct upstream *u = &upstream[n];
		char v4[8];
		unsigned int len;

		if (n == UPSTREAM_TESTS)
			fail("more GEA vectors than expected");
		if (sscanf(p, "    test_gea(%7[a-z], \"%63[0-9A-F]\", 0x%" SCNx32 ", %d, %u, \"%511[0-9A-F]\");", v4, key,
			   &u->iv, &u->dir, &len, res) != 6 || len > sizeof(u->expected))
			fail("unexpected GEA vector");
		u->v4 = !strcmp(v4, "true");
		if (!u->v4 && strcmp(v4, "false"))
			fail("unexpected GEA version");
		u->len = (uint16_t)len;
		parse_hex(key, u->key, u->v4 ? 16 : 8);
		parse_hex(res, u->expected, len);
		n++;
		p++;
	}
	p = text;
	while ((p = strstr(p, "    real_gea("))) {
		struct upstream *u = &upstream[n];
		uint32_t iov_ui, lfn, oc;
		unsigned int sapi, len;
		char dir[32];

		if (n == UPSTREAM_TESTS + UPSTREAM_REAL)
			fail("more real GEA vectors than expected");
		if (sscanf(p, "    real_gea(%" SCNu32 ", %u, %" SCNu32 ", %" SCNu32 ", %31[A-Z0-9_], \"%63[0-9a-f]\", %u, \"%511[0-9a-f]\");",
			   &iov_ui, &sapi, &lfn, &oc, dir, key, &len, res) != 8 || len > sizeof(u->expected))
			fail("unexpected real GEA vector");
		if (!strcmp(dir, "GPRS_CIPH_SGSN2MS"))
			u->dir = GPRS_CIPH_SGSN2MS;
		else if (!strcmp(dir, "GPRS_CIPH_MS2SGSN"))
			u->dir = GPRS_CIPH_MS2SGSN;
		else
			fail("unexpected direction");
		u->v4 = false;
		u->iv = gprs_cipher_gen_input_ui(iov_ui, (uint8_t)sapi, lfn, oc);
		u->len = (uint16_t)len;
		parse_hex(key, u->key, 8);
		parse_hex(res, u->expected, len);
		n++;
		p++;
	}
	if (n != UPSTREAM_TESTS + UPSTREAM_REAL)
		fail("fewer GEA vectors than expected");
}

static void replay_gea_test(const char *ok_path)
{
	static char expected[MAX_TEXT];
	static uint8_t out[MAX_LEN + GUARD_BYTES];
	const char *ok = read_text(ok_path);

	snprintf(expected, sizeof(expected), "GEA3 support: %d\nGEA4 support: %d\n",
		 gprs_cipher_supported(GPRS_ALGO_GEA3), gprs_cipher_supported(GPRS_ALGO_GEA4));
	for (size_t i = 0; i < ARRAY_SIZE(upstream); i++) {
		const struct upstream *u = &upstream[i];
		int rc = run(out, u->len, u->v4 ? GPRS_ALGO_GEA4 : GPRS_ALGO_GEA3, u->key, u->v4 ? 16 : 8, u->iv,
			     u->dir);
		char line[128];

		snprintf(line, sizeof(line), "len %d, dir %d, INPUT 0x%X -> %s %s\n", u->len, u->dir, u->iv,
			 rc < 0 ? strerror(-rc) : "OK", memcmp(out, u->expected, u->len) ? "FAIL:" : "");
		strcat(expected, line);
	}
	if (strcmp(ok, expected))
		fail("gea_test.ok differs from the harness's calls");
}

/* ------------------------------------------------------------------------ */
/* Records                                                                  */
/* ------------------------------------------------------------------------ */

static const uint8_t pattern_keys[][16] = {
	{ 0 },
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10 },
};

static void run_record(const char *source, int algo, const uint8_t *key, size_t key_len, uint32_t iv, int dir,
		       uint16_t len)
{
	static uint8_t out[MAX_LEN + GUARD_BYTES];
	int rc = run(out, len, algo, key, key_len, iv, dir);

	printf("{\"kind\":\"run\",\"source\":\"%s\",\"algo\":%d", source, algo);
	print_hex("key", key, key_len);
	printf(",\"iv\":%u,\"dir\":%d,\"len\":%u,\"rc\":%d", iv, dir, len, rc);
	print_hex("out", out, (rc ? 0 : len) + GUARD_BYTES);
	printf("}\n");
}

static void runs(void)
{
	static const uint16_t lengths[] = { 0, 1, 7, 8, 9, 15, 16, 17, 59, 64, 65, 1500, 1522, 1523 };
	static const uint32_t ivs[] = { 0, 1, 0x80000000, UINT32_MAX };
	uint32_t state = 0x6ea3;
	uint8_t key[16];

	for (size_t i = 0; i < ARRAY_SIZE(upstream); i++) {
		const struct upstream *u = &upstream[i];

		run_record("upstream", u->v4 ? GPRS_ALGO_GEA4 : GPRS_ALGO_GEA3, u->key, u->v4 ? 16 : 8, u->iv,
			   u->dir, u->len);
	}
	for (int algo = GPRS_ALGO_GEA3; algo <= GPRS_ALGO_GEA4; algo++) {
		size_t key_len = algo == GPRS_ALGO_GEA4 ? 16 : 8;

		for (size_t i = 0; i < ARRAY_SIZE(lengths); i++)
			run_record("sweep", algo, pattern_keys[2], key_len, 0x12345678, GPRS_CIPH_SGSN2MS, lengths[i]);
		for (size_t i = 0; i < ARRAY_SIZE(pattern_keys); i++)
			for (size_t j = 0; j < ARRAY_SIZE(ivs); j++)
				for (int dir = 0; dir <= 1; dir++)
					run_record("sweep", algo, pattern_keys[i], key_len, ivs[j], dir, 32);
		/* Directions past 1 bit overlap the bearer field, which GEA leaves at 0. */
		for (int dir = 2; dir <= 3; dir++)
			run_record("sweep", algo, pattern_keys[2], key_len, 0x12345678, dir, 32);
		for (unsigned int bit = 0; bit < key_len * 8; bit++) {
			memset(key, 0, sizeof(key));
			key[bit / 8] = (uint8_t)(0x80 >> (bit % 8));
			run_record("sweep", algo, key, key_len, 0, GPRS_CIPH_MS2SGSN, 16);
		}
		for (int i = 0; i < 16; i++) {
			uint32_t iv = next_random(&state);
			int dir = next_random(&state) % 2;
			uint16_t len = next_random(&state) % 200;

			random_bytes(&state, key, sizeof(key));
			run_record("sweep", algo, key, key_len, iv, dir, len);
		}
	}
	/* Algorithms with no implementation or out of range, and a block that's too long. */
	for (int algo = -1; algo <= 6; algo++)
		run_record("result", algo, pattern_keys[2], algo == GPRS_ALGO_GEA3 ? 8 : 16, 0, 0, 8);
	run_record("result", GPRS_ALGO_GEA3, pattern_keys[2], 8, 0, 0, GSM0464_CIPH_MAX_BLOCK + 1);
	run_record("result", GPRS_ALGO_GEA4, pattern_keys[2], 16, 0, 0, UINT16_MAX);
	run_record("result", GPRS_ALGO_GEA1, pattern_keys[2], 8, 0, 0, UINT16_MAX);
}

static void algorithms(void)
{
	for (int algo = -1; algo <= 6; algo++) {
		const char *name = get_value_string_or_null(gprs_cipher_names, (uint32_t)algo);

		printf("{\"kind\":\"algorithm\",\"algo\":%d,\"supported\":%d,\"key_length\":%u,\"name\":", algo,
		       gprs_cipher_supported((enum gprs_ciph_algo)algo),
		       gprs_cipher_key_length((enum gprs_ciph_algo)algo));
		if (name)
			printf("\"%s\"}\n", name);
		else
			printf("null}\n");
	}
}

static void input_ui_record(uint32_t iov_ui, uint8_t sapi, uint32_t lfn, uint32_t oc)
{
	printf("{\"kind\":\"input_ui\",\"iov_ui\":%u,\"sapi\":%u,\"lfn\":%u,\"oc\":%u,\"input\":%u}\n", iov_ui, sapi,
	       lfn, oc, gprs_cipher_gen_input_ui(iov_ui, sapi, lfn, oc));
}

static void input_i_record(uint32_t iov_i, uint32_t lfn, uint32_t oc)
{
	printf("{\"kind\":\"input_i\",\"iov_i\":%u,\"lfn\":%u,\"oc\":%u,\"input\":%u}\n", iov_i, lfn, oc,
	       gprs_cipher_gen_input_i(iov_i, lfn, oc));
}

/* SAPIs from 16 on overflow an int in gprs_cipher_gen_input_ui(), so aren't called. */
static void inputs(void)
{
	static const uint32_t values[] = { 0, 1, 511, 512, 0x7fffffff, 0x80000000, UINT32_MAX };
	const size_t last = ARRAY_SIZE(values) - 1;
	uint32_t state;

	for (uint8_t sapi = 0; sapi < 16; sapi++)
		input_ui_record(0, sapi, 0, 0);
	for (size_t i = 0; i < ARRAY_SIZE(values); i++)
		for (size_t j = 0; j < ARRAY_SIZE(values); j++)
			input_ui_record(values[i], 3, values[j], values[last - j]);
	state = 0x1a7e;
	for (int i = 0; i < 32; i++) {
		uint32_t iov = next_random(&state), lfn = next_random(&state) % 512, oc = next_random(&state);

		input_ui_record(iov, next_random(&state) % 16, lfn, oc);
	}
	for (size_t i = 0; i < ARRAY_SIZE(values); i++)
		for (size_t j = 0; j < ARRAY_SIZE(values); j++)
			input_i_record(values[i], values[j], values[last - j]);
	state = 0x1a7f;
	for (int i = 0; i < 32; i++) {
		uint32_t iov = next_random(&state), lfn = next_random(&state) % 512, oc = next_random(&state);

		input_i_record(iov, lfn, oc);
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s gea_test.c gea_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}

	parse_gea_test(argv[1]);
	replay_gea_test(argv[2]);
	printf("{\"schema\":\"gea-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	algorithms();
	inputs();
	runs();
	return EXIT_SUCCESS;
}
