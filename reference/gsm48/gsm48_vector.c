/*
 * Reference harness for libosmogsm's 3GPP TS 24.008 identity encodings: the PLMN in BCD,
 * the Location Area and Routing Area Identifications, and the Mobile Identity, with their
 * legacy forms, names, comparisons and the Complete Layer 3 messages that carry one.
 *
 * Calls the API directly and writes NDJSON: a metadata line, the constants, then one
 * section per kind of record. The first argument is upstream's gsm0408_test.ok. The parts
 * of upstream's test that cover these identities are replayed with the same calls, and the
 * run fails unless their output matches those parts of that file byte for byte.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <talloc.h>

#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm23003.h>
#include <osmocom/gsm/gsm48.h>
#include <osmocom/gsm/protocol/gsm_04_08.h>
#include <osmocom/gsm/protocol/gsm_23_003.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55
/* Larger than any encoding or name, so the bytes past each one show it wrote no further. */
#define OUT_SIZE 96
/* How many bytes of an output buffer a record holds: past every encoding, into the guard. */
#define OUT_RECORDED 12
#define JUNK_U16 4242
#define JUNK_INT -4242

enum section {
	TO_BCD, FROM_BCD, LAI_ENCODE, LAI_DECODE, RA_ENCODE, RA_DECODE, RA_EQUAL,
	RAI_NAME, RAI_NAME_BUF, MI_TYPE_NAME, MI_BYTES, MI_TO_STRING, MI_NAME_BUF, GENERATE_MID,
	GENERATE_MID_FROM_TMSI, MI_ENCODE, MI_STR, MI_CMP, L3, SECTION_COUNT
};

static const char *const kind_names[SECTION_COUNT] = {
	"to_bcd",
	"from_bcd",
	"lai_encode",
	"lai_decode",
	"ra_encode",
	"ra_decode",
	"ra_equal",
	"rai_name",
	"rai_name_buf",
	"mi_type_name",
	"mi_bytes",
	"mi_to_string",
	"mi_name_buf",
	"generate_mid",
	"generate_mid_from_tmsi",
	"mi_encode",
	"mi_str",
	"mi_cmp",
	"l3",
};

static FILE *sections[SECTION_COUNT];
static char *section_text[SECTION_COUNT];
static size_t section_size[SECTION_COUNT];
/* Upstream's printed output, compared with parts of gsm0408_test.ok at the end. */
static FILE *upstream_out;
static char *upstream_text;
static size_t upstream_size;
static const char *source = "sweep";
static void *ctx;

static void fail(const char *what)
{
	fprintf(stderr, "reference expectation failed: %s\n", what);
	exit(EXIT_FAILURE);
}

#define CHECK(condition) do { if (!(condition)) fail(#condition); } while (0)

static void hex_bytes(FILE *f, const char *key, const void *data, size_t length)
{
	fprintf(f, "\"%s\":\"", key);
	for (size_t i = 0; i < length; i++)
		fprintf(f, "%02x", ((const unsigned char *)data)[i]);
	fprintf(f, "\"");
}

static void hex_text(FILE *f, const char *key, const char *text)
{
	if (text)
		hex_bytes(f, key, text, strlen(text));
	else
		fprintf(f, "\"%s\":null", key);
}

static void start(enum section section)
{
	fprintf(sections[section], "{\"kind\":\"%s\",\"source\":\"%s\"", kind_names[section], source);
}

static void guard_untouched(const void *buf, size_t from)
{
	for (size_t i = from; i < OUT_SIZE; i++)
		CHECK(((const unsigned char *)buf)[i] == GUARD);
}

static size_t parse_hex(const char *hex, uint8_t *out, size_t out_size)
{
	int len = osmo_hexparse(hex, out, out_size);
	CHECK(len >= 0);
	return len;
}

/* ---- Identity fields ---- */

static struct osmo_plmn_id plmn(uint16_t mcc, uint16_t mnc, bool mnc_3_digits)
{
	return (struct osmo_plmn_id){ .mcc = mcc, .mnc = mnc, .mnc_3_digits = mnc_3_digits };
}

static void plmn_fields(FILE *f, const char *prefix, const struct osmo_plmn_id *p)
{
	fprintf(f, ",\"%smcc\":%u,\"%smnc\":%u,\"%smnc_3_digits\":%u", prefix, p->mcc, prefix, p->mnc,
		prefix, p->mnc_3_digits);
}

static void lai_fields(FILE *f, const char *prefix, const struct osmo_location_area_id *l)
{
	plmn_fields(f, prefix, &l->plmn);
	fprintf(f, ",\"%slac\":%u", prefix, l->lac);
}

static void rai_fields(FILE *f, const char *prefix, const struct osmo_routing_area_id *r)
{
	lai_fields(f, prefix, &r->lac);
	fprintf(f, ",\"%srac\":%u", prefix, r->rac);
}

static void gprs_ra_fields(FILE *f, const char *prefix, const struct gprs_ra_id *r)
{
	fprintf(f, ",\"%smcc\":%u,\"%smnc\":%u,\"%smnc_3_digits\":%u,\"%slac\":%u,\"%srac\":%u", prefix,
		r->mcc, prefix, r->mnc, prefix, r->mnc_3_digits, prefix, r->lac, prefix, r->rac);
}

static struct gprs_ra_id gprs_ra(const struct osmo_routing_area_id *r)
{
	return (struct gprs_ra_id){
		.mcc = r->lac.plmn.mcc,
		.mnc = r->lac.plmn.mnc,
		.mnc_3_digits = r->lac.plmn.mnc_3_digits,
		.lac = r->lac.lac,
		.rac = r->rac,
	};
}

static bool same_plmn(const struct osmo_plmn_id *a, const struct osmo_plmn_id *b)
{
	return a->mcc == b->mcc && a->mnc == b->mnc && a->mnc_3_digits == b->mnc_3_digits;
}

/* The decoded identity: its type, the union read as a TMSI, and for a type with digits, the
 * digits. Every other union byte is left out, since C doesn't define it in every state. */
static void mi_fields(FILE *f, const char *prefix, const struct osmo_mobile_identity *mi)
{
	fprintf(f, ",\"%stype\":%u,\"%stmsi\":%" PRIu32 ",", prefix, mi->type, prefix, mi->tmsi);
	switch (mi->type) {
	case GSM_MI_TYPE_IMSI:
	case GSM_MI_TYPE_IMEI:
	case GSM_MI_TYPE_IMEISV:
		CHECK(memchr(mi->imeisv, 0, sizeof(mi->imeisv)));
		fprintf(f, "\"%sdigits\":\"%s\"", prefix, mi->imeisv);
		break;
	default:
		fprintf(f, "\"%sdigits\":null", prefix);
		break;
	}
}

/* An identity to encode: the digits go in the union whatever the type, as a caller's would. */
static struct osmo_mobile_identity mi_struct(uint8_t type, const char *digits, uint32_t tmsi)
{
	struct osmo_mobile_identity mi;

	memset(&mi, 0, sizeof(mi));
	mi.type = type;
	if (type == GSM_MI_TYPE_TMSI) {
		mi.tmsi = tmsi;
	} else {
		CHECK(strlen(digits) < sizeof(mi.imeisv));
		memcpy(mi.imeisv, digits, strlen(digits));
	}
	return mi;
}

static void mi_input_fields(FILE *f, const char *prefix, const struct osmo_mobile_identity *mi)
{
	if (!mi) {
		fprintf(f, ",\"%snull\":1", prefix);
		return;
	}
	fprintf(f, ",\"%snull\":0,\"%stype\":%u,", prefix, prefix, mi->type);
	if (mi->type == GSM_MI_TYPE_TMSI)
		fprintf(f, "\"%stmsi\":%" PRIu32 ",\"%sdigits\":null", prefix, mi->tmsi, prefix);
	else
		fprintf(f, "\"%stmsi\":null,\"%sdigits\":\"%s\"", prefix, prefix, mi->imeisv);
}

/* ---- PLMN in BCD ---- */

static const uint16_t sweep_mccs[] = { 0, 1, 9, 10, 12, 77, 99, 100, 123, 262, 901, 999, 1000, 1234, 65535 };
static const uint16_t sweep_mncs[] = { 0, 1, 2, 9, 10, 34, 99, 100, 121, 456, 999, 1000, 65535 };

static void record_to_bcd(const struct osmo_plmn_id *p)
{
	uint8_t out[OUT_SIZE], legacy[OUT_SIZE];
	FILE *f = sections[TO_BCD];

	memset(out, GUARD, sizeof(out));
	memset(legacy, GUARD, sizeof(legacy));
	osmo_plmn_to_bcd(out, p);
	gsm48_mcc_mnc_to_bcd(legacy, p->mcc, p->mnc);
	guard_untouched(out, 3);
	guard_untouched(legacy, 3);
	start(TO_BCD);
	plmn_fields(f, "", p);
	fprintf(f, ",");
	hex_bytes(f, "bcd", out, 3);
	fprintf(f, ",");
	hex_bytes(f, "legacy_bcd", legacy, 3);
	fprintf(f, "}\n");
}

