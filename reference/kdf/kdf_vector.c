/*
 * Reference harness for libosmogsm's key derivation functions: Kc128 (3GPP TS 33.102),
 * and K_ASME, K_eNB, NH and the NAS algorithm keys (3GPP TS 33.401), each an
 * HMAC-SHA-256 over the parameters 3GPP TS 33.220 lays out.
 *
 * libosmocore has no test for these functions, so the harness calls each with fixed
 * patterns, the edges of its integer parameters and pseudo-random inputs. It writes
 * NDJSON: a metadata line, then one record per call, each kind in one section.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/crypt/kdf.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55
#define GUARD_BYTES 4
#define RANDOM_RECORDS 16

static const uint8_t zero32[32];
static const uint8_t counting[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};
static const uint8_t ones32[32] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
/* MCC 001, MNC 01, and MCC 262, MNC 42, as 3GPP TS 24.008 encodes a PLMN identity. */
static const uint8_t test_plmn[3] = { 0x00, 0xf1, 0x10 };
static const uint8_t german_plmn[3] = { 0x62, 0xf2, 0x24 };

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

static void hex(const char *key, const uint8_t *bytes, size_t len)
{
	printf(",\"%s\":\"", key);
	for (size_t i = 0; i < len; i++)
		printf("%02x", bytes[i]);
	printf("\"");
}

/* The output, then the guard bytes after it, checked untouched. */
static void written(const char *key, const uint8_t *buf, size_t len)
{
	for (size_t i = len; i < len + GUARD_BYTES; i++)
		if (buf[i] != GUARD)
			fail("a KDF function wrote past its output");
	hex(key, buf, len + GUARD_BYTES);
}

static void kc128_record(const char *source, const uint8_t *ck, const uint8_t *ik)
{
	uint8_t kc128[16 + GUARD_BYTES];

	memset(kc128, GUARD, sizeof(kc128));
	osmo_kdf_kc128(ck, ik, kc128);
	printf("{\"kind\":\"kc128\",\"source\":\"%s\"", source);
	hex("ck", ck, 16);
	hex("ik", ik, 16);
	written("kc128", kc128, 16);
	printf("}\n");
}

static void kc128s(void)
{
	uint32_t state = 0x4c31;
	uint8_t ck[16], ik[16];

	kc128_record("pattern", zero32, zero32);
	kc128_record("pattern", ones32, ones32);
	kc128_record("pattern", counting, counting + 16);
	/* Swapping CK and IK changes the HMAC key. */
	kc128_record("pattern", counting + 16, counting);
	for (int i = 0; i < RANDOM_RECORDS; i++) {
		random_bytes(&state, ck, sizeof(ck));
		random_bytes(&state, ik, sizeof(ik));
		kc128_record("random", ck, ik);
	}
}

static void kasme_record(const char *source, const uint8_t *ck, const uint8_t *ik,
			 const uint8_t *plmn_id, const uint8_t *sqn, const uint8_t *ak)
{
	uint8_t kasme[32 + GUARD_BYTES];

	memset(kasme, GUARD, sizeof(kasme));
	osmo_kdf_kasme(ck, ik, plmn_id, sqn, ak, kasme);
	printf("{\"kind\":\"kasme\",\"source\":\"%s\"", source);
	hex("ck", ck, 16);
	hex("ik", ik, 16);
	hex("plmn_id", plmn_id, 3);
	hex("sqn", sqn, 6);
	hex("ak", ak, 6);
	written("kasme", kasme, 32);
	printf("}\n");
}

static void kasmes(void)
{
	static const uint8_t sqn[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x21 };
	static const uint8_t ak[6] = { 0xa5, 0x5a, 0x0f, 0xf0, 0x3c, 0xc3 };
	uint8_t concealed[6];
	uint32_t state = 0x4a53;
	uint8_t ck[16], ik[16], plmn_id[3], sqn_r[6], ak_r[6];

	kasme_record("pattern", zero32, zero32, zero32, zero32, zero32);
	kasme_record("pattern", ones32, ones32, ones32, ones32, ones32);
	kasme_record("pattern", counting, counting + 16, test_plmn, sqn, ak);
	kasme_record("pattern", counting, counting + 16, german_plmn, sqn, ak);
	/* Only SQN xor AK enters the derivation: these three conceal the same value. */
	for (int i = 0; i < 6; i++)
		concealed[i] = sqn[i] ^ ak[i];
	kasme_record("pattern", counting, counting + 16, test_plmn, concealed, zero32);
	kasme_record("pattern", counting, counting + 16, test_plmn, zero32, concealed);
	kasme_record("pattern", counting, counting + 16, test_plmn, ak, sqn);
	/* SQN and AK equal conceal zero. */
	kasme_record("pattern", counting, counting + 16, test_plmn, ak, ak);
	for (int i = 0; i < RANDOM_RECORDS; i++) {
		random_bytes(&state, ck, sizeof(ck));
		random_bytes(&state, ik, sizeof(ik));
		random_bytes(&state, plmn_id, sizeof(plmn_id));
		random_bytes(&state, sqn_r, sizeof(sqn_r));
		random_bytes(&state, ak_r, sizeof(ak_r));
		kasme_record("random", ck, ik, plmn_id, sqn_r, ak_r);
	}
}

