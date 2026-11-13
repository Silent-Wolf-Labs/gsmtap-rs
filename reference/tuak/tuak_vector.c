/*
 * Reference harness for libosmogsm's TUAK authentication functions (3GPP TS 35.231) and
 * the TUAK vectors osmo_auth_gen_vec2() and osmo_auth_gen_vec_auts2() give.
 *
 * The TUAK functions are internal to libosmogsm: they're in no exported symbol list, so
 * the harness links the internal libgsmint archive, as upstream's tuak_test does, and
 * the object whose constructor registers TUAK with the auth core. It writes NDJSON: a metadata line, then one record per observation, each kind in
 * one section.
 *
 * The arguments are upstream's tuak_test.c and tuak_test.ok. The harness reads every
 * test set from the source, rebuilds tuak_test.ok from its own calls and fails if a line
 * differs. It also checks f5*, which the upstream test parses but doesn't print.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/bit64gen.h>
#include <osmocom/core/utils.h>
#include <osmocom/crypt/auth.h>

/* Internal to tuak/tuak.c, declared in tuak/tuak.h, which isn't installed. */
int tuak_f1(const uint8_t *opc, const uint8_t *k, uint8_t k_len_bytes, const uint8_t *_rand,
	    const uint8_t *sqn, const uint8_t *amf, uint8_t *mac_a, uint8_t mac_a_len_bytes,
	    unsigned int keccac_iterations);
int tuak_f1star(const uint8_t *opc, const uint8_t *k, uint8_t k_len_bytes, const uint8_t *_rand,
		const uint8_t *sqn, const uint8_t *amf, uint8_t *mac_s, uint8_t mac_s_len_bytes,
		unsigned int keccac_iterations);
int tuak_f2345(const uint8_t *opc, const uint8_t *k, uint8_t k_len_bytes,
	       const uint8_t *_rand, uint8_t *res, uint8_t res_len_bytes,
	       uint8_t *ck, uint8_t ck_len_bytes,
	       uint8_t *ik, uint8_t ik_len_bytes, uint8_t *ak, unsigned int keccac_iterations);
int tuak_f5star(const uint8_t *opc, const uint8_t *k, uint8_t k_len_bytes,
		const uint8_t *_rand, uint8_t *ak, unsigned int keccac_iterations);
void tuak_set_keccak_iterations(unsigned int i);
int tuak_opc_gen(uint8_t *opc, const uint8_t *k, uint8_t k_len_bytes, const uint8_t *op);

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55
#define GUARD_BYTES 4
#define MAX_TEXT 65536
#define UPSTREAM_SETS 6

static const uint8_t zero32[32];
static const uint8_t counting[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};
static const uint8_t ones32[32] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
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

static uint64_t random_u64(uint32_t *state)
{
	uint64_t high = next_random(state);

	return high << 32 | next_random(state);
}

static void hex(const char *key, const uint8_t *bytes, size_t len)
{
	printf(",\"%s\":\"", key);
	for (size_t i = 0; i < len; i++)
		printf("%02x", bytes[i]);
	printf("\"");
}