static void record_from_bcd(const uint8_t bcd[3])
{
	struct osmo_plmn_id p = plmn(JUNK_U16, JUNK_U16, true);
	uint16_t legacy_mcc = JUNK_U16, legacy_mnc = JUNK_U16;
	struct gprs_ra_id parsed = { .mcc = JUNK_U16, .mnc = JUNK_U16, .mnc_3_digits = true };
	uint8_t copy[3], ra[6];
	FILE *f = sections[FROM_BCD];

	memcpy(copy, bcd, 3);
	osmo_plmn_from_bcd(bcd, &p);
	gsm48_mcc_mnc_from_bcd(copy, &legacy_mcc, &legacy_mnc);
	CHECK(!memcmp(copy, bcd, 3));
	memcpy(ra, bcd, 3);
	memset(ra + 3, 0, 3);
	gsm48_parse_ra(&parsed, ra);
	start(FROM_BCD);
	fprintf(f, ",");
	hex_bytes(f, "bcd", bcd, 3);
	plmn_fields(f, "", &p);
	fprintf(f, ",\"legacy_mcc\":%u,\"legacy_mnc\":%u,\"parse_ra_mcc\":%u,\"parse_ra_mnc\":%u,"
		"\"parse_ra_mnc_3_digits\":%u}\n", legacy_mcc, legacy_mnc, parsed.mcc, parsed.mnc,
		parsed.mnc_3_digits);
}

static void sweep_plmn_bcd(void)
{
	static const uint8_t nibbles[] = { 0x0, 0x9, 0xa, 0xf };
	uint8_t bcd[3];

	for (size_t i = 0; i < ARRAY_SIZE(sweep_mccs); i++)
		for (size_t j = 0; j < ARRAY_SIZE(sweep_mncs); j++)
			for (int three = 0; three <= 1; three++) {
				struct osmo_plmn_id p = plmn(sweep_mccs[i], sweep_mncs[j], three);
				record_to_bcd(&p);
			}

	/* Every combination of a digit, the highest digit, the lowest hex and the filler. */
	for (unsigned int n = 0; n < 4096; n++) {
		for (int byte = 0; byte < 3; byte++)
			bcd[byte] = nibbles[(n >> (byte * 4)) & 3] | nibbles[(n >> (byte * 4 + 2)) & 3] << 4;
		record_from_bcd(bcd);
	}
	/* Every value of each byte, with the others valid. */
	for (unsigned int v = 0; v < 256; v++) {
		const uint8_t b0[3] = { v, 0xf3, 0x54 };
		const uint8_t b1[3] = { 0x21, v, 0x54 };
		const uint8_t b2_two[3] = { 0x21, 0xf3, v };
		const uint8_t b2_three[3] = { 0x21, 0x63, v };
		record_from_bcd(b0);
		record_from_bcd(b1);
		record_from_bcd(b2_two);
		record_from_bcd(b2_three);
	}
}

/* ---- Location Area Identification ---- */

static const uint16_t area_mccs[] = { 0, 1, 12, 77, 99, 100, 123, 999, 1000, 65535 };
static const uint16_t area_mncs[] = { 0, 2, 34, 99, 100, 456, 999, 1000 };

static void record_lai_encode(const struct osmo_location_area_id *l)
{
	uint8_t out[OUT_SIZE], legacy[OUT_SIZE];
	FILE *f = sections[LAI_ENCODE];

	memset(out, GUARD, sizeof(out));
	memset(legacy, GUARD, sizeof(legacy));
	gsm48_generate_lai2((struct gsm48_loc_area_id *)out, l);
	gsm48_generate_lai((struct gsm48_loc_area_id *)legacy, l->plmn.mcc, l->plmn.mnc, l->lac);
	guard_untouched(out, sizeof(struct gsm48_loc_area_id));
	guard_untouched(legacy, sizeof(struct gsm48_loc_area_id));
	start(LAI_ENCODE);
	lai_fields(f, "", l);
	fprintf(f, ",");
	hex_bytes(f, "bytes", out, sizeof(struct gsm48_loc_area_id));
	fprintf(f, ",");
	hex_bytes(f, "legacy_bytes", legacy, sizeof(struct gsm48_loc_area_id));
	fprintf(f, "}\n");
}

static void record_lai_decode(const uint8_t bytes[5])
{
	struct gsm48_loc_area_id in;
	struct osmo_location_area_id decoded = { .plmn = plmn(JUNK_U16, JUNK_U16, true), .lac = JUNK_U16 };
	struct osmo_plmn_id from_bcd;
	uint16_t mcc = JUNK_U16, mnc = JUNK_U16, lac = JUNK_U16;
	int rc;
	FILE *f = sections[LAI_DECODE];

	memcpy(&in, bytes, sizeof(in));
	gsm48_decode_lai2(&in, &decoded);
	rc = gsm48_decode_lai(&in, &mcc, &mnc, &lac);
	CHECK(!memcmp(&in, bytes, sizeof(in)));
	osmo_plmn_from_bcd(bytes, &from_bcd);
	CHECK(same_plmn(&decoded.plmn, &from_bcd));
	start(LAI_DECODE);
	fprintf(f, ",");
	hex_bytes(f, "bytes", bytes, 5);
	lai_fields(f, "", &decoded);
	fprintf(f, ",\"legacy_rc\":%d,\"legacy_mcc\":%u,\"legacy_mnc\":%u,\"legacy_lac\":%u}\n", rc, mcc,
		mnc, lac);
}

static const char *const area_bcds[] = {
	"00f000", "000000", "00f120", "002100", "10f243", "104230", "216354", "999999",
	"ffffff", "aaaaaa", "21f3a4", "2a6354",
};
static const char *const area_codes[] = { "0000", "0001", "1234", "fffe", "ffff" };

static void sweep_lai(void)
{
	static const uint16_t lacs[] = { 0, 1, 0x1234, 65535 };
	uint8_t bytes[5];

	for (size_t i = 0; i < ARRAY_SIZE(area_mccs); i++)
		for (size_t j = 0; j < ARRAY_SIZE(area_mncs); j++)
			for (int three = 0; three <= 1; three++)
				for (size_t k = 0; k < ARRAY_SIZE(lacs); k++) {
					struct osmo_location_area_id l = {
						.plmn = plmn(area_mccs[i], area_mncs[j], three),
						.lac = lacs[k],
					};
					record_lai_encode(&l);
				}
	for (size_t i = 0; i < ARRAY_SIZE(area_bcds); i++)
		for (size_t k = 0; k < ARRAY_SIZE(area_codes); k++) {
			CHECK(parse_hex(area_bcds[i], bytes, 3) == 3);
			CHECK(parse_hex(area_codes[k], bytes + 3, 2) == 2);
			record_lai_decode(bytes);
		}
}

/* ---- Routing Area Identification ---- */

static void record_ra_encode(const struct osmo_routing_area_id *r, size_t buflen)
{
	uint8_t out[OUT_SIZE], legacy[OUT_SIZE], constructed[OUT_SIZE];
	struct gprs_ra_id g = gprs_ra(r), converted;
	struct osmo_routing_area_id back;
	int rc, constructed_rc;
	FILE *f = sections[RA_ENCODE];

	memset(out, GUARD, sizeof(out));
	memset(legacy, GUARD, sizeof(legacy));
	memset(constructed, GUARD, sizeof(constructed));
	rc = osmo_routing_area_id_encode_buf(out, buflen, r);
	gsm48_encode_ra((struct gsm48_ra_id *)legacy, &g);
	constructed_rc = gsm48_construct_ra(constructed, &g);
	guard_untouched(out, rc > 0 ? (size_t)rc : 0);
	guard_untouched(legacy, sizeof(struct gsm48_ra_id));
	guard_untouched(constructed, sizeof(struct gsm48_ra_id));
	CHECK(!memcmp(legacy, constructed, sizeof(struct gsm48_ra_id)));
	/* The conversions between the two structs copy every field. */
	osmo_rai_to_gprs(&converted, r);
	CHECK(converted.mcc == g.mcc && converted.mnc == g.mnc && converted.mnc_3_digits == g.mnc_3_digits
	      && converted.lac == g.lac && converted.rac == g.rac);
	gprs_rai_to_osmo(&back, &converted);
	CHECK(!osmo_rai_cmp(&back, r) && back.lac.plmn.mnc_3_digits == r->lac.plmn.mnc_3_digits);
	start(RA_ENCODE);
	rai_fields(f, "", r);
	fprintf(f, ",\"buflen\":%zu,\"rc\":%d,", buflen, rc);
	hex_bytes(f, "out", out, OUT_RECORDED);
	fprintf(f, ",");
	hex_bytes(f, "legacy_bytes", legacy, sizeof(struct gsm48_ra_id));
	fprintf(f, ",\"construct_rc\":%d}\n", constructed_rc);
}

