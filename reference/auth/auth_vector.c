/*
 * Reference harness for libosmogsm's authentication core, Milenage and XOR-3G.
 *
 * Calls the API directly and writes NDJSON: a metadata line, then one record per
 * observation. The arguments are upstream's milenage_test.ok and xor2g_test.ok, whose
 * output is rebuilt from the same calls, and the run fails if a line differs.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/bit64gen.h>
#include <osmocom/core/utils.h>
#include <osmocom/crypt/auth.h>

/* Exported by libosmogsm, but declared only in the uninstalled milenage/milenage.h. */
int milenage_f1(const uint8_t *opc, const uint8_t *k, const uint8_t *_rand, const uint8_t *sqn,
		const uint8_t *amf, uint8_t *mac_a, uint8_t *mac_s);
int milenage_f2345(const uint8_t *opc, const uint8_t *k, const uint8_t *_rand, uint8_t *res,
		   uint8_t *ck, uint8_t *ik, uint8_t *ak, uint8_t *akstar);
int milenage_auts(const uint8_t *opc, const uint8_t *k, const uint8_t *_rand,
		  const uint8_t *auts, uint8_t *sqn);
int gsm_milenage(const uint8_t *opc, const uint8_t *k, const uint8_t *_rand, uint8_t *sres,
		 uint8_t *kc);
int milenage_check(const uint8_t *opc, const uint8_t *k, const uint8_t *sqn,
		   const uint8_t *_rand, const uint8_t *autn, uint8_t *ik, uint8_t *ck,
		   uint8_t *res, size_t *res_len, uint8_t *auts);
int milenage_opc_gen(uint8_t *opc, const uint8_t *k, const uint8_t *op);
void milenage_generate(const uint8_t *opc, const uint8_t *amf, const uint8_t *k,
		       const uint8_t *sqn, const uint8_t *_rand, uint8_t *autn, uint8_t *ik,
		       uint8_t *ck, uint8_t *res, size_t *res_len);
/* Exported by libosmogsm, but in no header. */
int osmo_auth_3g_from_2g(struct osmo_auth_vector *vec);

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55

static const uint8_t zero16[16];
static const uint8_t counting[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};
static const uint8_t ones16[16] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