/* The bytes a call wrote, then the guard bytes after them, checked untouched. */
static void written(const char *key, const uint8_t *buf, size_t size, size_t len)
{
	for (size_t i = len; i < size; i++)
		if (buf[i] != GUARD)
			fail("a TUAK function wrote past its output");
	hex(key, buf, len + GUARD_BYTES);
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

/* ------------------------------------------------------------------------ */
/* Upstream tuak_test                                                       */
/* ------------------------------------------------------------------------ */

struct test_set {
	char name[64];
	uint8_t k[32], rand[16], sqn[6], amf[2], top[32];
	uint8_t k_len;
	unsigned int iterations;
	uint8_t topc[32], mac_a[32], mac_s[32], res[32], ck[32], ik[32], ak[6], ak_star[6];
	uint8_t mac_len, res_len, ck_len, ik_len;
};

static struct test_set sets[UPSTREAM_SETS];

/* The quoted value after the next `field`, from *p on. */
static const char *next_quoted(const char **p, const char *field, char *out, size_t size)
{
	const char *start = strstr(*p, field), *end;

	if (!start)
		fail(field);
	start += strlen(field);
	end = strchr(start, '"');
	if (!end || (size_t)(end - start) >= size)
		fail(field);
	memcpy(out, start, end - start);
	out[end - start] = 0;
	*p = end + 1;
	return out;
}

/* Parses hex of 1 to max bytes and returns its length. */
static uint8_t next_hex(const char **p, const char *field, uint8_t *out, size_t max)
{
	char text[2 * 32 + 1];
	size_t len;

	next_quoted(p, field, text, sizeof(text));
	len = strlen(text) / 2;
	if (len == 0 || len > max || strlen(text) != 2 * len || osmo_hexparse(text, out, max) != (int)len)
		fail(field);
	return (uint8_t)len;
}

static void expect_len(uint8_t len, uint8_t expected, const char *what)
{
	if (len != expected)
		fail(what);
}

static void parse_tuak_test(const char *path)
{
	const char *p = read_text(path);
	size_t n = 0;

	p = strstr(p, "testspecs[] = {");
	if (!p)
		fail("tuak_test.c has no testspecs");
	while (n < UPSTREAM_SETS) {
		struct test_set *s = &sets[n++];
		const char *it;
		char *end;

		next_quoted(&p, ".name = \"", s->name, sizeof(s->name));
		s->k_len = next_hex(&p, ".k = \"", s->k, 32);
		if (s->k_len != 16 && s->k_len != 32)
			fail("upstream K length");
		expect_len(next_hex(&p, ".rand = \"", s->rand, 16), 16, "RAND");
		expect_len(next_hex(&p, ".sqn = \"", s->sqn, 6), 6, "SQN");
		expect_len(next_hex(&p, ".amf = \"", s->amf, 2), 2, "AMF");
		expect_len(next_hex(&p, ".top = \"", s->top, 32), 32, "TOP");
		it = strstr(p, ".keccak_iterations = ");
		if (!it)
			fail("iterations");
		s->iterations = (unsigned int)strtoul(it + strlen(".keccak_iterations = "), &end, 10);
		if (*end != ',' || s->iterations == 0)
			fail("iterations");
		p = end;
		expect_len(next_hex(&p, ".topc = \"", s->topc, 32), 32, "TOPc");
		s->mac_len = next_hex(&p, ".f1 = \"", s->mac_a, 32);
		expect_len(next_hex(&p, ".f1star = \"", s->mac_s, 32), s->mac_len, "MAC-S");
		s->res_len = next_hex(&p, ".f2 = \"", s->res, 32);
		s->ck_len = next_hex(&p, ".f3 = \"", s->ck, 32);
		s->ik_len = next_hex(&p, ".f4 = \"", s->ik, 32);
		expect_len(next_hex(&p, ".f5 = \"", s->ak, 6), 6, "AK");
		expect_len(next_hex(&p, ".f5star = \"", s->ak_star, 6), 6, "AK*");
	}
	if (strstr(p, ".name = \""))
		fail("more test sets in tuak_test.c than expected");
}

static void append_hex_line(char *expected, const char *label, const uint8_t *actual,
			    const uint8_t *wanted, size_t len)
{
	char line[256];

	if (memcmp(actual, wanted, len))
		fail(label);
	snprintf(line, sizeof(line), "\t%s: %s\r\n", label, osmo_hexdump_nospc(actual, len));
	strcat(expected, line);
}

static void replay_tuak_test(const char *ok_path)
{
	static char expected[MAX_TEXT];
	const char *ok = read_text(ok_path);

	for (size_t i = 0; i < UPSTREAM_SETS; i++) {
		const struct test_set *s = &sets[i];
		uint8_t topc[32], mac_a[32], mac_s[32], res[32], ck[32], ik[32], ak[6], ak_star[6];
		char line[96];

		snprintf(line, sizeof(line), "==> %s\n", s->name);
		strcat(expected, line);
		tuak_set_keccak_iterations(s->iterations);
		if (tuak_opc_gen(topc, s->k, s->k_len, s->top))
			fail("tuak_opc_gen");
		append_hex_line(expected, "TOPc", topc, s->topc, 32);
		if (tuak_f1(topc, s->k, s->k_len, s->rand, s->sqn, s->amf, mac_a, s->mac_len, s->iterations) ||
		    tuak_f1star(topc, s->k, s->k_len, s->rand, s->sqn, s->amf, mac_s, s->mac_len,
				s->iterations))
			fail("tuak_f1");
		append_hex_line(expected, "MAC_A", mac_a, s->mac_a, s->mac_len);
		append_hex_line(expected, "MAC_S", mac_s, s->mac_s, s->mac_len);
		if (tuak_f2345(topc, s->k, s->k_len, s->rand, res, s->res_len, ck, s->ck_len, ik, s->ik_len,
			       ak, s->iterations))
			fail("tuak_f2345");
		append_hex_line(expected, "RES", res, s->res, s->res_len);
		append_hex_line(expected, "CK", ck, s->ck, s->ck_len);
		append_hex_line(expected, "IK", ik, s->ik, s->ik_len);
		append_hex_line(expected, "AK", ak, s->ak, 6);
		if (tuak_f5star(topc, s->k, s->k_len, s->rand, ak_star, s->iterations) ||
		    memcmp(ak_star, s->ak_star, 6))
			fail("f5* differs from the value tuak_test.c gives");
	}
	tuak_set_keccak_iterations(1);
	if (strcmp(ok, expected))
		fail("tuak_test.ok differs from the harness's calls");
}

/* ------------------------------------------------------------------------ */
/* The TUAK functions                                                       */
/* ------------------------------------------------------------------------ */

static void opc_record(const char *source, const uint8_t *k, uint8_t k_len, const uint8_t *top,
		       unsigned int iterations)
{
	uint8_t topc[32 + GUARD_BYTES];
	int rc;

	memset(topc, GUARD, sizeof(topc));
	tuak_set_keccak_iterations(iterations);
	rc = tuak_opc_gen(topc, k, k_len, top);
	tuak_set_keccak_iterations(1);
	printf("{\"kind\":\"opc\",\"source\":\"%s\"", source);
	hex("k", k, 32);
	printf(",\"k_len\":%u", k_len);
	hex("top", top, 32);
	printf(",\"iterations\":%u,\"rc\":%d", iterations, rc);
	written("topc", topc, sizeof(topc), rc ? 0 : 32);
	printf("}\n");
}

static void opcs(void)
{
	static const uint8_t k_lens[] = { 16, 32, 0, 8, 24, 33 };
	uint32_t state = 0x70bc;
	uint8_t k[32], top[32];

	for (size_t i = 0; i < UPSTREAM_SETS; i++)
		opc_record("upstream", sets[i].k, sets[i].k_len, sets[i].top, sets[i].iterations);
	for (size_t i = 0; i < ARRAY_SIZE(k_lens); i++) {
		opc_record("sweep", counting, k_lens[i], ones32, 1);
		opc_record("sweep", ones32, k_lens[i], counting, 2);
	}
	opc_record("sweep", zero32, 16, zero32, 1);
	opc_record("sweep", zero32, 32, zero32, 3);
	for (int i = 0; i < 16; i++) {
		random_bytes(&state, k, sizeof(k));
		random_bytes(&state, top, sizeof(top));
		opc_record("sweep", k, i % 2 ? 32 : 16, top, 1 + i % 3);
	}
}

static void f1_record(const char *source, bool star, const uint8_t *k, uint8_t k_len,
		      const uint8_t *opc, const uint8_t *rand, const uint8_t *sqn, const uint8_t *amf,
		      uint8_t mac_len, unsigned int iterations)
{
	uint8_t mac[32 + GUARD_BYTES];
	int rc;

	memset(mac, GUARD, sizeof(mac));
	if (star)
		rc = tuak_f1star(opc, k, k_len, rand, sqn, amf, mac, mac_len, iterations);
	else
		rc = tuak_f1(opc, k, k_len, rand, sqn, amf, mac, mac_len, iterations);
	printf("{\"kind\":\"f1\",\"source\":\"%s\",\"star\":%s", source, star ? "true" : "false");
	hex("k", k, 32);
	printf(",\"k_len\":%u", k_len);
	hex("opc", opc, 32);
	hex("rand", rand, 16);
	hex("sqn", sqn, 6);
	hex("amf", amf, 2);
	printf(",\"mac_len\":%u,\"iterations\":%u,\"rc\":%d", mac_len, iterations, rc);
	written("mac", mac, sizeof(mac), rc ? 0 : mac_len);
	printf("}\n");
}

static void f1s(void)
{
	static const uint8_t mac_lens[] = { 8, 16, 32 };
	static const uint8_t bad_lens[] = { 0, 4, 12, 24, 64 };
	uint32_t state = 0xf1f1;
	uint8_t k[32], opc[32], rand[16], sqn[6], amf[2];

	for (size_t i = 0; i < UPSTREAM_SETS; i++) {
		const struct test_set *s = &sets[i];

		for (int star = 0; star <= 1; star++)
			f1_record("upstream", star, s->k, s->k_len, s->topc, s->rand, s->sqn, s->amf,
				  s->mac_len, s->iterations);
	}
	for (int star = 0; star <= 1; star++) {
		for (size_t i = 0; i < ARRAY_SIZE(mac_lens); i++)
			for (uint8_t k_len = 16; k_len <= 32; k_len += 16)
				for (unsigned int iterations = 1; iterations <= 2; iterations++)
					f1_record("sweep", star, counting, k_len, ones32, counting, counting + 16,
						  counting + 22, mac_lens[i], iterations);
		for (size_t i = 0; i < ARRAY_SIZE(bad_lens); i++)
			f1_record("sweep", star, counting, 16, ones32, counting, counting, counting, bad_lens[i], 1);
		f1_record("sweep", star, counting, 0, ones32, counting, counting, counting, 8, 1);
		f1_record("sweep", star, counting, 24, ones32, counting, counting, counting, 8, 1);
	}
	for (int i = 0; i < 24; i++) {
		random_bytes(&state, k, sizeof(k));
		random_bytes(&state, opc, sizeof(opc));
		random_bytes(&state, rand, sizeof(rand));
		random_bytes(&state, sqn, sizeof(sqn));
		random_bytes(&state, amf, sizeof(amf));
		f1_record("sweep", i % 2, k, i % 4 < 2 ? 16 : 32, opc, rand, sqn, amf, mac_lens[i % 3],
			  1 + i % 2);
	}
}

static void f2345_record(const char *source, const uint8_t *k, uint8_t k_len, const uint8_t *opc,
			 const uint8_t *rand, uint8_t res_len, uint8_t ck_len, uint8_t ik_len,
			 unsigned int iterations)
{
	uint8_t res[32 + GUARD_BYTES], ck[32 + GUARD_BYTES], ik[32 + GUARD_BYTES], ak[6 + GUARD_BYTES];
	int rc;

	memset(res, GUARD, sizeof(res));
	memset(ck, GUARD, sizeof(ck));
	memset(ik, GUARD, sizeof(ik));
	memset(ak, GUARD, sizeof(ak));
	rc = tuak_f2345(opc, k, k_len, rand, res, res_len, ck, ck_len, ik, ik_len, ak, iterations);
	printf("{\"kind\":\"f2345\",\"source\":\"%s\"", source);
	hex("k", k, 32);
	printf(",\"k_len\":%u", k_len);
	hex("opc", opc, 32);
	hex("rand", rand, 16);
	printf(",\"res_len\":%u,\"ck_len\":%u,\"ik_len\":%u,\"iterations\":%u,\"rc\":%d", res_len, ck_len,
	       ik_len, iterations, rc);
	written("res", res, sizeof(res), rc ? 0 : res_len);
	written("ck", ck, sizeof(ck), rc ? 0 : ck_len);
	written("ik", ik, sizeof(ik), rc ? 0 : ik_len);
	written("ak", ak, sizeof(ak), rc ? 0 : 6);
	printf("}\n");
}

static void f2345s(void)
{
	static const uint8_t res_lens[] = { 4, 8, 16, 32 };
	static const uint8_t key_lens[] = { 16, 32 };
	uint32_t state = 0xf2345;
	uint8_t k[32], opc[32], rand[16];

	for (size_t i = 0; i < UPSTREAM_SETS; i++) {
		const struct test_set *s = &sets[i];

		f2345_record("upstream", s->k, s->k_len, s->topc, s->rand, s->res_len, s->ck_len, s->ik_len,
			     s->iterations);
	}
	for (size_t r = 0; r < ARRAY_SIZE(res_lens); r++)
		for (size_t c = 0; c < ARRAY_SIZE(key_lens); c++)
			for (size_t i = 0; i < ARRAY_SIZE(key_lens); i++)
				for (size_t n = 0; n < ARRAY_SIZE(key_lens); n++)
					f2345_record("sweep", counting, key_lens[n], ones32, counting, res_lens[r],
						     key_lens[c], key_lens[i], 1);
	f2345_record("sweep", counting, 32, ones32, counting, 8, 16, 16, 2);
	f2345_record("sweep", counting, 32, ones32, counting, 8, 16, 16, 3);
	/* Lengths C rejects, one at a time. */
	f2345_record("sweep", counting, 16, ones32, counting, 0, 16, 16, 1);
	f2345_record("sweep", counting, 16, ones32, counting, 2, 16, 16, 1);
	f2345_record("sweep", counting, 16, ones32, counting, 12, 16, 16, 1);
	f2345_record("sweep", counting, 16, ones32, counting, 8, 8, 16, 1);
	f2345_record("sweep", counting, 16, ones32, counting, 8, 24, 16, 1);
	f2345_record("sweep", counting, 16, ones32, counting, 8, 16, 0, 1);
	f2345_record("sweep", counting, 16, ones32, counting, 8, 16, 64, 1);
	f2345_record("sweep", counting, 0, ones32, counting, 8, 16, 16, 1);
	f2345_record("sweep", counting, 24, ones32, counting, 8, 16, 16, 1);
	for (int i = 0; i < 24; i++) {
		random_bytes(&state, k, sizeof(k));
		random_bytes(&state, opc, sizeof(opc));
		random_bytes(&state, rand, sizeof(rand));
		f2345_record("sweep", k, key_lens[i % 2], opc, rand, res_lens[(i / 2) % 4], key_lens[(i / 8) % 2],
			     key_lens[(i / 3) % 2], 1 + i % 2);
	}
}

static void f5star_record(const char *source, const uint8_t *k, uint8_t k_len, const uint8_t *opc,
			  const uint8_t *rand, unsigned int iterations)
{
	uint8_t ak[6 + GUARD_BYTES];
	int rc;

	memset(ak, GUARD, sizeof(ak));
	rc = tuak_f5star(opc, k, k_len, rand, ak, iterations);
	printf("{\"kind\":\"f5star\",\"source\":\"%s\"", source);
	hex("k", k, 32);
	printf(",\"k_len\":%u", k_len);
	hex("opc", opc, 32);
	hex("rand", rand, 16);
	printf(",\"iterations\":%u,\"rc\":%d", iterations, rc);
	written("ak", ak, sizeof(ak), rc ? 0 : 6);
	printf("}\n");
}

static void f5stars(void)
{
	uint32_t state = 0xf5f5;
	uint8_t k[32], opc[32], rand[16];

	for (size_t i = 0; i < UPSTREAM_SETS; i++)
		f5star_record("upstream", sets[i].k, sets[i].k_len, sets[i].topc, sets[i].rand, sets[i].iterations);
	for (uint8_t k_len = 16; k_len <= 32; k_len += 16)
		for (unsigned int iterations = 1; iterations <= 3; iterations++)
			f5star_record("sweep", counting, k_len, ones32, counting, iterations);
	f5star_record("sweep", counting, 0, ones32, counting, 1);
	f5star_record("sweep", counting, 31, ones32, counting, 1);
	for (int i = 0; i < 12; i++) {
		random_bytes(&state, k, sizeof(k));
		random_bytes(&state, opc, sizeof(opc));
		random_bytes(&state, rand, sizeof(rand));
		f5star_record("sweep", k, i % 2 ? 32 : 16, opc, rand, 1 + i % 2);
	}
}

/* ------------------------------------------------------------------------ */
/* Vectors through the auth core                                            */
/* ------------------------------------------------------------------------ */

struct subscriber {
	enum osmo_sub_auth_type type;
	const uint8_t *key;
	uint8_t k_len;
	const uint8_t *opc;
	uint8_t opc_len;
	int opc_is_op;
	const uint8_t *amf;
	uint64_t sqn;
	unsigned int ind_bitlen;
	unsigned int ind;
};

static void fill(struct osmo_sub_auth_data2 *aud, const struct subscriber *s)
{
	memset(aud, 0, sizeof(*aud));
	aud->type = s->type;
	aud->algo = OSMO_AUTH_ALG_TUAK;
	memcpy(aud->u.umts.k, s->key, 32);
	aud->u.umts.k_len = s->k_len;
	memcpy(aud->u.umts.opc, s->opc, 32);
	aud->u.umts.opc_len = s->opc_len;
	aud->u.umts.opc_is_op = s->opc_is_op;
	memcpy(aud->u.umts.amf, s->amf, 2);
	aud->u.umts.sqn = s->sqn;
	aud->u.umts.ind_bitlen = s->ind_bitlen;
	aud->u.umts.ind = s->ind;
}

static void subscriber_fields(const struct subscriber *s)
{
	printf(",\"type\":%d", s->type);
	hex("key", s->key, 32);
	printf(",\"k_len\":%u", s->k_len);
	hex("opc", s->opc, 32);
	printf(",\"opc_len\":%u,\"opc_is_op\":%d", s->opc_len, s->opc_is_op);
	hex("amf", s->amf, 2);
	printf(",\"sqn\":%" PRIu64 ",\"ind_bitlen\":%u,\"ind\":%u", s->sqn, s->ind_bitlen, s->ind);
}

/* Everything the call can change, prefilled with GUARD apart from the RES length. */
static void outcome(int rc, const struct osmo_auth_vector *vec, const struct osmo_sub_auth_data2 *aud)
{
	printf(",\"rc\":%d", rc);
	hex("out_rand", vec->rand, 16);
	hex("autn", vec->autn, 16);
	hex("ck", vec->ck, 16);
	hex("ik", vec->ik, 16);
	hex("res", vec->res, 16);
	printf(",\"res_len\":%u", vec->res_len);
	hex("kc", vec->kc, 8);
	hex("sres", vec->sres, 4);
	printf(",\"auth_types\":%" PRIu32 ",\"out_sqn\":%" PRIu64 ",\"out_sqn_ms\":%" PRIu64, vec->auth_types,
	       aud->u.umts.sqn, aud->u.umts.sqn_ms);
}

static void gen_vec_record(const struct subscriber *s, uint8_t res_len, const uint8_t *rand)
{
	struct osmo_sub_auth_data2 aud;
	struct osmo_auth_vector vec;
	int rc;

	fill(&aud, s);
	memset(&vec, GUARD, sizeof(vec));
	vec.res_len = res_len;
	rc = osmo_auth_gen_vec2(&vec, &aud, rand);
	printf("{\"kind\":\"gen_vec\"");
	subscriber_fields(s);
	printf(",\"in_res_len\":%u", res_len);
	hex("rand", rand, 16);
	outcome(rc, &vec, &aud);
	printf("}\n");
}

static void gen_vec_auts_record(const struct subscriber *s, uint8_t res_len, const uint8_t *auts,
				const uint8_t *rand_auts, const uint8_t *rand)
{
	struct osmo_sub_auth_data2 aud;
	struct osmo_auth_vector vec;
	int rc;

	fill(&aud, s);
	memset(&vec, GUARD, sizeof(vec));
	vec.res_len = res_len;
	rc = osmo_auth_gen_vec_auts2(&vec, &aud, auts, rand_auts, rand);
	printf("{\"kind\":\"gen_vec_auts\"");
	subscriber_fields(s);
	printf(",\"in_res_len\":%u", res_len);
	hex("auts", auts, 14);
	hex("rand_auts", rand_auts, 16);
	hex("rand", rand, 16);
	outcome(rc, &vec, &aud);
	printf("}\n");
}

/*
 * A USIM's AUTS: (SQN_MS ^ AK*) || MAC-S, with AMF 0 (3GPP TS 33.102 section 6.3.3), from
 * the first k_len bytes of the key. The inputs come from the TUAK functions; the result
 * under test comes from osmo_auth_gen_vec_auts2().
 */
static void make_auts(const uint8_t *k, uint8_t k_len, const uint8_t *opc, uint8_t op_k_len,
		      const uint8_t *rand, uint64_t sqn_ms, uint8_t *auts)
{
	static const uint8_t amf0[2];
	uint8_t topc[32], ak[6], sqn[6];

	/* A nonzero op_k_len takes opc as OP, and derives OPc with that much of the key. */
	if (op_k_len) {
		if (tuak_opc_gen(topc, k, op_k_len, opc))
			fail("tuak_opc_gen");
	} else {
		memcpy(topc, opc, 32);
	}
	osmo_store64be_ext(sqn_ms, sqn, 6);
	if (tuak_f5star(topc, k, k_len, rand, ak, 1))
		fail("tuak_f5star");
	for (int i = 0; i < 6; i++)
		auts[i] = sqn[i] ^ ak[i];
	if (tuak_f1star(topc, k, k_len, rand, sqn, amf0, auts + 6, 8, 1))
		fail("tuak_f1star");
}

static void vectors(void)
{
	static const uint8_t amf0[2], amf_b9[2] = { 0xb9, 0xb9 };
	static const uint64_t sqns[] = {
		0, 0x21, 31, 32, 0xffffffffffffull, 0x1000000000000ull, UINT64_MAX, UINT64_MAX - 32,
	};
	static const unsigned int bitlens[] = { 0, 1, 5, 28, 29 };
	uint32_t state = 0x7ea4;
	uint8_t key[32], opc[32], rand[16], rand2[16], amf[2], auts[14];
	struct subscriber s;

	/* Upstream test set 2's inputs, through the auth core. */
	s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, sets[1].k, 32, sets[1].topc, 32, 0, sets[1].amf,
				 0x0123456789aaull, 0, 0 };
	for (uint8_t res_len = 0; res_len <= 32; res_len++)
		gen_vec_record(&s, res_len, sets[1].rand);
	s.opc = sets[1].top;
	s.opc_is_op = 1;
	gen_vec_record(&s, 8, sets[1].rand);
	/* OP is taken as OPc unless the type is UMTS. */
	s.type = OSMO_AUTH_TYPE_NONE;
	gen_vec_record(&s, 8, sets[1].rand);
	s.type = 3;
	gen_vec_record(&s, 8, sets[1].rand);
	/* Key and OPc lengths C checks. */
	s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, counting, 16, ones32, 32, 0, amf_b9, 5, 0, 0 };
	gen_vec_record(&s, 8, counting);
	s.k_len = 32;
	gen_vec_record(&s, 8, counting);
	for (int i = 0; i < 4; i++) {
		static const uint8_t bad[] = { 0, 8, 24, 33 };

		s.k_len = bad[i];
		gen_vec_record(&s, 8, counting);
	}
	s.k_len = 16;
	for (int i = 0; i < 3; i++) {
		static const uint8_t bad[] = { 0, 16, 31 };

		s.opc_len = bad[i];
		gen_vec_record(&s, 8, counting);
	}
	s.opc_len = 32;
	/* SQN and IND: every combination of these. */
	for (size_t i = 0; i < ARRAY_SIZE(sqns); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(bitlens); j++) {
			unsigned int bitlen = bitlens[j];
			unsigned int slots = 1u << bitlen;
			unsigned int inds[] = { 0, 1, slots - 1, slots };

			for (size_t n = 0; n < ARRAY_SIZE(inds); n++) {
				int seen = 0;

				for (size_t m = 0; m < n; m++)
					seen |= inds[m] == inds[n];
				if (seen)
					continue;
				s.sqn = sqns[i];
				s.ind_bitlen = bitlen;
				s.ind = inds[n];
				gen_vec_record(&s, 8, zero32);
			}
		}
	}
	for (int i = 0; i < 24; i++) {
		static const uint8_t res_lens[] = { 4, 8, 16 };

		random_bytes(&state, key, sizeof(key));
		random_bytes(&state, opc, sizeof(opc));
		random_bytes(&state, rand, sizeof(rand));
		random_bytes(&state, amf, sizeof(amf));
		s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, key, i % 2 ? 32 : 16, opc, 32, i % 3 == 0, amf,
					 random_u64(&state) & 0xffffffffffffull, (unsigned int)(i % 6), 0 };
		s.ind = next_random(&state) & ((1u << s.ind_bitlen) - 1);
		gen_vec_record(&s, res_lens[i % 3], rand);
	}

	/*
	 * AUTS. C checks AUTS with all 32 bytes of the key buffer, whatever k_len says, so a
	 * 16-byte K only passes with an AUTS made from that buffer as a 32-byte key.
	 */
	memcpy(key, counting, 32);
	s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, key, 32, ones32, 32, 0, amf0, 0x21, 0, 0 };
	make_auts(key, 32, ones32, 0, zero32, 31, auts);
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	s.ind_bitlen = 5;
	s.ind = 10;
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	/* AUTS passes, then the vector fails: C has already moved SQN. */
	s.ind = 32;
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	s.ind_bitlen = 29;
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	s.ind_bitlen = 5;
	s.ind = 10;
	gen_vec_auts_record(&s, 3, auts, zero32, zero32);
	gen_vec_auts_record(&s, 16, auts, zero32, zero32);
	auts[13] ^= 0x01;
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	auts[13] ^= 0x01;
	auts[0] ^= 0x01;
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	auts[0] ^= 0x01;
	s.k_len = 24;
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	s.k_len = 32;
	s.opc_len = 16;
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	s.opc_len = 32;
	/* A 16-byte K: an AUTS from the 16-byte key fails, one from the whole buffer passes. */
	s.k_len = 16;
	make_auts(key, 16, ones32, 0, zero32, 31, auts);
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	make_auts(key, 32, ones32, 0, zero32, 31, auts);
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	memset(key + 16, 0, 16);
	make_auts(key, 16, ones32, 0, zero32, 31, auts);
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	make_auts(key, 32, ones32, 0, zero32, 31, auts);
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	/* OP: OPc comes from the 16-byte K, AUTS from the whole buffer. */
	memcpy(key, counting, 32);
	s.opc_is_op = 1;
	make_auts(key, 32, ones32, 16, zero32, 31, auts);
	gen_vec_auts_record(&s, 8, auts, zero32, zero32);
	s.opc_is_op = 0;
	for (int i = 0; i < 12; i++) {
		uint64_t sqn_ms = random_u64(&state) & 0xffffffffffffull;

		random_bytes(&state, key, sizeof(key));
		random_bytes(&state, opc, sizeof(opc));
		random_bytes(&state, rand, sizeof(rand));
		random_bytes(&state, rand2, sizeof(rand2));
		random_bytes(&state, amf, sizeof(amf));
		s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, key, 32, opc, 32, i % 4 == 0, amf,
					 random_u64(&state) & 0xffffffffffffull, (unsigned int)(i % 6), 0 };
		make_auts(key, 32, opc, s.opc_is_op ? 32 : 0, rand2, sqn_ms, auts);
		/* The AUTS is checked against rand_auts, the new vector uses rand. */
		gen_vec_auts_record(&s, (uint8_t[]){ 4, 8, 16 }[i % 3], auts, rand2, rand);
		gen_vec_auts_record(&s, 8, auts, rand, rand2);
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s tuak_test.c tuak_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
	parse_tuak_test(argv[1]);
	replay_tuak_test(argv[2]);
	printf("{\"schema\":\"tuak-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	opcs();
	f1s();
	f2345s();
	f5stars();
	vectors();
	return EXIT_SUCCESS;
}