static void record_ra_decode(const uint8_t *bytes, size_t len)
{
	uint8_t in[OUT_SIZE];
	struct osmo_routing_area_id decoded = {
		.lac = { .plmn = plmn(JUNK_U16, JUNK_U16, true), .lac = JUNK_U16 },
		.rac = 0xa5,
	};
	struct gprs_ra_id parsed = { .mcc = JUNK_U16, .mnc = JUNK_U16, .mnc_3_digits = true,
				     .lac = JUNK_U16, .rac = 0xa5 };
	int rc;
	FILE *f = sections[RA_DECODE];

	memset(in, GUARD, sizeof(in));
	memcpy(in, bytes, len);
	rc = osmo_routing_area_id_decode(&decoded, in, len);
	start(RA_DECODE);
	fprintf(f, ",");
	hex_bytes(f, "bytes", bytes, len);
	fprintf(f, ",\"rc\":%d", rc);
	rai_fields(f, "", &decoded);
	if (len >= sizeof(struct gsm48_ra_id)) {
		struct osmo_plmn_id from_bcd;

		gsm48_parse_ra(&parsed, in);
		osmo_plmn_from_bcd(in, &from_bcd);
		CHECK(rc != 6 || same_plmn(&decoded.lac.plmn, &from_bcd));
		gprs_ra_fields(f, "parse_ra_", &parsed);
	}
	fprintf(f, "}\n");
}

static void record_ra_equal(const struct gprs_ra_id *a, const struct gprs_ra_id *b)
{
	FILE *f = sections[RA_EQUAL];

	start(RA_EQUAL);
	gprs_ra_fields(f, "a_", a);
	gprs_ra_fields(f, "b_", b);
	fprintf(f, ",\"result\":%u}\n", gsm48_ra_equal(a, b));
}

static void sweep_ra(void)
{
	static const uint16_t lacs[] = { 0, 0x1234, 65535 };
	static const uint8_t racs[] = { 0, 5, 255 };
	static const struct gprs_ra_id equal_items[] = {
		{ .mcc = 262, .mnc = 42, .lac = 0x1234, .rac = 5 },
		{ .mcc = 263, .mnc = 42, .lac = 0x1234, .rac = 5 },
		{ .mcc = 262, .mnc = 43, .lac = 0x1234, .rac = 5 },
		{ .mcc = 262, .mnc = 42, .mnc_3_digits = true, .lac = 0x1234, .rac = 5 },
		{ .mcc = 262, .mnc = 42, .lac = 0x1235, .rac = 5 },
		{ .mcc = 262, .mnc = 42, .lac = 0x1234, .rac = 6 },
		{ .mcc = 262, .mnc = 420, .lac = 0x1234, .rac = 5 },
		{ .mcc = 262, .mnc = 420, .mnc_3_digits = true, .lac = 0x1234, .rac = 5 },
	};
	uint8_t bytes[8];

	for (size_t i = 0; i < ARRAY_SIZE(area_mccs); i++)
		for (size_t j = 0; j < ARRAY_SIZE(area_mncs); j++)
			for (int three = 0; three <= 1; three++)
				for (size_t k = 0; k < ARRAY_SIZE(lacs); k++) {
					struct osmo_routing_area_id r = {
						.lac = { .plmn = plmn(area_mccs[i], area_mncs[j], three), .lac = lacs[k] },
						.rac = racs[k],
					};
					record_ra_encode(&r, 6);
				}
	for (size_t buflen = 0; buflen <= 8; buflen++) {
		struct osmo_routing_area_id r = { .lac = { .plmn = plmn(1, 2, true), .lac = 23 }, .rac = 42 };
		record_ra_encode(&r, buflen);
	}

	for (size_t i = 0; i < ARRAY_SIZE(area_bcds); i++)
		for (size_t k = 0; k < ARRAY_SIZE(area_codes); k++) {
			CHECK(parse_hex(area_bcds[i], bytes, 3) == 3);
			CHECK(parse_hex(area_codes[k], bytes + 3, 2) == 2);
			bytes[5] = 0x80 + k;
			record_ra_decode(bytes, 6);
		}
	CHECK(parse_hex("00f12000172aff", bytes, sizeof(bytes)) == 7);
	for (size_t len = 0; len <= 7; len++)
		record_ra_decode(bytes, len);

	for (size_t i = 0; i < ARRAY_SIZE(equal_items); i++)
		for (size_t j = 0; j < ARRAY_SIZE(equal_items); j++)
			record_ra_equal(&equal_items[i], &equal_items[j]);
}

/* ---- Routing area names ---- */

static void record_rai_name(const struct gprs_ra_id *r)
{
	char buf[OUT_SIZE];
	const char *text = osmo_rai_name(r);
	char *c = osmo_rai_name_c(ctx, r);
	FILE *f = sections[RAI_NAME];

	memset(buf, GUARD, sizeof(buf));
	CHECK(osmo_rai_name_buf(buf, sizeof(buf), r) == buf);
	guard_untouched(buf, strlen(buf) + 1);
	CHECK(!strcmp(text, buf));
	CHECK(c && !strcmp(c, buf));
	talloc_free(c);
	start(RAI_NAME);
	gprs_ra_fields(f, "", r);
	fprintf(f, ",\"text\":\"%s\"}\n", buf);
}

static void record_rai_name_buf(const struct gprs_ra_id *r, size_t buf_len)
{
	char buf[OUT_SIZE];
	FILE *f = sections[RAI_NAME_BUF];

	memset(buf, GUARD, sizeof(buf));
	CHECK(osmo_rai_name_buf(buf, buf_len, r) == buf);
	start(RAI_NAME_BUF);
	gprs_ra_fields(f, "", r);
	fprintf(f, ",\"buf_len\":%zu,", buf_len);
	hex_bytes(f, "out", buf, buf_len + 1);
	fprintf(f, "}\n");
	guard_untouched(buf, buf_len);
}

static void sweep_rai_names(void)
{
	static const uint16_t lacs[] = { 0, 0x1234, 65535 };
	static const uint8_t racs[] = { 0, 5, 255 };
	static const struct gprs_ra_id buf_items[] = {
		{ .mcc = 1, .mnc = 2, .lac = 23, .rac = 42 },
		{ .mcc = 1, .mnc = 2, .mnc_3_digits = true, .lac = 23, .rac = 42 },
		{ .mcc = 65535, .mnc = 65535, .lac = 65535, .rac = 255 },
	};

	for (size_t i = 0; i < ARRAY_SIZE(area_mccs); i++)
		for (size_t j = 0; j < ARRAY_SIZE(area_mncs); j++)
			for (int three = 0; three <= 1; three++)
				for (size_t k = 0; k < ARRAY_SIZE(lacs); k++) {
					struct gprs_ra_id r = {
						.mcc = area_mccs[i], .mnc = area_mncs[j], .mnc_3_digits = three,
						.lac = lacs[k], .rac = racs[k],
					};
					record_rai_name(&r);
				}
	for (size_t i = 0; i < ARRAY_SIZE(buf_items); i++) {
		size_t len = strlen(osmo_rai_name(&buf_items[i]));
		for (size_t buf_len = 0; buf_len <= len + 1; buf_len++)
			record_rai_name_buf(&buf_items[i], buf_len);
	}
}

/* ---- Mobile identity type names ---- */

static void sweep_mi_type_names(void)
{
	for (unsigned int type = 0; type < 256; type++) {
		start(MI_TYPE_NAME);
		fprintf(sections[MI_TYPE_NAME], ",\"type\":%u,\"text\":\"%s\"}\n", type,
			gsm48_mi_type_name(type));
	}
}

/* ---- Encoded mobile identities ---- */

static void record_mi_decode(FILE *f, const char *prefix, const uint8_t *mi_data, uint8_t mi_len,
			     bool allow_hex)
{
	struct osmo_mobile_identity mi;
	int rc;

	memset(&mi, 0xff, sizeof(mi));
	rc = osmo_mobile_identity_decode(&mi, mi_data, mi_len, allow_hex);
	fprintf(f, ",\"%src\":%d", prefix, rc);
	mi_fields(f, prefix, &mi);
}

/* One encoded identity through every function that reads one: the name, the legacy string
 * and the decoder, with and without hex digits. */
static void record_mi_bytes(const uint8_t *mi_data, uint8_t mi_len)
{
	char name[OUT_SIZE], str[OUT_SIZE];
	char *c = osmo_mi_name_c(ctx, mi_data, mi_len);
	const char *text = osmo_mi_name(mi_data, mi_len);
	int rc;
	FILE *f = sections[MI_BYTES];

	memset(name, GUARD, sizeof(name));
	CHECK(osmo_mi_name_buf(name, sizeof(name), mi_data, mi_len) == name);
	guard_untouched(name, strlen(name) + 1);
	CHECK(!strcmp(text, name));
	CHECK(c && !strcmp(c, name));
	talloc_free(c);
	memset(str, GUARD, sizeof(str));
	rc = gsm48_mi_to_string(str, 64, mi_data, mi_len);
	guard_untouched(str, strlen(str) + 1);

	start(MI_BYTES);
	if (mi_data) {
		fprintf(f, ",\"null\":0,");
		hex_bytes(f, "bytes", mi_data, mi_len);
	} else {
		fprintf(f, ",\"null\":1,\"len\":%u", mi_len);
	}
	fprintf(f, ",\"name\":\"%s\",\"to_string_rc\":%d,", name, rc);
	hex_text(f, "to_string", str);
	record_mi_decode(f, "", mi_data, mi_len, false);
	record_mi_decode(f, "hex_", mi_data, mi_len, true);
	fprintf(f, "}\n");
}