/* The inputs of 3GPP TS 35.207 test set 1. The outputs are recorded from C. */
static const uint8_t set1_k[16] = {
	0x46, 0x5b, 0x5c, 0xe8, 0xb1, 0x99, 0xb4, 0x9f, 0xaa, 0x5f, 0x0a, 0x2e, 0xe2, 0x38, 0xa6, 0xbc,
};
static const uint8_t set1_rand[16] = {
	0x23, 0x55, 0x3c, 0xbe, 0x96, 0x37, 0xa8, 0x9d, 0x21, 0x8a, 0xe6, 0x4d, 0xae, 0x47, 0xbf, 0x35,
};
static const uint8_t set1_sqn[6] = { 0xff, 0x9b, 0xb4, 0xd0, 0xb6, 0x07 };
static const uint8_t set1_amf[2] = { 0xb9, 0xb9 };
static const uint8_t set1_op[16] = {
	0xcd, 0xc2, 0x02, 0xd5, 0x12, 0x3e, 0x20, 0xf6, 0x2b, 0x6d, 0x67, 0x6a, 0xc7, 0x2c, 0xb3, 0x18,
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

/* Where records go. Resync records are held back so each kind stays in one section. */
static FILE *out;
static FILE *held;

static void hex(const char *key, const uint8_t *bytes, size_t len)
{
	fprintf(out, ",\"%s\":\"", key);
	for (size_t i = 0; i < len; i++)
		fprintf(out, "%02x", bytes[i]);
	fprintf(out, "\"");
}

/* Names: osmo_auth_alg_name(), osmo_auth_alg_parse() and osmo_sub_auth_type_names. */
static void names(void)
{
	static const char *const texts[] = {
		"None", "COMP128v1", "COMP128v2", "COMP128v3", "XOR-3G", "MILENAGE", "XOR-2G", "TUAK",
		"none", "comp128V1", "milenage", "Milenage", "xor-3g", "tuak", "XOR", "XOR3G",
		"COMP128", "COMP128v4", "MILENAGE ", "", "unknown",
	};

	for (int algo = 0; algo <= _OSMO_AUTH_ALG_NUM; algo++)
		fprintf(out, "{\"kind\":\"alg_name\",\"algo\":%d,\"name\":\"%s\"}\n", algo,
		       osmo_auth_alg_name((enum osmo_auth_algo)algo));
	for (size_t i = 0; i < ARRAY_SIZE(texts); i++)
		fprintf(out, "{\"kind\":\"alg_parse\",\"text\":\"%s\",\"algo\":%d}\n", texts[i],
		       (int)osmo_auth_alg_parse(texts[i]));
	for (int type = 0; type <= 3; type++)
		fprintf(out, "{\"kind\":\"type_name\",\"type\":%d,\"name\":\"%s\"}\n", type,
		       get_value_string(osmo_sub_auth_type_names, type));
}

/* osmo_auth_c2(), osmo_auth_c3(), osmo_c4() and osmo_auth_3g_from_2g(). */
static void conversions(void)
{
	uint32_t state = 0xc2c3;
	uint8_t res[16], sres[4], ck[16], ik[16], kc[8];

	for (int func = 1; func <= 2; func++)
		for (size_t len = 0; len <= 16; len++) {
			random_bytes(&state, res, sizeof(res));
			memset(sres, GUARD, sizeof(sres));
			osmo_auth_c2(sres, res, len, (uint8_t)func);
			fprintf(out, "{\"kind\":\"c2\",\"func\":%d", func);
			hex("res", res, len);
			hex("sres", sres, 4);
			fprintf(out, "}\n");
		}
	for (int i = 0; i < 16; i++) {
		if (i == 0) {
			memcpy(ck, counting, 16);
			memcpy(ik, counting + 16, 16);
		} else {
			random_bytes(&state, ck, sizeof(ck));
			random_bytes(&state, ik, sizeof(ik));
		}
		memset(kc, GUARD, sizeof(kc));
		osmo_auth_c3(kc, ck, ik);
		fprintf(out, "{\"kind\":\"c3\"");
		hex("ck", ck, 16);
		hex("ik", ik, 16);
		hex("kc", kc, 8);
		fprintf(out, "}\n");
	}
	for (int i = 0; i < 8; i++) {
		random_bytes(&state, kc, sizeof(kc));
		memset(ck, GUARD, sizeof(ck));
		osmo_c4(ck, kc);
		fprintf(out, "{\"kind\":\"c4\"");
		hex("kc", kc, 8);
		hex("ck", ck, 16);
		fprintf(out, "}\n");
	}
	for (uint32_t types = 0; types <= 3; types++)
		for (int i = 0; i < 4; i++) {
			struct osmo_auth_vector vec;
			int rc;

			memset(&vec, GUARD, sizeof(vec));
			random_bytes(&state, vec.kc, sizeof(vec.kc));
			vec.auth_types = types;
			rc = osmo_auth_3g_from_2g(&vec);
			fprintf(out, "{\"kind\":\"from_2g\",\"auth_types\":%" PRIu32, types);
			hex("kc", vec.kc, 8);
			fprintf(out, ",\"rc\":%d", rc);
			hex("ck", vec.ck, 16);
			hex("ik", vec.ik, 16);
			fprintf(out, "}\n");
		}
}

/* Milenage's functions, called directly. */
static void milenage_record(const uint8_t *k, const uint8_t *opc, const uint8_t *rand,
			    const uint8_t *sqn, const uint8_t *amf)
{
	uint8_t mac_a[8], mac_s[8], res[8], ck[16], ik[16], ak[6], akstar[6], sres[4], kc[8];

	if (milenage_f1(opc, k, rand, sqn, amf, mac_a, mac_s) ||
	    milenage_f2345(opc, k, rand, res, ck, ik, ak, akstar) ||
	    gsm_milenage(opc, k, rand, sres, kc))
		fail("Milenage returned an error");
	fprintf(out, "{\"kind\":\"milenage\"");
	hex("k", k, 16);
	hex("opc", opc, 16);
	hex("rand", rand, 16);
	hex("sqn", sqn, 6);
	hex("amf", amf, 2);
	hex("mac_a", mac_a, 8);
	hex("mac_s", mac_s, 8);
	hex("res", res, 8);
	hex("ck", ck, 16);
	hex("ik", ik, 16);
	hex("ak", ak, 6);
	hex("ak_star", akstar, 6);
	hex("sres", sres, 4);
	hex("kc", kc, 8);
	fprintf(out, "}\n");
}

static void opc_record(const uint8_t *k, const uint8_t *op)
{
	uint8_t opc[16];

	if (milenage_opc_gen(opc, k, op))
		fail("milenage_opc_gen returned an error");
	fprintf(out, "{\"kind\":\"opc\"");
	hex("k", k, 16);
	hex("op", op, 16);
	hex("opc", opc, 16);
	fprintf(out, "}\n");
}

/* An AUTS as a USIM at SQN_MS would send it: (SQN_MS ^ AK*) || MAC-S, with AMF 0. */
static void milenage_make_auts(const uint8_t *k, const uint8_t *opc, const uint8_t *rand,
			       uint64_t sqn_ms, uint8_t *auts)
{
	static const uint8_t amf[2];
	uint8_t sqn[6], akstar[6];

	osmo_store64be_ext(sqn_ms, sqn, 6);
	if (milenage_f2345(opc, k, rand, NULL, NULL, NULL, NULL, akstar) ||
	    milenage_f1(opc, k, rand, sqn, amf, NULL, auts + 6))
		fail("Milenage returned an error");
	for (int i = 0; i < 6; i++)
		auts[i] = sqn[i] ^ akstar[i];
}

static void auts_record(const uint8_t *k, const uint8_t *opc, const uint8_t *rand,
			const uint8_t *auts)
{
	uint8_t sqn[6];
	int rc;

	memset(sqn, GUARD, sizeof(sqn));
	rc = milenage_auts(opc, k, rand, auts, sqn);
	fprintf(out, "{\"kind\":\"auts\"");
	hex("k", k, 16);
	hex("opc", opc, 16);
	hex("rand", rand, 16);
	hex("auts", auts, 14);
	fprintf(out, ",\"rc\":%d", rc);
	hex("sqn", sqn, 6);
	fprintf(out, "}\n");
}

/* milenage_check(), the USIM side: verify AUTN against the USIM's SQN. */
static void check_record(const uint8_t *k, const uint8_t *opc, const uint8_t *sqn,
			 const uint8_t *rand, const uint8_t *autn)
{
	uint8_t ik[16], ck[16], res[8], auts[14];
	size_t res_len = GUARD;
	int rc;

	memset(ik, GUARD, sizeof(ik));
	memset(ck, GUARD, sizeof(ck));
	memset(res, GUARD, sizeof(res));
	memset(auts, GUARD, sizeof(auts));
	rc = milenage_check(opc, k, sqn, rand, autn, ik, ck, res, &res_len, auts);
	fprintf(out, "{\"kind\":\"check\"");
	hex("k", k, 16);
	hex("opc", opc, 16);
	hex("sqn", sqn, 6);
	hex("rand", rand, 16);
	hex("autn", autn, 16);
	fprintf(out, ",\"rc\":%d,\"res_len\":%zu", rc, res_len);
	hex("res", res, 8);
	hex("ck", ck, 16);
	hex("ik", ik, 16);
	hex("auts", auts, 14);
	fprintf(out, "}\n");
}

static void milenage_functions(void)
{
	uint32_t state = 0x3135;
	uint8_t k[16], opc[16], op[16], rand[16], sqn[6], amf[2], autn[16], auts[14];
	size_t res_len;
	uint8_t res[8], ck[16], ik[16];

	if (milenage_opc_gen(opc, set1_k, set1_op))
		fail("milenage_opc_gen");
	opc_record(set1_k, set1_op);
	opc_record(counting, zero16);
	opc_record(zero16, zero16);
	opc_record(ones16, ones16);
	for (int i = 0; i < 8; i++) {
		random_bytes(&state, k, sizeof(k));
		random_bytes(&state, op, sizeof(op));
		opc_record(k, op);
	}

	milenage_record(set1_k, opc, set1_rand, set1_sqn, set1_amf);
	milenage_record(counting, counting, zero16, zero16, zero16);
	milenage_record(zero16, zero16, zero16, zero16, zero16);
	milenage_record(ones16, ones16, ones16, ones16, ones16);
	for (int i = 0; i < 24; i++) {
		random_bytes(&state, k, sizeof(k));
		random_bytes(&state, opc, sizeof(opc));
		random_bytes(&state, rand, sizeof(rand));
		random_bytes(&state, sqn, sizeof(sqn));
		random_bytes(&state, amf, sizeof(amf));
		milenage_record(k, opc, rand, sqn, amf);
	}

	/* AUTS: valid, each byte corrupted, and from a different RAND. */
	for (int i = 0; i < 6; i++) {
		static const uint64_t sqns[] = { 0, 0x1f, 0xffffffffffffull, 1ull << 47, 0x123456789abc };

		random_bytes(&state, k, sizeof(k));
		random_bytes(&state, opc, sizeof(opc));
		random_bytes(&state, rand, sizeof(rand));
		milenage_make_auts(k, opc, rand, i < 5 ? sqns[i] : random_u64(&state), auts);
		auts_record(k, opc, rand, auts);
		for (int byte = 0; byte < 14; byte += 13) {
			auts[byte] ^= 0x01;
			auts_record(k, opc, rand, auts);
			auts[byte] ^= 0x01;
		}
		rand[15] ^= 0x80;
		auts_record(k, opc, rand, auts);
	}

	/* milenage_check(): a fresh SQN, an equal one, an older one, a newer one and a bad MAC. */
	for (int i = 0; i < 6; i++) {
		uint8_t usim_sqn[6];
		uint64_t network;
		static const int64_t deltas[] = { 1, 0, -1, 1000, 1, 1 };

		random_bytes(&state, k, sizeof(k));
		random_bytes(&state, opc, sizeof(opc));
		random_bytes(&state, rand, sizeof(rand));
		random_bytes(&state, amf, sizeof(amf));
		network = (random_u64(&state) & 0xffffffffffull) + 2000;
		osmo_store64be_ext(network, sqn, 6);
		osmo_store64be_ext(network - (uint64_t)deltas[i], usim_sqn, 6);
		res_len = sizeof(res);
		milenage_generate(opc, amf, k, sqn, rand, autn, ik, ck, res, &res_len);
		if (res_len != 8)
			fail("milenage_generate");
		if (i == 4)
			autn[15] ^= 0x01;
		if (i == 5)
			autn[6] ^= 0x01;
		check_record(k, opc, usim_sqn, rand, autn);
	}
}

struct subscriber {
	enum osmo_sub_auth_type type;
	enum osmo_auth_algo algo;
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
	aud->algo = s->algo;
	if (s->type == OSMO_AUTH_TYPE_GSM) {
		memcpy(aud->u.gsm.ki, s->key, 16);
		return;
	}
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
	fprintf(out, ",\"type\":%d,\"algo\":%d", s->type, s->algo);
	hex("key", s->key, 32);
	fprintf(out, ",\"k_len\":%u", s->k_len);
	hex("opc", s->opc, 32);
	fprintf(out, ",\"opc_len\":%u,\"opc_is_op\":%d", s->opc_len, s->opc_is_op);
	hex("amf", s->amf, 2);
	fprintf(out, ",\"sqn\":%" PRIu64 ",\"ind_bitlen\":%u,\"ind\":%u", s->sqn, s->ind_bitlen, s->ind);
}

/* Everything the call can change, prefilled with GUARD apart from the RES length. */
static void outcome(int rc, const struct osmo_auth_vector *vec,
		    const struct osmo_sub_auth_data2 *aud)
{
	fprintf(out, ",\"rc\":%d", rc);
	hex("out_rand", vec->rand, 16);
	hex("autn", vec->autn, 16);
	hex("ck", vec->ck, 16);
	hex("ik", vec->ik, 16);
	hex("res", vec->res, 16);
	fprintf(out, ",\"res_len\":%u", vec->res_len);
	hex("kc", vec->kc, 8);
	hex("sres", vec->sres, 4);
	fprintf(out, ",\"auth_types\":%" PRIu32, vec->auth_types);
	if (aud->type == OSMO_AUTH_TYPE_GSM)
		hex("out_key", aud->u.gsm.ki, 16);
	else
		fprintf(out, ",\"out_sqn\":%" PRIu64 ",\"out_sqn_ms\":%" PRIu64, aud->u.umts.sqn,
		       aud->u.umts.sqn_ms);
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
	fprintf(out, "{\"kind\":\"gen_vec\"");
	subscriber_fields(s);
	fprintf(out, ",\"in_res_len\":%u", res_len);
	hex("rand", rand, 16);
	outcome(rc, &vec, &aud);
	fprintf(out, "}\n");
}

static void gen_vec_auts_record(const struct subscriber *s, uint8_t res_len,
				const uint8_t *auts, const uint8_t *rand_auts,
				const uint8_t *rand)
{
	struct osmo_sub_auth_data2 aud;
	struct osmo_auth_vector vec;
	int rc;

	FILE *records = out;

	out = held;
	fill(&aud, s);
	memset(&vec, GUARD, sizeof(vec));
	vec.res_len = res_len;
	rc = osmo_auth_gen_vec_auts2(&vec, &aud, auts, rand_auts, rand);
	fprintf(out, "{\"kind\":\"gen_vec_auts\"");
	subscriber_fields(s);
	fprintf(out, ",\"in_res_len\":%u", res_len);
	hex("auts", auts, 14);
	hex("rand_auts", rand_auts, 16);
	hex("rand", rand, 16);
	outcome(rc, &vec, &aud);
	fprintf(out, "}\n");
	out = records;
}

/* An XOR-3G AUTS: (SQN_MS ^ AK) || MAC-S, both from Ki XOR RAND, with AMF 0. */
static void xor_make_auts(const uint8_t *k, const uint8_t *rand, uint64_t sqn_ms, uint8_t *auts)
{
	uint8_t xdout[16], sqn[6];

	for (int i = 0; i < 16; i++)
		xdout[i] = k[i] ^ rand[i];
	osmo_store64be_ext(sqn_ms, sqn, 6);
	for (int i = 0; i < 6; i++)
		auts[i] = sqn[i] ^ xdout[3 + i];
	for (int i = 0; i < 8; i++)
		auts[6 + i] = xdout[i] ^ (i < 6 ? sqn[i] : 0);
}

static void vectors(void)
{
	uint32_t state = 0x6e5ec;
	uint8_t key[32], opc[32], rand[16], rand2[16], amf[2], auts[14];
	static const uint8_t amf0[2], amf_b9[2] = { 0xb9, 0xb9 };
	static const uint64_t sqns[] = {
		0, 0x21, 31, 32, 0xffffffffffffull, 0x1000000000000ull, UINT64_MAX, UINT64_MAX - 32,
	};
	struct subscriber s;

	/* Milenage. */
	uint8_t set1_opc[32] = { 0 }, set1_key[32] = { 0 }, set1_op32[32] = { 0 };

	memcpy(set1_key, set1_k, 16);
	memcpy(set1_op32, set1_op, 16);
	if (milenage_opc_gen(set1_opc, set1_k, set1_op))
		fail("milenage_opc_gen");
	s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, OSMO_AUTH_ALG_MILENAGE, set1_key, 16,
				 set1_opc, 16, 0, set1_amf, 0xff9bb4d0b606ull, 0, 0 };
	gen_vec_record(&s, 8, set1_rand);
	gen_vec_record(&s, 4, set1_rand);
	s.opc = set1_op32;
	s.opc_is_op = 1;
	gen_vec_record(&s, 8, set1_rand);
	/* Errors: lengths C checks, IND limits. */
	s.opc_is_op = 0;
	s.opc = set1_opc;
	for (uint8_t res_len = 0; res_len <= 16; res_len++)
		if (res_len != 4 && res_len != 8)
			gen_vec_record(&s, res_len, set1_rand);
	s.k_len = 32;
	gen_vec_record(&s, 8, set1_rand);
	s.k_len = 0;
	gen_vec_record(&s, 8, set1_rand);
	s.k_len = 16;
	s.opc_len = 32;
	gen_vec_record(&s, 8, set1_rand);
	s.opc_len = 16;
	/* SQN and IND: every combination of these. */
	for (size_t i = 0; i < ARRAY_SIZE(sqns); i++) {
		static const unsigned int bitlens[] = { 0, 1, 5, 28, 29 };

		for (size_t j = 0; j < ARRAY_SIZE(bitlens); j++) {
			unsigned int bitlen = bitlens[j];
			unsigned int slots = bitlen < 32 ? 1u << bitlen : 0;
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
				gen_vec_record(&s, 8, zero16);
			}
		}
	}
	/* Other types: C reads the UMTS fields anyway, but takes OP as OPc. */
	s = (struct subscriber){ OSMO_AUTH_TYPE_NONE, OSMO_AUTH_ALG_MILENAGE, set1_key, 16,
				 set1_op32, 16, 1, set1_amf, 0xff9bb4d0b606ull, 0, 0 };
	gen_vec_record(&s, 8, set1_rand);
	s.opc_is_op = 0;
	gen_vec_record(&s, 8, set1_rand);
	s.type = 3;
	s.opc_is_op = 1;
	gen_vec_record(&s, 8, set1_rand);
	/* A GSM subscriber, and other types. */
	s = (struct subscriber){ OSMO_AUTH_TYPE_GSM, OSMO_AUTH_ALG_MILENAGE, counting, 0,
				 zero16, 0, 0, amf0, 0, 0, 0 };
	memset(key, 0, sizeof(key));
	memcpy(key, counting, 16);
	s.key = key;
	memset(opc, 0, sizeof(opc));
	s.opc = opc;
	gen_vec_record(&s, 8, zero16);
	for (int i = 0; i < 24; i++) {
		random_bytes(&state, key, 16);
		random_bytes(&state, opc, 16);
		random_bytes(&state, rand, sizeof(rand));
		random_bytes(&state, amf, sizeof(amf));
		s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, OSMO_AUTH_ALG_MILENAGE, key, 16, opc,
					 16, i % 3 == 0, amf, random_u64(&state) & 0xffffffffffffull,
					 (unsigned int)(i % 6), 0 };
		s.ind = next_random(&state) & ((1u << s.ind_bitlen) - 1);
		gen_vec_record(&s, i % 2 ? 4 : 8, rand);
	}

	/* Milenage AUTS: the upstream case, valid ones, corrupted ones, a separate RAND. */
	memset(key, 0, sizeof(key));
	memcpy(key, counting, 16);
	memset(opc, 0, sizeof(opc));
	memcpy(opc, counting, 16);
	s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, OSMO_AUTH_ALG_MILENAGE, key, 16, opc, 16, 0,
				 amf0, 0x21, 0, 0 };
	milenage_make_auts(key, opc, zero16, 31, auts);
	gen_vec_auts_record(&s, 8, auts, zero16, zero16);
	s.ind_bitlen = 5;
	s.ind = 10;
	gen_vec_auts_record(&s, 8, auts, zero16, zero16);
	/* AUTS passes, then the vector fails: C has already moved SQN. */
	s.ind = 32;
	gen_vec_auts_record(&s, 8, auts, zero16, zero16);
	s.ind = 10;
	gen_vec_auts_record(&s, 3, auts, zero16, zero16);
	auts[13] ^= 0x01;
	gen_vec_auts_record(&s, 8, auts, zero16, zero16);
	auts[13] ^= 0x01;
	s.k_len = 32;
	gen_vec_auts_record(&s, 8, auts, zero16, zero16);
	s.k_len = 16;
	for (int i = 0; i < 12; i++) {
		uint64_t sqn_ms = random_u64(&state) & 0xffffffffffffull;

		random_bytes(&state, key, 16);
		random_bytes(&state, opc, 16);
		random_bytes(&state, rand, sizeof(rand));
		random_bytes(&state, rand2, sizeof(rand2));
		random_bytes(&state, amf, sizeof(amf));
		s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, OSMO_AUTH_ALG_MILENAGE, key, 16, opc,
					 16, 0, amf, random_u64(&state) & 0xffffffffffffull,
					 (unsigned int)(i % 6), 0 };
		milenage_make_auts(key, opc, rand2, sqn_ms, auts);
		/* The AUTS is checked against rand_auts, the new vector uses rand. */
		gen_vec_auts_record(&s, 8, auts, rand2, rand);
		gen_vec_auts_record(&s, 8, auts, rand, rand2);
	}

	/* XOR-3G with UMTS and GSM subscribers. */
	for (int i = 0; i < 16; i++) {
		random_bytes(&state, key, 32);
		random_bytes(&state, opc, 32);
		random_bytes(&state, rand, sizeof(rand));
		random_bytes(&state, amf, sizeof(amf));
		s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, OSMO_AUTH_ALG_XOR_3G, key, 16, opc,
					 (uint8_t)(i % 2 ? 32 : 16), 0, amf,
					 (size_t)i < ARRAY_SIZE(sqns) ? sqns[i] : random_u64(&state),
					 (unsigned int)(i % 30), (unsigned int)i * 3 };
		gen_vec_record(&s, (uint8_t)(i % 9), rand);
		s.type = OSMO_AUTH_TYPE_GSM;
		gen_vec_record(&s, 8, rand);
	}
	s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, OSMO_AUTH_ALG_XOR_3G, key, 32, opc, 16, 0,
				 amf_b9, 0, 0, 0 };
	gen_vec_record(&s, 8, zero16);
	s.k_len = 0;
	gen_vec_record(&s, 8, zero16);
	s.type = OSMO_AUTH_TYPE_NONE;
	gen_vec_record(&s, 8, zero16);
	s.type = 3;
	gen_vec_record(&s, 8, zero16);
	/* XOR-3G AUTS: valid, corrupted, and from a GSM subscriber. It uses rand, not rand_auts. */
	for (int i = 0; i < 8; i++) {
		uint64_t sqn_ms = random_u64(&state) & 0xffffffffffffull;

		random_bytes(&state, key, 16);
		random_bytes(&state, rand, sizeof(rand));
		random_bytes(&state, rand2, sizeof(rand2));
		s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, OSMO_AUTH_ALG_XOR_3G, key, 16, opc, 16,
					 0, amf_b9, 5, 0, 0 };
		xor_make_auts(key, rand, sqn_ms, auts);
		gen_vec_auts_record(&s, 8, auts, rand2, rand);
		if (i == 1) {
			auts[13] ^= 0x01;
			gen_vec_auts_record(&s, 8, auts, rand2, rand);
			auts[13] ^= 0x01;
			auts[0] ^= 0x01;
			gen_vec_auts_record(&s, 8, auts, rand2, rand);
			auts[0] ^= 0x01;
		}
		if (i == 2) {
			s.type = OSMO_AUTH_TYPE_GSM;
			gen_vec_auts_record(&s, 8, auts, rand2, rand);
			s.type = OSMO_AUTH_TYPE_NONE;
			gen_vec_auts_record(&s, 8, auts, rand2, rand);
		}
	}

	/* Algorithms with no implementation or no resync, and XOR-2G with other types. */
	s = (struct subscriber){ OSMO_AUTH_TYPE_UMTS, OSMO_AUTH_ALG_NONE, key, 16, opc, 16, 0,
				 amf0, 0, 0, 0 };
	gen_vec_record(&s, 8, zero16);
	gen_vec_auts_record(&s, 8, auts, zero16, zero16);
	for (int algo = OSMO_AUTH_ALG_COMP128v1; algo <= OSMO_AUTH_ALG_XOR_2G; algo++) {
		if (algo == OSMO_AUTH_ALG_XOR_3G || algo == OSMO_AUTH_ALG_MILENAGE)
			continue;
		s.type = OSMO_AUTH_TYPE_GSM;
		s.algo = algo;
		gen_vec_auts_record(&s, 8, auts, zero16, zero16);
	}
	s.algo = OSMO_AUTH_ALG_XOR_2G;
	s.type = OSMO_AUTH_TYPE_UMTS;
	gen_vec_record(&s, 8, zero16);
	s.type = OSMO_AUTH_TYPE_NONE;
	gen_vec_record(&s, 8, zero16);
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