static void enb_record(const char *source, const uint8_t *kasme, uint32_t ul_count)
{
	uint8_t kenb[32 + GUARD_BYTES];

	memset(kenb, GUARD, sizeof(kenb));
	osmo_kdf_enb(kasme, ul_count, kenb);
	printf("{\"kind\":\"enb\",\"source\":\"%s\"", source);
	hex("kasme", kasme, 32);
	printf(",\"ul_count\":%u", ul_count);
	written("kenb", kenb, 32);
	printf("}\n");
}

static void enbs(void)
{
	static const uint32_t counts[] = {
		0, 1, 2, 0xff, 0x100, 0xffff, 0x10000, 0x00ffffff, 0x01000000,
		0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff, 0x01020304,
	};
	uint32_t state = 0x654e;
	uint8_t kasme[32];

	for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
		enb_record("pattern", counting, counts[i]);
	enb_record("pattern", zero32, 0);
	enb_record("pattern", ones32, 0xffffffff);
	for (int i = 0; i < RANDOM_RECORDS; i++) {
		random_bytes(&state, kasme, sizeof(kasme));
		enb_record("random", kasme, next_random(&state));
	}
}

static void nh_record(const char *source, const uint8_t *kasme, const uint8_t *sync_input)
{
	uint8_t nh[32 + GUARD_BYTES];

	memset(nh, GUARD, sizeof(nh));
	osmo_kdf_nh(kasme, sync_input, nh);
	printf("{\"kind\":\"nh\",\"source\":\"%s\"", source);
	hex("kasme", kasme, 32);
	hex("sync_input", sync_input, 32);
	written("nh", nh, 32);
	printf("}\n");
}

static void nhs(void)
{
	uint32_t state = 0x4e48;
	uint8_t kasme[32], sync_input[32], kenb[32 + GUARD_BYTES], nh[32 + GUARD_BYTES];

	nh_record("pattern", zero32, zero32);
	nh_record("pattern", ones32, ones32);
	nh_record("pattern", counting, zero32);
	nh_record("pattern", zero32, counting);
	/* A chain as TS 33.401 section 7.2.8 describes: the first NH from K_eNB, each next
	 * from the NH before it. */
	osmo_kdf_enb(counting, 0, kenb);
	nh_record("chain", counting, kenb);
	memcpy(sync_input, kenb, 32);
	for (int i = 0; i < 3; i++) {
		memset(nh, GUARD, sizeof(nh));
		osmo_kdf_nh(counting, sync_input, nh);
		nh_record("chain", counting, nh);
		memcpy(sync_input, nh, 32);
	}
	for (int i = 0; i < RANDOM_RECORDS; i++) {
		random_bytes(&state, kasme, sizeof(kasme));
		random_bytes(&state, sync_input, sizeof(sync_input));
		nh_record("random", kasme, sync_input);
	}
}

static void nas_record(const char *source, uint8_t algo_type, uint8_t algo_id, const uint8_t *kasme)
{
	uint8_t knas[16 + GUARD_BYTES];

	memset(knas, GUARD, sizeof(knas));
	osmo_kdf_nas(algo_type, algo_id, kasme, knas);
	printf("{\"kind\":\"nas\",\"source\":\"%s\",\"algo_type\":%u,\"algo_id\":%u", source,
	       algo_type, algo_id);
	hex("kasme", kasme, 32);
	written("knas", knas, 16);
	printf("}\n");
}

static void nass(void)
{
	static const uint8_t edges[] = { 0x00, 0x0f, 0x10, 0x7f, 0x80, 0xff };
	uint32_t state = 0x4e41;
	uint8_t kasme[32];

	/* TS 33.401 defines types 1 to 6 and the algorithm identities 0 to 7 (EEA and EIA),
	 * in four bits. C takes any byte. */
	for (uint8_t type = 0; type <= 7; type++)
		for (uint8_t id = 0; id <= 15; id++)
			nas_record("sweep", type, id, counting);
	for (size_t i = 0; i < sizeof(edges); i++) {
		nas_record("edge", edges[i], 1, counting);
		nas_record("edge", 1, edges[i], counting);
	}
	nas_record("pattern", 1, 1, zero32);
	nas_record("pattern", 2, 2, ones32);
	for (int i = 0; i < RANDOM_RECORDS; i++) {
		random_bytes(&state, kasme, sizeof(kasme));
		nas_record("random", (uint8_t)(1 + next_random(&state) % 2),
			   (uint8_t)(next_random(&state) % 8), kasme);
	}
}

int main(int argc, char **argv)
{
	(void)argv;
	if (argc != 1) {
		fprintf(stderr, "usage: kdf_vector\n");
		return EXIT_FAILURE;
	}
	printf("{\"schema\":\"kdf-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	kc128s();
	kasmes();
	enbs();
	nhs();
	nass();
	return EXIT_SUCCESS;
}