static void sweep_mi_bytes(void)
{
	static const uint8_t high_nibbles[] = { 0x0, 0x1, 0x9, 0xa, 0xf };
	static const uint8_t lengths[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 20 };
	static const uint8_t digits[] = { 0x21, 0x43, 0x65, 0x87, 0x09 };
	uint8_t mi[32];

	record_mi_bytes(NULL, 0);
	record_mi_bytes(NULL, 5);
	mi[0] = 0x19;
	record_mi_bytes(mi, 0);
	for (unsigned int type = 0; type < 16; type++)
		for (size_t h = 0; h < ARRAY_SIZE(high_nibbles); h++)
			for (size_t l = 0; l < ARRAY_SIZE(lengths); l++)
				for (int body = 0; body < (lengths[l] > 1 ? 3 : 1); body++) {
					uint8_t len = lengths[l];

					mi[0] = high_nibbles[h] << 4 | type;
					for (uint8_t i = 1; i < len; i++)
						mi[i] = digits[(i - 1) % ARRAY_SIZE(digits)];
					/* The last digit is the filler, or a hex digit is in the middle. */
					if (body == 1)
						mi[len - 1] |= 0xf0;
					else if (body == 2)
						mi[1] = 0x2a;
					record_mi_bytes(mi, len);
				}
}

static void record_mi_to_string(const uint8_t *mi_data, uint8_t mi_len, int str_len)
{
	char str[OUT_SIZE];
	int rc;
	FILE *f = sections[MI_TO_STRING];

	memset(str, GUARD, sizeof(str));
	rc = gsm48_mi_to_string(str, str_len, mi_data, mi_len);
	guard_untouched(str, str_len);
	start(MI_TO_STRING);
	fprintf(f, ",");
	hex_bytes(f, "bytes", mi_data, mi_len);
	fprintf(f, ",\"str_len\":%d,\"rc\":%d,", str_len, rc);
	hex_bytes(f, "out", str, str_len + 1);
	fprintf(f, "}\n");
}

static void record_mi_name_buf(const uint8_t *mi_data, uint8_t mi_len, size_t buf_len)
{
	char buf[OUT_SIZE];
	FILE *f = sections[MI_NAME_BUF];

	memset(buf, GUARD, sizeof(buf));
	CHECK(osmo_mi_name_buf(buf, buf_len, mi_data, mi_len) == buf);
	guard_untouched(buf, buf_len);
	start(MI_NAME_BUF);
	fprintf(f, ",");
	hex_bytes(f, "bytes", mi_data, mi_len);
	fprintf(f, ",\"buf_len\":%zu,", buf_len);
	hex_bytes(f, "out", buf, buf_len + 1);
	fprintf(f, "}\n");
}

static void sweep_mi_to_string(void)
{
	static const char *const inputs[] = {
		"1932547698103254", "11325476981032f4", "3378563412907856f4", "f412345678",
		"f400c0ffee", "1832", "1032f4", "1a32547698103254", "0412345678",
	};
	uint8_t mi[16];

	for (size_t i = 0; i < ARRAY_SIZE(inputs); i++) {
		uint8_t len = parse_hex(inputs[i], mi, sizeof(mi));
		size_t name_len = strlen(osmo_mi_name(mi, len));

		for (int str_len = 0; str_len <= 20; str_len++)
			record_mi_to_string(mi, len, str_len);
		for (size_t buf_len = 0; buf_len <= name_len + 1; buf_len++)
			record_mi_name_buf(mi, len, buf_len);
	}
}

/* ---- Legacy mobile identity generation ---- */

static void record_generate_mid(const char *id, uint8_t type)
{
	uint8_t out[OUT_SIZE];
	uint8_t rc;
	FILE *f = sections[GENERATE_MID];

	memset(out, GUARD, sizeof(out));
	rc = gsm48_generate_mid(out, id, type);
	guard_untouched(out, rc);
	start(GENERATE_MID);
	fprintf(f, ",");
	hex_text(f, "id", id);
	fprintf(f, ",\"type\":%u,\"rc\":%u,", type, rc);
	hex_bytes(f, "out", out, rc);
	fprintf(f, "}\n");
	if (type == GSM_MI_TYPE_IMSI) {
		uint8_t imsi_out[OUT_SIZE];

		memset(imsi_out, GUARD, sizeof(imsi_out));
		CHECK(gsm48_generate_mid_from_imsi(imsi_out, id) == rc);
		CHECK(!memcmp(imsi_out, out, sizeof(out)));
	}
}

static void sweep_generate_mid(void)
{
	static const char *const odd_ids[] = { "1a3", "1A3", "1x3", "12 4", "f", "123456789abcdef0" };
	static const uint8_t types[] = { 0, 1, 2, 3, 4, 5, 6, 7, 0x09, 0x0f, 0xf1 };
	static const uint32_t tmsis[] = { 0, 1, 0x00c0ffee, 0x12345678, 0xaabbccdd, 0xffffffff };
	const char *digits = "12345678901234567";
	char id[32];

	for (size_t t = 0; t < ARRAY_SIZE(types); t++) {
		for (size_t len = 0; len <= strlen(digits); len++) {
			memcpy(id, digits, len);
			id[len] = 0;
			record_generate_mid(id, types[t]);
		}
		for (size_t i = 0; i < ARRAY_SIZE(odd_ids); i++)
			record_generate_mid(odd_ids[i], types[t]);
	}
	for (size_t i = 0; i < ARRAY_SIZE(tmsis); i++) {
		uint8_t out[OUT_SIZE];
		int rc;

		memset(out, GUARD, sizeof(out));
		rc = gsm48_generate_mid_from_tmsi(out, tmsis[i]);
		guard_untouched(out, rc);
		start(GENERATE_MID_FROM_TMSI);
		fprintf(sections[GENERATE_MID_FROM_TMSI], ",\"tmsi\":%" PRIu32 ",\"rc\":%d,", tmsis[i], rc);
		hex_bytes(sections[GENERATE_MID_FROM_TMSI], "out", out, rc);
		fprintf(sections[GENERATE_MID_FROM_TMSI], "}\n");
	}
}

/* ---- Mobile identity encoding ---- */

static void record_mi_encode(const struct osmo_mobile_identity *mi, bool null_buf, size_t buflen,
			     bool allow_hex)
{
	uint8_t out[OUT_SIZE];
	int digits = JUNK_INT;
	int len_rc = osmo_mobile_identity_encoded_len(mi, &digits);
	int rc;
	FILE *f = sections[MI_ENCODE];

	CHECK(osmo_mobile_identity_encoded_len(mi, NULL) == len_rc);
	memset(out, GUARD, sizeof(out));
	rc = osmo_mobile_identity_encode_buf(null_buf ? NULL : out, buflen, mi, allow_hex);
	guard_untouched(out, buflen < OUT_SIZE ? buflen : OUT_SIZE);
	start(MI_ENCODE);
	mi_input_fields(f, "", mi);
	fprintf(f, ",\"null_buf\":%u,\"buflen\":%zu,\"allow_hex\":%u,\"len_rc\":%d,\"len_digits\":%d,"
		"\"rc\":%d,", null_buf, buflen, allow_hex, len_rc, digits, rc);
	hex_bytes(f, "out", out, OUT_RECORDED);
	fprintf(f, "}\n");
}

static const uint8_t encode_types[] = { 0, 1, 2, 3, 5, 7, 0x09, 0x0c };
static const char *const encode_digits[] = {
	"", "1", "12345", "123456", "1234567", "12345678901234", "123456789012345",
	"1234567890123456", "12345a", "12345A", "12345x", "1234 6", "abcdef",
};
static const uint32_t encode_tmsis[] = { 0, 0x12345678, 0xffffffff };

static void sweep_mi_encode(void)
{
	static const size_t buflens[] = { 0, 1, 4, 5, 8, 9, OUT_SIZE };

	for (int allow_hex = 0; allow_hex <= 1; allow_hex++) {
		for (size_t b = 0; b < ARRAY_SIZE(buflens); b++) {
			for (size_t t = 0; t < ARRAY_SIZE(encode_types); t++)
				for (size_t d = 0; d < ARRAY_SIZE(encode_digits); d++) {
					struct osmo_mobile_identity mi = mi_struct(encode_types[t], encode_digits[d], 0);
					record_mi_encode(&mi, false, buflens[b], allow_hex);
				}
			for (size_t i = 0; i < ARRAY_SIZE(encode_tmsis); i++) {
				struct osmo_mobile_identity mi = mi_struct(GSM_MI_TYPE_TMSI, "", encode_tmsis[i]);
				record_mi_encode(&mi, false, buflens[b], allow_hex);
			}
		}
		{
			struct osmo_mobile_identity mi = mi_struct(GSM_MI_TYPE_IMSI, "123456", 0);
			record_mi_encode(&mi, true, 10, allow_hex);
			record_mi_encode(NULL, false, 10, allow_hex);
		}
	}
}