static void expect_hex(FILE *file, const char *label, const uint8_t *bytes, size_t len)
{
	char expected[256];

	snprintf(expected, sizeof(expected), "%s:\t%s\n", label, osmo_hexdump(bytes, len));
	expect_line(file, expected);
}

/* milenage_test.c, through the deprecated API it uses. */
static void replay_milenage_output(const char *ok_path)
{
	static const uint8_t auts[14] = {
		0x87, 0x11, 0xa0, 0xec, 0x9e, 0x16, 0x37, 0xdf, 0x17, 0xf8, 0x0b, 0x38, 0x4e, 0xe4,
	};
	FILE *file = open_or_fail(ok_path);
	struct osmo_sub_auth_data aud;
	struct osmo_auth_vector vec;
	uint8_t opc[16];
	char expected[128];

	memset(&aud, 0, sizeof(aud));
	aud.type = OSMO_AUTH_TYPE_UMTS;
	aud.algo = OSMO_AUTH_ALG_MILENAGE;
	memcpy(aud.u.umts.opc, counting, 16);
	memcpy(aud.u.umts.k, counting, 16);
	aud.u.umts.sqn = 0x21;
	memset(&vec, 0, sizeof(vec));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	if (osmo_auth_gen_vec(&vec, &aud, zero16) < 0)
		fail("osmo_auth_gen_vec");
	expect_hex(file, "RAND", vec.rand, 16);
	expect_hex(file, "AUTN", vec.autn, 16);
	expect_hex(file, "IK", vec.ik, 16);
	expect_hex(file, "CK", vec.ck, 16);
	expect_hex(file, "RES", vec.res, vec.res_len);
	expect_hex(file, "SRES", vec.sres, 4);
	expect_hex(file, "Kc", vec.kc, 8);
	for (int pass = 0; pass < 4; pass++) {
		int rc;

		if (pass == 1) {
			aud.u.umts.ind_bitlen = 5;
			aud.u.umts.ind = 10;
		}
		if (pass < 2)
			rc = osmo_auth_gen_vec_auts(&vec, &aud, auts, zero16, zero16);
		else
			rc = osmo_auth_gen_vec(&vec, &aud, zero16);
		if (rc < 0)
			fail("milenage_test call");
		snprintf(expected, sizeof(expected), "%stuple generated with SQN = %" PRIu64 "\n",
			 pass < 2 ? "AUTS success: " : "", aud.u.umts.sqn);
		expect_line(file, expected);
	}
#pragma GCC diagnostic pop
	snprintf(expected, sizeof(expected), "MILENAGE supported: %d\n",
		 osmo_auth_supported(osmo_auth_alg_parse("MILENAGE")));
	expect_line(file, expected);
	if (milenage_opc_gen(opc, counting, zero16))
		fail("milenage_opc_gen");
	expect_hex(file, "OP", zero16, 16);
	expect_hex(file, "OPC", opc, 16);
	expect_end(file, ok_path);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s milenage_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *held_text = NULL;
	size_t held_len = 0;

	out = stdout;
	held = open_memstream(&held_text, &held_len);
	if (!held)
		fail("open_memstream");
	replay_milenage_output(argv[1]);
	fprintf(out, "{\"schema\":\"auth-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	names();
	conversions();
	milenage_functions();
	vectors();
	fclose(held);
	fputs(held_text, stdout);
	free(held_text);
	return EXIT_SUCCESS;
}