/* ---- Mobile identity text ---- */

static void record_mi_str(const struct osmo_mobile_identity *mi, size_t buflen)
{
	char buf[OUT_SIZE];
	int rc;
	FILE *f = sections[MI_STR];

	memset(buf, GUARD, sizeof(buf));
	rc = osmo_mobile_identity_to_str_buf(buf, buflen, mi);
	guard_untouched(buf, buflen);
	if (buflen == OUT_SIZE) {
		char *c = osmo_mobile_identity_to_str_c(ctx, mi);

		guard_untouched(buf, strlen(buf) + 1);
		CHECK(c && !strcmp(c, buf));
		talloc_free(c);
	}
	start(MI_STR);
	mi_input_fields(f, "", mi);
	fprintf(f, ",\"buflen\":%zu,\"rc\":%d,", buflen, rc);
	if (buflen == OUT_SIZE)
		hex_text(f, "out", buf);
	else
		hex_bytes(f, "out", buf, buflen + 1);
	fprintf(f, "}\n");
}

static void sweep_mi_str(void)
{
	static const uint8_t types[] = { 0, 1, 2, 3, 5, 0x09, 0xff };
	static const char *const digits[] = { "", "123456", "1234567890123456" };
	static const size_t buflens[] = { 0, 1, 5, 6, OUT_SIZE };

	for (size_t b = 0; b < ARRAY_SIZE(buflens); b++) {
		for (size_t t = 0; t < ARRAY_SIZE(types); t++)
			for (size_t d = 0; d < ARRAY_SIZE(digits); d++) {
				struct osmo_mobile_identity mi = mi_struct(types[t], digits[d], 0);
				record_mi_str(&mi, buflens[b]);
			}
		for (size_t i = 0; i < ARRAY_SIZE(encode_tmsis); i++) {
			struct osmo_mobile_identity mi = mi_struct(GSM_MI_TYPE_TMSI, "", encode_tmsis[i]);
			record_mi_str(&mi, buflens[b]);
		}
		record_mi_str(NULL, buflens[b]);
	}
}

/* ---- Mobile identity comparison ---- */

static void sweep_mi_cmp(void)
{
	struct osmo_mobile_identity items[] = {
		mi_struct(GSM_MI_TYPE_NONE, "", 0),
		mi_struct(GSM_MI_TYPE_NONE, "999", 0),
		mi_struct(GSM_MI_TYPE_IMSI, "123456", 0),
		mi_struct(GSM_MI_TYPE_IMSI, "1234567", 0),
		mi_struct(GSM_MI_TYPE_IMSI, "123457", 0),
		mi_struct(GSM_MI_TYPE_IMSI, "923456", 0),
		mi_struct(GSM_MI_TYPE_IMEI, "123456789012345", 0),
		mi_struct(GSM_MI_TYPE_IMEI, "12345678901234", 0),
		mi_struct(GSM_MI_TYPE_IMEISV, "1234567890123456", 0),
		mi_struct(GSM_MI_TYPE_TMSI, "", 0),
		mi_struct(GSM_MI_TYPE_TMSI, "", 1),
		mi_struct(GSM_MI_TYPE_TMSI, "", 0x10000000),
		mi_struct(GSM_MI_TYPE_TMSI, "", 0xffffffff),
		mi_struct(5, "123", 0),
		mi_struct(5, "456", 0),
	};
	const size_t count = ARRAY_SIZE(items) + 1;
	FILE *f = sections[MI_CMP];

	for (size_t i = 0; i < count; i++)
		for (size_t j = 0; j < count; j++) {
			const struct osmo_mobile_identity *a = i < ARRAY_SIZE(items) ? &items[i] : NULL;
			const struct osmo_mobile_identity *b = j < ARRAY_SIZE(items) ? &items[j] : NULL;
			int rc = osmo_mobile_identity_cmp(a, b);

			start(MI_CMP);
			mi_input_fields(f, "a_", a);
			mi_input_fields(f, "b_", b);
			fprintf(f, ",\"rc\":%d}\n", rc);
		}
	/* An identity is equal to itself, whatever its union holds. */
	CHECK(osmo_mobile_identity_cmp(&items[2], &items[2]) == 0);
}

/* ---- Complete Layer 3 messages ---- */

static int record_l3(const uint8_t *l3, size_t len, bool allow_hex, struct osmo_mobile_identity *out)
{
	struct osmo_mobile_identity mi;
	uint8_t *copy = talloc_memdup(ctx, l3, len ? len : 1);
	int rc;
	FILE *f = sections[L3];

	CHECK(copy);
	memset(&mi, 0xff, sizeof(mi));
	rc = osmo_mobile_identity_decode_from_l3_buf(&mi, copy, len, allow_hex);
	CHECK(!memcmp(copy, l3, len));
	talloc_free(copy);
	start(L3);
	fprintf(f, ",");
	hex_bytes(f, "l3", l3, len);
	fprintf(f, ",\"allow_hex\":%u,\"rc\":%d", allow_hex, rc);
	mi_fields(f, "", &mi);
	fprintf(f, "}\n");
	if (out)
		*out = mi;
	return rc;
}

/* Every prefix of a message, which shows each length check. */
static void record_l3_prefixes(const uint8_t *l3, size_t len)
{
	for (size_t prefix = 0; prefix < len; prefix++)
		record_l3(l3, prefix, false, NULL);
}

static void sweep_l3(void)
{
	static const char *const extra[] = {
		/* Talker Indication: CM2 of three bytes, then the MI. */
		"0611" "035058a6" "05f45a42e404",
		"0611" "035058a6" "04113254f6",
		"0611" "025058" "04113254f6",
		"0611" "045058a623" "04113254f6",
		"0611" "035058a6" "00",
		"0611" "035058a6",
		/* Identity Response with an IMSI of hex digits. */
		"0519" "08a9100700000064f2",
		"0519" "081a3254769810a254",
		/* A Paging Response shorter than C's minimum, whose identity would fit. */
		"0627000001" "05ff",
		/* CM Service Request with a CM2 that runs past the message. */
		"052401095058a6",
		/* RR Paging Response and MM messages with N(SD) bits in the message type. */
		"05480200816800013004113254f6",
		"05880200816800013004113254f6",
		"06670003505886" "05f4b48883de",
		/* A transaction identifier in the first byte. */
		"15080200816800013004113254f6",
		"86270003505886" "05f4b48883de",
	};
	uint8_t l3[64];
	size_t len;

	for (size_t i = 0; i < ARRAY_SIZE(extra); i++) {
		len = parse_hex(extra[i], l3, sizeof(l3));
		record_l3(l3, len, false, NULL);
		record_l3(l3, len, true, NULL);
		record_l3_prefixes(l3, len);
	}
	/* Every first byte and every message type around a valid LU and Paging Response. */
	len = parse_hex("050802008168000130" "04113254f6", l3, sizeof(l3));
	for (unsigned int v = 0; v < 256; v++) {
		l3[0] = v;
		record_l3(l3, len, false, NULL);
	}
	l3[0] = 0x05;
	for (unsigned int v = 0; v < 256; v++) {
		l3[1] = v;
		record_l3(l3, len, false, NULL);
	}
	len = parse_hex("06270003505886" "05f4b48883de", l3, sizeof(l3));
	for (unsigned int v = 0; v < 256; v++) {
		l3[1] = v;
		record_l3(l3, len, false, NULL);
	}
}

/* ---- Upstream's test, replayed ---- */

static const struct gprs_ra_id upstream_ra_items[] = {
	{ .mcc = 77, .mnc = 121, .lac = 666, .rac = 5 },
	{ .mcc = 84, .mnc = 98, .lac = 11, .rac = 89 },
	{ .mcc = 0, .mnc = 0, .lac = 0, .rac = 0, .mnc_3_digits = false },
	{ .mcc = 0, .mnc = 0, .lac = 0, .rac = 0, .mnc_3_digits = true },
	{ .mcc = 999, .mnc = 999, .lac = 65535, .rac = 255 },
	{ .mcc = 1, .mnc = 2, .lac = 23, .rac = 42, .mnc_3_digits = false },
	{ .mcc = 1, .mnc = 2, .lac = 23, .rac = 42, .mnc_3_digits = true },
	{ .mcc = 12, .mnc = 34, .lac = 56, .rac = 78, .mnc_3_digits = false },
	{ .mcc = 12, .mnc = 34, .lac = 23, .rac = 42, .mnc_3_digits = true },
	{ .mcc = 123, .mnc = 456, .lac = 23, .rac = 42, .mnc_3_digits = false },
	{ .mcc = 123, .mnc = 456, .lac = 23, .rac = 42, .mnc_3_digits = true },
};

static const struct {
	uint8_t type;
	const char *digits;
	const char *name;
	size_t str_size;
	const char *tlv_hex;
	const char *str;
	int rc;
} upstream_mids[] = {
	{ GSM_MI_TYPE_IMSI, "123456789012345", "IMSI-123456789012345", 0, "17081932547698103254", NULL, 0 },
	{ GSM_MI_TYPE_IMSI, "12345678901234", "IMSI-12345678901234", 0, "170811325476981032f4", NULL, 0 },
	{ GSM_MI_TYPE_IMSI, "423423", "IMSI-423423", 0, "1704413224f3", NULL, 0 },
	{ GSM_MI_TYPE_IMSI | GSM_MI_ODD, "423423", "IMSI-423423", 0, "1704413224f3", NULL, 0 },
	{ GSM_MI_TYPE_IMSI, "4234235", "IMSI-4234235", 0, "170449322453", NULL, 0 },
	{ GSM_MI_TYPE_IMSI, "4234235", "IMSI-4234235", 4, "170449322453", "423", 0 },
	{ GSM_MI_TYPE_IMEI, "123456789012345", "IMEI-123456789012345", 0, "17081a32547698103254", NULL, 0 },
	{ GSM_MI_TYPE_IMEI, "98765432109876", "IMEI-98765432109876", 0, "170892785634129078f6", NULL, 0 },
	{ GSM_MI_TYPE_IMEI, "987654321098765", "IMEI-987654321098765", 0, "17089a78563412907856", NULL, 0 },
	{ GSM_MI_TYPE_IMEISV, "9876543210987654", "IMEI-SV-9876543210987654", 0, "17099378563412907856f4", NULL, 0 },
	{ GSM_MI_TYPE_IMEISV, "9876543210987654", "IMEI-SV-9876543210987654", 17, "17099378563412907856f4", "9876543210987654", 0 },
	{ GSM_MI_TYPE_TMSI, "305419896", "TMSI-0x12345678", 0, "1705f412345678", NULL, 9 },
	{ GSM_MI_TYPE_TMSI, "12648430", "TMSI-0x00C0FFEE", 0, "1705f400c0ffee", NULL, 8 },
	{ GSM_MI_TYPE_TMSI, "0", "TMSI-0x00000000", 0, "1705f400000000", NULL, 1 },
	{ GSM_MI_TYPE_TMSI, "305419896", "TMSI-0x12345678", 5, "1705f412345678", "3054", 9 },
	{ GSM_MI_TYPE_NONE, "123", "unknown", 0, "17021832", "", 0 },
	{ GSM_MI_TYPE_NONE, "1234", "unknown", 0, "17031032f4", "", 0 },
	{ GSM_MI_ODD, "1234", "unknown", 0, "17031032f4", "", 0 },
};

static const struct {
	const char *label;
	const char *l3;
	int rc;
	uint8_t type;
	const char *digits;
	uint32_t tmsi;
} upstream_l3s[] = {
	{ "LU with IMSI 901700000004620", "050802008168000130089910070000006402", 0, GSM_MI_TYPE_IMSI, "901700000004620", 0 },
	{ "LU with TMSI 0x0980ad8a", "05084262f224002a5005f40980ad8a", 0, GSM_MI_TYPE_TMSI, NULL, 0x0980ad8a },
	{ "LU with invalid MI type", "050802008168000130089d10070000006402", -EINVAL, 0, NULL, 0 },
	{ "LU with truncated IMSI MI", "0508020081680001300899100700000064", -EBADMSG, 0, NULL, 0 },
	{ "LU with too short IMSI MI (12345)", "05080200816800013003193254", -EBADMSG, 0, NULL, 0 },
	{ "LU with just long enough IMSI MI 123456", "05080200816800013004113254f6", 0, GSM_MI_TYPE_IMSI, "123456", 0 },
	{ "LU with max length IMSI MI 123456789012345", "050802008168000130081932547698103254", 0, GSM_MI_TYPE_IMSI, "123456789012345", 0 },
	{ "LU with just too long IMSI MI 1234567890123456", "050802008168000130091132547698103254f6", -EBADMSG, 0, NULL, 0 },
	{ "LU with truncated TMSI MI", "05084262f224002a5005f40980ad", -EBADMSG, 0, NULL, 0 },
	{ "LU with odd length TMSI", "05084262f224002a5005fc0980ad8a", -EBADMSG, 0, NULL, 0 },
	{ "LU with too long TMSI MI", "05084262f224002a5006f40980ad23", -EBADMSG, 0, NULL, 0 },
	{ "LU with too short TMSI", "05084262f224002a5004f480ad8a", -EBADMSG, 0, NULL, 0 },
	{ "CM Service Request with IMSI 123456", "052401035058a604113254f6", 0, GSM_MI_TYPE_IMSI, "123456", 0 },
	{ "CM Service Request with TMSI 0x5a42e404", "052401035058a605f45a42e404", 0, GSM_MI_TYPE_TMSI, NULL, 0x5a42e404 },
	{ "CM Service Request with shorter CM2, with IMSI 123456", "05240102505804113254f6", 0, GSM_MI_TYPE_IMSI, "123456", 0 },
	{ "CM Service Request with longer CM2, with IMSI 123456", "052401055058a6234204113254f6", 0, GSM_MI_TYPE_IMSI, "123456", 0 },
	{ "CM Service Request with shorter CM2, with TMSI 0x00000000", "05240102505805f400000000", 0, GSM_MI_TYPE_TMSI, NULL, 0 },
	{ "CM Service Request with invalid MI type", "052401035058a6089d10070000006402", -EINVAL, 0, NULL, 0 },
	{ "CM Service Request with truncated IMSI MI", "052401035058a60899100700000064", -EBADMSG, 0, NULL, 0 },
	{ "CM Service Request with truncated TMSI MI", "052401015005f40980ad", -EBADMSG, 0, NULL, 0 },
	{ "CM Service Request with odd length TMSI", "052401045058a62305fc0980ad8a", -EBADMSG, 0, NULL, 0 },
	{ "CM Service Request with too long TMSI MI", "052401035058a606f40980ad23", -EBADMSG, 0, NULL, 0 },
	{ "CM Service Request with too short TMSI", "052401035058a604f480ad8a", -EBADMSG, 0, NULL, 0 },
	{ "CM Service Reestablish Request with TMSI 0x5a42e404", "052801035058a605f45a42e404", 0, GSM_MI_TYPE_TMSI, NULL, 0x5a42e404 },
	{ "Paging Response with IMSI 1234567", "062700035058860419325476", 0, GSM_MI_TYPE_IMSI, "1234567", 0 },
	{ "Paging Response with TMSI 0xb48883de", "0627000350588605f4b48883de", 0, GSM_MI_TYPE_TMSI, NULL, 0xb48883de },
	{ "Paging Response with TMSI, with unused nibble not 0xf", "06270003505886" "0504b48883de", -EBADMSG, 0, NULL, 0 },
	{ "Paging Response with too short IMEI (1234567)", "06270003505886041a325476", -EBADMSG, 0, NULL, 0 },
	{ "Paging Response with IMEI 123456789012345", "06270003505886081a32547698103254", 0, GSM_MI_TYPE_IMEI, "123456789012345", 0 },
	{ "Paging Response with IMEI 12345678901234 (no Luhn checksum)", "062700035058860812325476981032f4", 0, GSM_MI_TYPE_IMEI, "12345678901234", 0 },
	{ "Paging Response with IMEISV 1234567890123456", "06270003505886091332547698103254f6", 0, GSM_MI_TYPE_IMEISV, "1234567890123456", 0 },
	{ "Paging Response with too short IMEISV 123456789012345", "06270003505886081b32547698103254", -EBADMSG, 0, NULL, 0 },
	{ "Paging Response with too long IMEISV 12345678901234567", "06270003505886091b3254769810325476", -EBADMSG, 0, NULL, 0 },
	{ "Paging Response with IMSI 123456789012345 and flipped ODD bit", "06270003505886081132547698103254", -EBADMSG, 0, NULL, 0 },
	{ "IMSI-Detach with IMSI 901700000004620", "050130089910070000006402", 0, GSM_MI_TYPE_IMSI, "901700000004620", 0 },
	{ "IMSI-Detach with TMSI 0x0980ad8a", "05013005f40980ad8a", 0, GSM_MI_TYPE_TMSI, NULL, 0x0980ad8a },
	{ "IMSI-Detach with invalid MI type", "050130089d10070000006402", -EINVAL, 0, NULL, 0 },
	{ "IMSI-Detach with truncated IMSI MI", "0501300899100700000064", -EBADMSG, 0, NULL, 0 },
	{ "IMSI-Detach with too short IMSI MI (12345)", "05013003193254", -EBADMSG, 0, NULL, 0 },
	{ "IMSI-Detach with just long enough IMSI MI 123456", "05013004113254f6", 0, GSM_MI_TYPE_IMSI, "123456", 0 },
	{ "IMSI-Detach with max length IMSI MI 123456789012345", "050130081932547698103254", 0, GSM_MI_TYPE_IMSI, "123456789012345", 0 },
	{ "IMSI-Detach with just too long IMSI MI 1234567890123456", "050130091132547698103254f6", -EBADMSG, 0, NULL, 0 },
	{ "IMSI-Detach with truncated TMSI MI", "05013005f40980ad", -EBADMSG, 0, NULL, 0 },
	{ "IMSI-Detach with odd length TMSI", "05013005fc0980ad8a", -EBADMSG, 0, NULL, 0 },
	{ "IMSI-Detach with too long TMSI MI", "05013006f40980ad23", -EBADMSG, 0, NULL, 0 },
	{ "IMSI-Detach with too short TMSI", "05013004f480ad8a", -EBADMSG, 0, NULL, 0 },
	{ "Identity Response with IMSI 901700000004620", "0519089910070000006402", 0, GSM_MI_TYPE_IMSI, "901700000004620", 0 },
	{ "Identity Response with IMEI 123456789012345", "0519081a32547698103254", 0, GSM_MI_TYPE_IMEI, "123456789012345", 0 },
	{ "Identity Response with IMEISV 9876543210987654", "0519099378563412907856f4", 0, GSM_MI_TYPE_IMEISV, "9876543210987654", 0 },
};

static void dump_ra(FILE *f, const struct gprs_ra_id *r)
{
	fprintf(f, "%s%s\n", osmo_rai_name(r), r->mnc_3_digits ? " (3-digit MNC)" : "");
}

static void dump_lai(FILE *f, const struct osmo_location_area_id *l)
{
	fprintf(f, "%s%s\n", osmo_lai_name(l), l->plmn.mnc_3_digits ? " (3-digit MNC)" : "");
}

static void dump_rai(FILE *f, const struct osmo_routing_area_id *r)
{
	fprintf(f, "%s%s", osmo_rai_name2(r), r->lac.plmn.mnc_3_digits ? " (3-digit MNC)" : "");
}

static void replay_mids(FILE *f)
{
	static const uint8_t zero_length_types[] = { GSM_MI_TYPE_IMSI, GSM_MI_TYPE_TMSI, GSM_MI_TYPE_NONE };
	static const uint8_t tmsi_mid[] = { 0x17, 0x05, 0xf4, 0xaa, 0xbb, 0xcc, 0xdd };
	uint8_t buf[OUT_SIZE];
	uint8_t valid_mi[64];
	int valid_mi_len;
	uint8_t len;

	memset(buf, 0xfe, sizeof(buf));
	CHECK(gsm48_generate_mid_from_tmsi(buf, 0xaabbccdd) == 7);
	CHECK(!memcmp(buf, tmsi_mid, sizeof(tmsi_mid)));
	fprintf(f, "Simple TMSI encoding test....passed\n");
	len = gsm48_generate_mid_from_imsi(buf, "901700000004620");
	fprintf(f, "Simple IMSI encoding test....passed: [%u] %s\n", len, osmo_hexdump(buf, len));
	record_generate_mid("901700000004620", GSM_MI_TYPE_IMSI);

	fprintf(f, "\nTesting Mobile Identity conversions\n");
	for (size_t i = 0; i < ARRAY_SIZE(upstream_mids); i++) {
		uint8_t tlv[64];
		char str[64] = {};
		size_t str_size = upstream_mids[i].str_size ? : sizeof(str);
		const char *expect_str = upstream_mids[i].str ? : upstream_mids[i].digits;
		int expect_rc = upstream_mids[i].rc ? : (int)strlen(expect_str) + 1;
		int tlv_len, rc;
		size_t str_len;
		const char *name;

		fprintf(f, "- %s %s\n", gsm48_mi_type_name(upstream_mids[i].type), upstream_mids[i].digits);
		if (upstream_mids[i].type == GSM_MI_TYPE_TMSI) {
			uint32_t tmsi = (uint32_t)atoll(upstream_mids[i].digits);
			tlv_len = gsm48_generate_mid_from_tmsi(tlv, tmsi);
		} else {
			tlv_len = gsm48_generate_mid(tlv, upstream_mids[i].digits, upstream_mids[i].type);
			record_generate_mid(upstream_mids[i].digits, upstream_mids[i].type);
		}
		CHECK(!strcmp(osmo_hexdump_nospc(tlv, tlv_len), upstream_mids[i].tlv_hex));
		fprintf(f, "  -> MI-TLV-hex='%s'\n", osmo_hexdump_nospc(tlv, tlv_len));
		rc = gsm48_mi_to_string(str, str_size, tlv + 2, tlv_len - 2);
		CHECK(!strcmp(str, expect_str) && rc == expect_rc);
		fprintf(f, "  -> MI-str=%s rc=%d\n", osmo_quote_str(str, -1), rc);
		record_mi_to_string(tlv + 2, tlv_len - 2, str_size);
		name = osmo_mi_name(tlv + 2, tlv_len - 2);
		CHECK(!strcmp(name, upstream_mids[i].name));
		fprintf(f, "  -> MI-name=%s\n", osmo_quote_str(name, -1));
		record_mi_bytes(tlv + 2, tlv_len - 2);
		/* A tainted buffer still ends at the NUL. */
		str_len = strlen(str);
		str[str_len] = '!';
		gsm48_mi_to_string(str, str_size, tlv + 2, tlv_len - 2);
		CHECK(strlen(str) == str_len);
	}

	fprintf(f, "\nDecoding zero length Mobile Identities\n");
	valid_mi_len = osmo_hexparse("1932547698103254", valid_mi, sizeof(valid_mi));
	CHECK(valid_mi_len == 8);
	for (int odd = 0; odd <= 1; odd++)
		for (size_t i = 0; i < ARRAY_SIZE(zero_length_types); i++) {
			uint8_t type = zero_length_types[i] | (odd ? GSM_MI_ODD : 0);
			char str[8] = {};
			int rc;

			fprintf(f, "- MI type: %s%s\n", gsm48_mi_type_name(type & GSM_MI_TYPE_MASK),
				odd ? " | GSM_MI_ODD" : "");
			valid_mi[0] = (valid_mi[0] & 0xf0) | type;
			fprintf(f, "  - writing to zero-length string:\n");
			memset(str, '!', sizeof(str) - 1);
			rc = gsm48_mi_to_string(str, 0, valid_mi, valid_mi_len);
			CHECK(str[0] == '!');
			fprintf(f, "    rc=%d\n    nothing written\n", rc);
			record_mi_to_string(valid_mi, valid_mi_len, 0);
			fprintf(f, "  - writing to 1-byte-length string:\n");
			memset(str, '!', sizeof(str) - 1);
			rc = gsm48_mi_to_string(str, 1, valid_mi, valid_mi_len);
			CHECK(str[0] == '\0' && str[1] == '!');
			fprintf(f, "    rc=%d\n    returned empty string\n", rc);
			record_mi_to_string(valid_mi, valid_mi_len, 1);
			fprintf(f, "  - decode zero-length mi:\n");
			memset(str, '!', sizeof(str) - 1);
			rc = gsm48_mi_to_string(str, sizeof(str), valid_mi, 0);
			CHECK(str[0] == '\0');
			fprintf(f, "    rc=%d\n    returned empty string\n", rc);
			record_mi_to_string(valid_mi, 0, sizeof(str));
		}
	fprintf(f, "\n");
}

static void replay_l3s(FILE *f)
{
	fprintf(f, "test_struct_mobile_identity()\n");
	for (size_t i = 0; i < ARRAY_SIZE(upstream_l3s); i++) {
		uint8_t l3[64];
		size_t len = parse_hex(upstream_l3s[i].l3, l3, sizeof(l3));
		struct osmo_mobile_identity mi;
		int rc = record_l3(l3, len, false, &mi);
		bool ok = rc == upstream_l3s[i].rc;

		if (ok && !rc) {
			struct osmo_mobile_identity expect = mi_struct(upstream_l3s[i].type,
				upstream_l3s[i].digits ? : "", upstream_l3s[i].tmsi);
			ok = !osmo_mobile_identity_cmp(&mi, &expect);
		}
		CHECK(ok);
		fprintf(f, "%s: %s", upstream_l3s[i].label, rc ? "rc != 0" : "rc == 0");
		if (!rc) {
			char *text = osmo_mobile_identity_to_str_c(ctx, &mi);
			fprintf(f, ", mi = %s", text);
			talloc_free(text);
		}
		fprintf(f, " ok\n");
		source = "sweep";
		record_l3_prefixes(l3, len);
		source = "upstream";
	}
	fprintf(f, "\n");
}

static void replay_areas(FILE *f)
{
	for (size_t i = 0; i < ARRAY_SIZE(upstream_ra_items); i++) {
		const struct gprs_ra_id *r = &upstream_ra_items[i];
		struct gsm48_ra_id ra;
		struct gprs_ra_id parsed = {};

		gsm48_encode_ra(&ra, r);
		fprintf(f, "Constructed RA:\n");
		gsm48_parse_ra(&parsed, (const uint8_t *)&ra);
		dump_ra(f, r);
		fprintf(f, "MCC+MNC in BCD: %s\n", osmo_hexdump(ra.digits, sizeof(ra.digits)));
		dump_ra(f, &parsed);
		CHECK(r->mnc == parsed.mnc && r->mcc == parsed.mcc && r->lac == parsed.lac && r->rac == parsed.rac
		      && (r->mnc_3_digits || r->mnc > 99) == parsed.mnc_3_digits);
		fprintf(f, "RA test...passed\n");
		record_rai_name(r);
		record_rai_name(&parsed);
	}

	for (size_t i = 0; i < ARRAY_SIZE(upstream_ra_items); i++) {
		struct gprs_ra_id laid = upstream_ra_items[i];
		struct gprs_ra_id decoded = {};
		struct gsm48_loc_area_id lai48 = {};
		struct osmo_location_area_id lai2 = {
			.plmn = plmn(laid.mcc, laid.mnc, laid.mnc_3_digits),
			.lac = laid.lac,
		};
		struct osmo_location_area_id decoded2 = {};

		laid.rac = 0;
		fprintf(f, "- gsm48_generate_lai() from ");
		dump_ra(f, &laid);
		gsm48_generate_lai(&lai48, laid.mcc, laid.mnc, laid.lac);
		fprintf(f, "  Encoded %s\n", osmo_hexdump((unsigned char *)&lai48, sizeof(lai48)));
		CHECK(gsm48_decode_lai(&lai48, &decoded.mcc, &decoded.mnc, &decoded.lac) == 0);
		fprintf(f, "  gsm48_decode_lai() gives  ");
		dump_ra(f, &decoded);
		CHECK(decoded.mcc == laid.mcc && decoded.mnc == laid.mnc && decoded.lac == laid.lac);
		fprintf(f, "  passed\n");
		record_lai_decode((const uint8_t *)&lai48);

		fprintf(f, "- gsm48_generate_lai2() from ");
		dump_lai(f, &lai2);
		gsm48_generate_lai2(&lai48, &lai2);
		fprintf(f, "  Encoded %s\n", osmo_hexdump((unsigned char *)&lai48, sizeof(lai48)));
		gsm48_decode_lai2(&lai48, &decoded2);
		fprintf(f, "  gsm48_decode_lai2() gives  ");
		dump_lai(f, &decoded2);
		CHECK(decoded2.plmn.mcc == lai2.plmn.mcc && decoded2.plmn.mnc == lai2.plmn.mnc
		      && decoded2.lac == lai2.lac
		      && decoded2.plmn.mnc_3_digits == (lai2.plmn.mnc_3_digits || lai2.plmn.mnc > 99));
		fprintf(f, "  passed\n");
		record_lai_encode(&lai2);
	}

	fprintf(f, "==test_osmo_routing_area_id()==\n");
	for (size_t i = 0; i < ARRAY_SIZE(upstream_ra_items); i++) {
		const struct gprs_ra_id *g = &upstream_ra_items[i];
		struct osmo_routing_area_id r = {
			.lac = { .plmn = plmn(g->mcc, g->mnc, g->mnc_3_digits), .lac = g->lac },
			.rac = g->rac,
		};
		uint8_t buf[sizeof(struct gsm48_ra_id)] = {};
		struct osmo_routing_area_id decoded = {};
		int rc;

		fprintf(f, "RA ID: ");
		dump_rai(f, &r);
		rc = osmo_routing_area_id_encode_buf(buf, sizeof(buf), &r);
		fprintf(f, "osmo_routing_area_id_encode_buf(): %src=%d\n", osmo_hexdump(buf, sizeof(buf)), rc);
		rc = osmo_routing_area_id_decode(&decoded, buf, sizeof(buf));
		fprintf(f, "osmo_routing_area_id_decode(): ");
		dump_rai(f, &decoded);
		fprintf(f, " rc=%d\n", rc);
		CHECK(!osmo_rai_cmp(&r, &decoded));
		fprintf(f, "ok\n");
		record_ra_encode(&r, sizeof(buf));
		record_ra_decode(buf, sizeof(buf));
	}
}

/* The text of the .ok file from the line that starts with `from` to the line that starts
 * with `to`, not including it. */
static void ok_part(FILE *expected, const char *ok, const char *from, const char *to)
{
	const char *begin = strstr(ok, from);
	const char *end;

	CHECK(begin && (begin == ok || begin[-1] == '\n'));
	end = strstr(begin, to);
	CHECK(end && end[-1] == '\n');
	fwrite(begin, 1, end - begin, expected);
}

static void upstream_replay(const char *ok_path)
{
	FILE *file, *expected_out;
	char *ok, *expected;
	size_t ok_size, expected_size;

	upstream_out = open_memstream(&upstream_text, &upstream_size);
	CHECK(upstream_out);
	source = "upstream";
	replay_mids(upstream_out);
	replay_l3s(upstream_out);
	replay_areas(upstream_out);
	source = "sweep";
	fclose(upstream_out);

	file = fopen(ok_path, "r");
	CHECK(file);
	CHECK(fseek(file, 0, SEEK_END) == 0);
	ok_size = ftell(file);
	CHECK(fseek(file, 0, SEEK_SET) == 0);
	ok = calloc(1, ok_size + 1);
	CHECK(ok && fread(ok, 1, ok_size, file) == ok_size);
	fclose(file);

	expected_out = open_memstream(&expected, &expected_size);
	CHECK(expected_out);
	ok_part(expected_out, ok, "Simple TMSI encoding test", "BSD number encoding");
	ok_part(expected_out, ok, "Constructed RA:", "=====cm3_1=====");
	fclose(expected_out);
	if (upstream_size != expected_size || memcmp(upstream_text, expected, expected_size)) {
		fwrite(upstream_text, 1, upstream_size, stderr);
		fail("replayed output differs from gsm0408_test.ok");
	}
	free(expected);
	free(ok);
	free(upstream_text);
}

/* ---- Constants ---- */

static void constants(void)
{
	static const struct { const char *name; long long value; } values[] = {
		{ "GSM_MI_TYPE_MASK", GSM_MI_TYPE_MASK },
		{ "GSM_MI_TYPE_NONE", GSM_MI_TYPE_NONE },
		{ "GSM_MI_TYPE_IMSI", GSM_MI_TYPE_IMSI },
		{ "GSM_MI_TYPE_IMEI", GSM_MI_TYPE_IMEI },
		{ "GSM_MI_TYPE_IMEISV", GSM_MI_TYPE_IMEISV },
		{ "GSM_MI_TYPE_TMSI", GSM_MI_TYPE_TMSI },
		{ "GSM_MI_ODD", GSM_MI_ODD },
		{ "GSM48_IE_MOBILE_ID", GSM48_IE_MOBILE_ID },
		{ "GSM48_TMSI_LEN", GSM48_TMSI_LEN },
		{ "GSM48_MID_TMSI_LEN", GSM48_MID_TMSI_LEN },
		{ "GSM48_MID_MAX_SIZE", GSM48_MID_MAX_SIZE },
		{ "GSM48_MI_SIZE", GSM48_MI_SIZE },
		{ "GSM_RESERVED_TMSI", GSM_RESERVED_TMSI },
		{ "GSM23003_TMSI_NUM_BYTES", GSM23003_TMSI_NUM_BYTES },
		{ "GSM23003_IMEI_NUM_DIGITS_NO_CHK", GSM23003_IMEI_NUM_DIGITS_NO_CHK },
		{ "GSM23003_IMEISV_NUM_DIGITS", GSM23003_IMEISV_NUM_DIGITS },
		{ "sizeof_gsm48_loc_area_id", sizeof(struct gsm48_loc_area_id) },
		{ "sizeof_gsm48_ra_id", sizeof(struct gsm48_ra_id) },
	};

	for (size_t i = 0; i < ARRAY_SIZE(values); i++)
		printf("{\"kind\":\"constant\",\"name\":\"%s\",\"value\":%lld}\n",
		       values[i].name, values[i].value);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s gsm0408_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
	ctx = talloc_named_const(NULL, 0, "gsm48_vector");
	CHECK(ctx);
	for (size_t i = 0; i < SECTION_COUNT; i++) {
		sections[i] = open_memstream(&section_text[i], &section_size[i]);
		CHECK(sections[i]);
	}

	upstream_replay(argv[1]);
	sweep_plmn_bcd();
	sweep_lai();
	sweep_ra();
	sweep_rai_names();
	sweep_mi_type_names();
	sweep_mi_bytes();
	sweep_mi_to_string();
	sweep_generate_mid();
	sweep_mi_encode();
	sweep_mi_str();
	sweep_mi_cmp();
	sweep_l3();

	printf("{\"schema\":\"gsm48-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	constants();
	for (size_t i = 0; i < SECTION_COUNT; i++) {
		fclose(sections[i]);
		fwrite(section_text[i], 1, section_size[i], stdout);
		free(section_text[i]);
	}
	CHECK(talloc_total_blocks(ctx) == 1);
	talloc_free(ctx);
	return EXIT_SUCCESS;
}
