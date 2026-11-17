/*
 * Reference harness for libosmogsm's 3GPP TS 23.003 identities: IMSI, MSISDN and IMEI
 * checks, MCC and MNC parsing, the names and comparisons of PLMN, LAI, RAI, CGI, CGI-PS,
 * SAI, RNC ID, MME ID and GUMMEI, and the home network and MME domain names.
 *
 * Calls the API directly and writes NDJSON: a metadata line, the constants, then one
 * section per kind of record. The first argument is upstream's gsm23003_test.ok. Upstream's
 * test is replayed with the same calls, and the run fails unless its output matches that
 * file byte for byte.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <talloc.h>

#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm23003.h>
#include <osmocom/gsm/protocol/gsm_23_003.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55
/* Larger than any name or domain, so the bytes past each one show it wrote no further. */
#define OUT_SIZE 96
#define JUNK_U16 4242

enum section { VALID, FROM_STR, NAME, NAME_BUF, CMP, GEN, PARSE, SECTION_COUNT };

static FILE *sections[SECTION_COUNT];
static char *section_text[SECTION_COUNT];
static size_t section_size[SECTION_COUNT];
/* Upstream's printed output, compared with gsm23003_test.ok at the end. */
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

static uint32_t next_random(uint32_t *state)
{
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

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

static void start(enum section section, const char *kind)
{
	fprintf(sections[section], "{\"kind\":\"%s\",\"source\":\"%s\"", kind, source);
}

static void guard_untouched(const char *buf, size_t from)
{
	for (size_t i = from; i < OUT_SIZE; i++)
		CHECK((unsigned char)buf[i] == GUARD);
}

/* ---- Identity fields ---- */

static void plmn_fields(FILE *f, const char *prefix, const struct osmo_plmn_id *plmn)
{
	fprintf(f, ",\"%smcc\":%u,\"%smnc\":%u,\"%smnc_3_digits\":%u", prefix, plmn->mcc, prefix,
		plmn->mnc, prefix, plmn->mnc_3_digits);
}

static void lai_fields(FILE *f, const char *prefix, const struct osmo_location_area_id *lai)
{
	plmn_fields(f, prefix, &lai->plmn);
	fprintf(f, ",\"%slac\":%u", prefix, lai->lac);
}

static void rai_fields(FILE *f, const char *prefix, const struct osmo_routing_area_id *rai)
{
	lai_fields(f, prefix, &rai->lac);
	fprintf(f, ",\"%srac\":%u", prefix, rai->rac);
}

static void cgi_fields(FILE *f, const char *prefix, const struct osmo_cell_global_id *cgi)
{
	lai_fields(f, prefix, &cgi->lai);
	fprintf(f, ",\"%sci\":%u", prefix, cgi->cell_identity);
}

static void cgi_ps_fields(FILE *f, const char *prefix, const struct osmo_cell_global_id_ps *cgi_ps)
{
	rai_fields(f, prefix, &cgi_ps->rai);
	fprintf(f, ",\"%sci\":%u", prefix, cgi_ps->cell_identity);
}

static void sai_fields(FILE *f, const char *prefix, const struct osmo_service_area_id *sai)
{
	lai_fields(f, prefix, &sai->lai);
	fprintf(f, ",\"%ssac\":%u", prefix, sai->sac);
}

static void rnc_id_fields(FILE *f, const char *prefix, const struct osmo_rnc_id *rnc_id)
{
	plmn_fields(f, prefix, &rnc_id->plmn);
	fprintf(f, ",\"%srnc_id\":%u", prefix, rnc_id->rnc_id);
}

static void mme_id_fields(FILE *f, const char *prefix, const struct osmo_mme_id *mme)
{
	fprintf(f, ",\"%sgroup_id\":%u,\"%scode\":%u", prefix, mme->group_id, prefix, mme->code);
}

static void gummei_fields(FILE *f, const char *prefix, const struct osmo_gummei *gummei)
{
	plmn_fields(f, prefix, &gummei->plmn);
	mme_id_fields(f, prefix, &gummei->mme);
}

static struct osmo_plmn_id plmn(uint16_t mcc, uint16_t mnc, bool mnc_3_digits)
{
	return (struct osmo_plmn_id){ .mcc = mcc, .mnc = mnc, .mnc_3_digits = mnc_3_digits };
}

static struct osmo_location_area_id lai(struct osmo_plmn_id p, uint16_t lac)
{
	return (struct osmo_location_area_id){ .plmn = p, .lac = lac };
}

static struct osmo_routing_area_id rai(struct osmo_plmn_id p, uint16_t lac, uint8_t rac)
{
	return (struct osmo_routing_area_id){ .lac = lai(p, lac), .rac = rac };
}

/* ---- Validity checks ---- */

static bool record_valid(const char *check, const char *input)
{
	bool result;

	if (!strcmp(check, "imsi"))
		result = osmo_imsi_str_valid(input);
	else if (!strcmp(check, "msisdn"))
		result = osmo_msisdn_str_valid(input);
	else if (!strcmp(check, "imei"))
		result = osmo_imei_str_valid(input, false);
	else if (!strcmp(check, "imei_check"))
		result = osmo_imei_str_valid(input, true);
	else
		fail("unknown check");
	start(VALID, "valid");
	fprintf(sections[VALID], ",\"check\":\"%s\",", check);
	hex_text(sections[VALID], "input", input);
	fprintf(sections[VALID], ",\"result\":%u}\n", result);
	return result;
}

/* ---- MCC and MNC from text ---- */

static int record_from_str(const char *input, uint16_t *mnc_out, bool *mnc_3_digits_out)
{
	/* Junk beforehand shows whether a failed call writes its results. */
	uint16_t mnc = JUNK_U16, mcc = JUNK_U16;
	bool mnc_3_digits = true;
	int rc = osmo_mnc_from_str(input, &mnc, &mnc_3_digits);
	int mcc_rc = osmo_mcc_from_str(input, &mcc);
	FILE *f = sections[FROM_STR];

	start(FROM_STR, "from_str");
	fprintf(f, ",");
	hex_text(f, "input", input);
	fprintf(f, ",\"rc\":%d,\"mnc\":%u,\"mnc_3_digits\":%u,\"mcc_rc\":%d,\"mcc\":%u}\n", rc, mnc,
		mnc_3_digits, mcc_rc, mcc);
	CHECK(rc == mcc_rc);
	if (mnc_out)
		*mnc_out = mnc;
	if (mnc_3_digits_out)
		*mnc_3_digits_out = mnc_3_digits;
	return rc;
}

/* ---- Names ---- */

/* Every name variant of one value must agree; the record holds the text once. */
#define SAME_NAMES(name, name2, buf_fn, c_fn, value)					\
	do {										\
		char buf_[OUT_SIZE];							\
		char *c_;								\
		memset(buf_, GUARD, sizeof(buf_));					\
		CHECK(buf_fn(buf_, sizeof(buf_), value) == buf_);			\
		guard_untouched(buf_, strlen(buf_) + 1);				\
		CHECK(!strcmp(name, buf_));						\
		CHECK(!strcmp(name2, buf_));						\
		c_ = (char *)c_fn(ctx, value);						\
		CHECK(c_ && !strcmp(c_, buf_));						\
		talloc_free(c_);							\
	} while (0)

static void name_text(const char *text)
{
	fprintf(sections[NAME], ",\"text\":\"%s\"}\n", text);
}

static void record_mcc_name(uint16_t mcc)
{
	const char *text = osmo_mcc_name(mcc);
	char buf[OUT_SIZE];
	char *c;

	memset(buf, GUARD, sizeof(buf));
	CHECK(osmo_mcc_name_buf(buf, sizeof(buf), mcc) == buf);
	guard_untouched(buf, strlen(buf) + 1);
	CHECK(!strcmp(text, buf));
	c = (char *)osmo_mcc_name_c(ctx, mcc);
	CHECK(c && !strcmp(c, buf));
	talloc_free(c);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"mcc\",\"mcc\":%u", mcc);
	name_text(buf);
}

static void record_mnc_name(uint16_t mnc, bool mnc_3_digits)
{
	const char *text = osmo_mnc_name(mnc, mnc_3_digits);
	char buf[OUT_SIZE];
	char *c;

	memset(buf, GUARD, sizeof(buf));
	CHECK(osmo_mnc_name_buf(buf, sizeof(buf), mnc, mnc_3_digits) == buf);
	guard_untouched(buf, strlen(buf) + 1);
	CHECK(!strcmp(text, buf));
	c = osmo_mnc_name_c(ctx, mnc, mnc_3_digits);
	CHECK(c && !strcmp(c, buf));
	talloc_free(c);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"mnc\",\"mnc\":%u,\"mnc_3_digits\":%u", mnc, mnc_3_digits);
	name_text(buf);
}

static void record_plmn_name(const struct osmo_plmn_id *p)
{
	SAME_NAMES(osmo_plmn_name(p), osmo_plmn_name2(p), osmo_plmn_name_buf, osmo_plmn_name_c, p);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"plmn\"");
	plmn_fields(sections[NAME], "", p);
	name_text(osmo_plmn_name(p));
}

static void record_lai_name(const struct osmo_location_area_id *l)
{
	SAME_NAMES(osmo_lai_name(l), osmo_lai_name(l), osmo_lai_name_buf, osmo_lai_name_c, l);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"lai\"");
	lai_fields(sections[NAME], "", l);
	name_text(osmo_lai_name(l));
}

static void record_rai_name(const struct osmo_routing_area_id *r)
{
	SAME_NAMES(osmo_rai_name2(r), osmo_rai_name2(r), osmo_rai_name2_buf, osmo_rai_name2_c, r);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"rai\"");
	rai_fields(sections[NAME], "", r);
	name_text(osmo_rai_name2(r));
}

static void record_cgi_name(const struct osmo_cell_global_id *c)
{
	SAME_NAMES(osmo_cgi_name(c), osmo_cgi_name2(c), osmo_cgi_name_buf, osmo_cgi_name_c, c);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"cgi\"");
	cgi_fields(sections[NAME], "", c);
	name_text(osmo_cgi_name(c));
}

static void record_cgi_ps_name(const struct osmo_cell_global_id_ps *c)
{
	SAME_NAMES(osmo_cgi_ps_name(c), osmo_cgi_ps_name2(c), osmo_cgi_ps_name_buf, osmo_cgi_ps_name_c, c);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"cgi_ps\"");
	cgi_ps_fields(sections[NAME], "", c);
	name_text(osmo_cgi_ps_name(c));
}

static void record_sai_name(const struct osmo_service_area_id *s)
{
	SAME_NAMES(osmo_sai_name(s), osmo_sai_name2(s), osmo_sai_name_buf, osmo_sai_name_c, s);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"sai\"");
	sai_fields(sections[NAME], "", s);
	name_text(osmo_sai_name(s));
}

static void record_rnc_id_name(const struct osmo_rnc_id *r)
{
	SAME_NAMES(osmo_rnc_id_name(r), osmo_rnc_id_name(r), osmo_rnc_id_name_buf, osmo_rnc_id_name_c, r);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"rnc_id\"");
	rnc_id_fields(sections[NAME], "", r);
	name_text(osmo_rnc_id_name(r));
}

static void record_gummei_name(const struct osmo_gummei *g)
{
	SAME_NAMES(osmo_gummei_name(g), osmo_gummei_name(g), osmo_gummei_name_buf, osmo_gummei_name_c, g);
	start(NAME, "name");
	fprintf(sections[NAME], ",\"type\":\"gummei\"");
	gummei_fields(sections[NAME], "", g);
	name_text(osmo_gummei_name(g));
}

/* ---- Names in short buffers ---- */

static void name_buf_record(const char *buf, size_t buf_len)
{
	FILE *f = sections[NAME_BUF];

	guard_untouched(buf, buf_len);
	fprintf(f, ",\"buf_len\":%zu,", buf_len);
	hex_bytes(f, "buf", buf, buf_len);
	fprintf(f, "}\n");
}

/* Each length from 0 to one past the name, so the cut and its NUL show. */
static void sweep_name_bufs(void)
{
	const struct osmo_plmn_id p = plmn(262, 42, false);
	const struct osmo_cell_global_id_ps cgi_ps = { .rai = rai(plmn(901, 70, true), 65535, 255),
						      .cell_identity = 65535 };
	const struct osmo_gummei gummei = { .plmn = plmn(1, 1, false),
					    .mme = { .group_id = 0xa123, .code = 0xb1 } };
	char buf[OUT_SIZE];

	for (size_t len = 0; len <= strlen(osmo_plmn_name(&p)) + 1; len++) {
		memset(buf, GUARD, sizeof(buf));
		CHECK(osmo_plmn_name_buf(buf, len, &p) == buf);
		start(NAME_BUF, "name_buf");
		fprintf(sections[NAME_BUF], ",\"type\":\"plmn\"");
		plmn_fields(sections[NAME_BUF], "", &p);
		name_buf_record(buf, len);
	}
	for (size_t len = 0; len <= strlen(osmo_cgi_ps_name(&cgi_ps)) + 1; len++) {
		memset(buf, GUARD, sizeof(buf));
		CHECK(osmo_cgi_ps_name_buf(buf, len, &cgi_ps) == buf);
		start(NAME_BUF, "name_buf");
		fprintf(sections[NAME_BUF], ",\"type\":\"cgi_ps\"");
		cgi_ps_fields(sections[NAME_BUF], "", &cgi_ps);
		name_buf_record(buf, len);
	}
	for (size_t len = 0; len <= strlen(osmo_gummei_name(&gummei)) + 1; len++) {
		memset(buf, GUARD, sizeof(buf));
		CHECK(osmo_gummei_name_buf(buf, len, &gummei) == buf);
		start(NAME_BUF, "name_buf");
		fprintf(sections[NAME_BUF], ",\"type\":\"gummei\"");
		gummei_fields(sections[NAME_BUF], "", &gummei);
		name_buf_record(buf, len);
	}
}

/* ---- Comparisons ---- */

static void cmp_result(int rc)
{
	fprintf(sections[CMP], ",\"result\":%d}\n", rc);
}

static void record_mnc_cmp(uint16_t a, bool a3, uint16_t b, bool b3)
{
	FILE *f = sections[CMP];

	start(CMP, "cmp");
	fprintf(f, ",\"type\":\"mnc\",\"a_mnc\":%u,\"a_mnc_3_digits\":%u,\"b_mnc\":%u,\"b_mnc_3_digits\":%u",
		a, a3, b, b3);
	cmp_result(osmo_mnc_cmp(a, a3, b, b3));
}

#define RECORD_CMP(type, fields_fn, cmp_fn, a, b)					\
	do {										\
		start(CMP, "cmp");							\
		fprintf(sections[CMP], ",\"type\":\"%s\"", type);			\
		fields_fn(sections[CMP], "a_", a);					\
		fields_fn(sections[CMP], "b_", b);					\
		cmp_result(cmp_fn(a, b));						\
	} while (0)

/* ---- Domain names ---- */

static void gen_result(int rc, const char *out)
{
	FILE *f = sections[GEN];

	fprintf(f, ",\"rc\":%d,", rc);
	if (rc < 0) {
		guard_untouched(out, 0);
		fprintf(f, "\"out\":null}\n");
		return;
	}
	CHECK((size_t)rc == strlen(out));
	guard_untouched(out, (size_t)rc + 1);
	hex_text(f, "out", out);
	fprintf(f, "}\n");
}

static int record_gen_home(const struct osmo_plmn_id *p, char *out)
{
	int rc;

	memset(out, GUARD, OUT_SIZE);
	rc = osmo_gen_home_network_domain(out, p);
	start(GEN, "gen");
	fprintf(sections[GEN], ",\"type\":\"home\"");
	plmn_fields(sections[GEN], "", p);
	gen_result(rc, out);
	return rc;
}

static int record_gen_mme(const struct osmo_gummei *g, char *out)
{
	int rc;

	memset(out, GUARD, OUT_SIZE);
	rc = osmo_gen_mme_domain(out, g);
	start(GEN, "gen");
	fprintf(sections[GEN], ",\"type\":\"mme\"");
	gummei_fields(sections[GEN], "", g);
	gen_result(rc, out);
	return rc;
}

static void record_gen_mme_group(uint16_t mmegi, const struct osmo_plmn_id *p)
{
	char out[OUT_SIZE];
	int rc;

	memset(out, GUARD, OUT_SIZE);
	rc = osmo_gen_mme_group_domain(out, mmegi, p);
	start(GEN, "gen");
	fprintf(sections[GEN], ",\"type\":\"mme_group\",\"group_id\":%u", mmegi);
	plmn_fields(sections[GEN], "", p);
	gen_result(rc, out);
}

/* The output holds junk beforehand; both parsers clear it first. */
static int record_parse_home(const char *input, struct osmo_plmn_id *out)
{
	int rc;

	*out = plmn(JUNK_U16, JUNK_U16, true);
	rc = osmo_parse_home_network_domain(out, input);
	start(PARSE, "parse");
	fprintf(sections[PARSE], ",\"type\":\"home\",");
	hex_text(sections[PARSE], "input", input);
	fprintf(sections[PARSE], ",\"rc\":%d", rc);
	plmn_fields(sections[PARSE], "", out);
	fprintf(sections[PARSE], "}\n");
	return rc;
}

static int record_parse_mme(const char *input, struct osmo_gummei *out)
{
	int rc;

	out->plmn = plmn(JUNK_U16, JUNK_U16, true);
	out->mme.group_id = JUNK_U16;
	out->mme.code = 0x42;
	rc = osmo_parse_mme_domain(out, input);
	start(PARSE, "parse");
	fprintf(sections[PARSE], ",\"type\":\"mme\",");
	hex_text(sections[PARSE], "input", input);
	fprintf(sections[PARSE], ",\"rc\":%d", rc);
	gummei_fields(sections[PARSE], "", out);
	fprintf(sections[PARSE], "}\n");
	return rc;
}

/* ---- Upstream's test, replayed ---- */

static const struct {
	const char *imsi;
	bool expect_ok;
} upstream_imsis[] = {
	{ "", false }, { " ", false }, { "1", false }, { "123", false }, { "12345", false },
	{ "123456", true }, { "1234567", true }, { "1234567890123", true },
	{ "123456789012345", true }, { "000000000000000", true }, { "999999999999999", true },
	{ "1234567890123456", false }, { "a23456789012345", false }, { "1234567b9012345", false },
	{ "12345678901234c", false }, { "123456789 01234", false }, { "1234567\n123456", false },
	{ "123456\t123456", false }, { "123456\r123456", false }, { NULL, false },
};

static const struct {
	const char *msisdn;
	bool expect_ok;
} upstream_msisdns[] = {
	{ "", false }, { " ", false }, { "1", true }, { "123", true }, { "12345", true },
	{ "123456", true }, { "1234567", true }, { "1234567890123", true },
	{ "123456789012345", true }, { "000000000000000", true }, { "999999999999999", true },
	{ "1234567890123456", false }, { "a23456789012345", false }, { "1234567b9012345", false },
	{ "12345678901234c", false }, { "123456789 01234", false }, { "1234567\n123456", false },
	{ "123456\t123456", false }, { "123456\r123456", false }, { NULL, false },
};

static const struct {
	bool with_15th_digit;
	const char *imei;
	bool expect_ok;
} upstream_imeis[] = {
	{ false, "12345678901234", true }, { false, "1234567890123", false },
	{ false, "123456789012345", false }, { true, "357613004448485", true },
	{ true, "357805023984447", true }, { true, "352936001349777", true },
	{ true, "357663017768551", true }, { true, "357613004448480", false },
	{ true, "357613004448405", false }, { true, "357613004448085", false },
	{ false, NULL, false },
};

static const struct {
	const char *mnc_str;
	int rc;
	uint16_t mnc;
	bool mnc_3_digits;
} upstream_mncs[] = {
	{ "0", 0, 0, false }, { "00", 0, 0, false }, { "000", 0, 0, true }, { "1", 0, 1, false },
	{ "01", 0, 1, false }, { "001", 0, 1, true }, { "", -EINVAL, 0, false },
	{ " ", -EINVAL, 0, false }, { "-1", -EINVAL, 0, false }, { "1000", -EINVAL, 0, false },
	{ "0x", -EINVAL, 0, false }, { " 23", -EINVAL, 0, false }, { "23 ", -EINVAL, 0, false },
	{ " 023", -EINVAL, 0, false }, { "023 ", -EINVAL, 0, false }, { "023 ", -EINVAL, 0, false },
};

static const char *bool_str(bool b)
{
	return b ? "true" : "false";
}

static void upstream_replay(const char *ok_path)
{
	static const struct osmo_gummei gummei = {
		.plmn = { .mcc = 901, .mnc = 70 },
		.mme = { .group_id = 0xA123, .code = 0xB1 },
	};
	FILE *f, *file;
	char out[OUT_SIZE];
	struct osmo_plmn_id parsed_plmn;
	struct osmo_gummei parsed_gummei;
	char *expected;
	size_t expected_size;

	upstream_out = open_memstream(&upstream_text, &upstream_size);
	CHECK(upstream_out);
	f = upstream_out;
	source = "upstream";

	fprintf(f, "----- test_valid_imsi\n");
	for (size_t i = 0; i < ARRAY_SIZE(upstream_imsis); i++) {
		bool ok = record_valid("imsi", upstream_imsis[i].imsi);
		CHECK(ok == upstream_imsis[i].expect_ok);
		fprintf(f, "%2zu: expect=%s result=%s imsi='%s'\n", i,
			bool_str(upstream_imsis[i].expect_ok), bool_str(ok),
			upstream_imsis[i].imsi ? upstream_imsis[i].imsi : "(null)");
	}
	fprintf(f, "----- test_valid_msisdn\n");
	for (size_t i = 0; i < ARRAY_SIZE(upstream_msisdns); i++) {
		bool ok = record_valid("msisdn", upstream_msisdns[i].msisdn);
		CHECK(ok == upstream_msisdns[i].expect_ok);
		fprintf(f, "%2zu: expect=%s result=%s msisdn='%s'\n", i,
			bool_str(upstream_msisdns[i].expect_ok), bool_str(ok),
			upstream_msisdns[i].msisdn ? upstream_msisdns[i].msisdn : "(null)");
	}
	fprintf(f, "----- test_valid_imei\n");
	for (size_t i = 0; i < ARRAY_SIZE(upstream_imeis); i++) {
		bool ok = record_valid(upstream_imeis[i].with_15th_digit ? "imei_check" : "imei",
				       upstream_imeis[i].imei);
		CHECK(ok == upstream_imeis[i].expect_ok);
		fprintf(f, "%2zu: expect=%s result=%s imei='%s' with_15th_digit=%s\n", i,
			bool_str(upstream_imeis[i].expect_ok), bool_str(ok),
			upstream_imeis[i].imei ? upstream_imeis[i].imei : "(null)",
			bool_str(upstream_imeis[i].with_15th_digit));
	}
	fprintf(f, "----- test_mnc_from_str\n");
	for (size_t i = 0; i < ARRAY_SIZE(upstream_mncs); i++) {
		/* Upstream's results start zeroed; a failed call leaves them so. */
		uint16_t mnc = 0;
		bool mnc_3_digits = false;
		int rc = osmo_mnc_from_str(upstream_mncs[i].mnc_str, &mnc, &mnc_3_digits);
		uint16_t recorded_mnc;
		bool recorded_3_digits;
		bool ok = rc == upstream_mncs[i].rc && mnc == upstream_mncs[i].mnc
			  && mnc_3_digits == upstream_mncs[i].mnc_3_digits;

		CHECK(ok);
		CHECK(record_from_str(upstream_mncs[i].mnc_str, &recorded_mnc, &recorded_3_digits) == rc);
		CHECK(rc != 0 || (recorded_mnc == mnc && recorded_3_digits == mnc_3_digits));
		fprintf(f, "%2zu: \"%s\" rc=%d mnc=%u mnc_3_digits=%u %s\n", i,
			osmo_escape_str(upstream_mncs[i].mnc_str, -1), rc, mnc, mnc_3_digits,
			ok ? "pass" : "FAIL");
	}

	record_gummei_name(&gummei);
	fprintf(f, "%s\n", osmo_gummei_name(&gummei));
	CHECK(!strcmp(osmo_gummei_name(&gummei), "901-70-a123-b1"));

	record_plmn_name(&gummei.plmn);
	CHECK(record_gen_home(&gummei.plmn, out) >= 0);
	fprintf(f, "%s -> %s\n", osmo_plmn_name(&gummei.plmn), out);
	CHECK(!strcmp(out, "epc.mnc070.mcc901.3gppnetwork.org"));
	CHECK(record_gen_mme(&gummei, out) >= 0);
	fprintf(f, "%s -> %s\n", osmo_gummei_name(&gummei), out);
	CHECK(!strcmp(out, "mmecb1.mmegia123.mme.epc.mnc070.mcc901.3gppnetwork.org"));

	CHECK(record_parse_home("epc.mnc070.mcc901.3gppnetwork.org", &parsed_plmn) == 0);
	record_plmn_name(&parsed_plmn);
	fprintf(f, "%s -> %s\n", "epc.mnc070.mcc901.3gppnetwork.org", osmo_plmn_name(&parsed_plmn));
	CHECK(parsed_plmn.mcc == 901 && parsed_plmn.mnc == 70);
	CHECK(record_parse_mme("mmec01.mmegiA001.mme.epc.mnc070.mcc901.3gppnetwork.org",
			       &parsed_gummei) == 0);
	record_gummei_name(&parsed_gummei);
	fprintf(f, "%s -> %s\n", "mmec01.mmegiA001.mme.epc.mnc070.mcc901.3gppnetwork.org",
		osmo_gummei_name(&parsed_gummei));
	CHECK(parsed_gummei.plmn.mcc == 901 && parsed_gummei.plmn.mnc == 70
	      && parsed_gummei.mme.group_id == 0xA001 && parsed_gummei.mme.code == 1);

	fclose(upstream_out);
	file = fopen(ok_path, "r");
	if (!file)
		fail("upstream output");
	expected = malloc(upstream_size + 2);
	CHECK(expected);
	expected_size = fread(expected, 1, upstream_size + 2, file);
	fclose(file);
	if (expected_size != upstream_size || memcmp(expected, upstream_text, upstream_size))
		fail("gsm23003_test.ok differs from the harness's calls");
	free(expected);
	free(upstream_text);
	source = "sweep";
}

/* ---- Sweeps beyond upstream's ---- */

static const uint16_t mccs[] = { 0, 1, 9, 10, 99, 100, 262, 901, 999, 1000, 65535 };
static const uint16_t mncs[] = { 0, 1, 9, 10, 70, 99, 100, 999, 1000, 65535 };
/* A few PLMNs, each with both MNC flags where the MNC is under 100. */
static const struct osmo_plmn_id some_plmns[] = {
	{ 1, 1, false }, { 1, 1, true }, { 262, 42, false }, { 901, 70, true },
	{ 999, 999, true }, { 65535, 65535, false },
};
static const uint16_t u16_edges[] = { 0, 1, 255, 256, 65534, 65535 };
static const uint8_t rac_edges[] = { 0, 1, 127, 128, 255 };

static void sweep_valid(void)
{
	static const char digits[] = "12345678901234567890";
	static const char *const others[] = { " ", "a", "/", ":", "+", "-", "\x80", "\xff", "\t" };
	static const char *const checks[] = { "imsi", "msisdn", "imei", "imei_check" };
	char text[32];
	uint32_t state = 0x23003;

	for (size_t c = 0; c < ARRAY_SIZE(checks); c++) {
		for (size_t len = 0; len <= 17; len++) {
			memcpy(text, digits, len);
			text[len] = 0;
			record_valid(checks[c], text);
		}
		/* A non-digit first, in the middle and last, at the shortest and longest valid
		 * lengths of each check. */
		for (size_t o = 0; o < ARRAY_SIZE(others); o++) {
			const size_t lens[] = { 1, 6, 14, 15 };
			for (size_t l = 0; l < ARRAY_SIZE(lens); l++) {
				size_t len = lens[l];
				const size_t positions[] = { 0, len / 2, len - 1 };
				for (size_t p = 0; p < ARRAY_SIZE(positions); p++) {
					if (p && positions[p] == positions[p - 1])
						continue;
					memcpy(text, digits, len);
					text[len] = 0;
					text[positions[p]] = others[o][0];
					record_valid(checks[c], text);
				}
			}
		}
		record_valid(checks[c], NULL);
	}
	/* Every check digit after some 14-digit IMEIs: exactly one passes the Luhn check. */
	for (int base = 0; base < 10; base++) {
		int passed = 0;
		if (base < 2) {
			strcpy(text, base ? "35761300444848" : "00000000000000");
		} else {
			for (int i = 0; i < 14; i++)
				text[i] = (char)('0' + next_random(&state) % 10);
			text[14] = 0;
		}
		for (char check = '0'; check <= '9'; check++) {
			text[14] = check;
			text[15] = 0;
			passed += record_valid("imei_check", text);
		}
		CHECK(passed == 1);
	}
}

static void sweep_from_str(void)
{
	static const char *const inputs[] = {
		"2", "02", "002", "9", "09", "009", "10", "010", "99", "099", "100", "999", "0999",
		"9999", "00000", "+1", "+12", "1a", "a1", "1 ", "\t1", "1\t", "0x1", "08", "1.",
		"1,", "\xef\xbc\x91", "\x80", "12\n",
	};

	for (size_t i = 0; i < ARRAY_SIZE(inputs); i++)
		record_from_str(inputs[i], NULL, NULL);
	record_from_str(NULL, NULL, NULL);
}

static void sweep_names(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(mccs); i++)
		record_mcc_name(mccs[i]);
	for (size_t i = 0; i < ARRAY_SIZE(mncs); i++)
		for (int three = 0; three <= 1; three++)
			record_mnc_name(mncs[i], three);
	for (size_t i = 0; i < ARRAY_SIZE(mccs); i++)
		for (size_t j = 0; j < ARRAY_SIZE(mncs); j++)
			for (int three = 0; three <= 1; three++) {
				struct osmo_plmn_id p = plmn(mccs[i], mncs[j], three);
				record_plmn_name(&p);
			}
	for (size_t i = 0; i < ARRAY_SIZE(some_plmns); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(u16_edges); j++) {
			struct osmo_location_area_id l = lai(some_plmns[i], u16_edges[j]);
			struct osmo_cell_global_id c = { .lai = l, .cell_identity = u16_edges[j] };
			struct osmo_service_area_id s = { .lai = l, .sac = u16_edges[j] };
			struct osmo_rnc_id r = { .plmn = some_plmns[i], .rnc_id = u16_edges[j] };

			record_lai_name(&l);
			record_cgi_name(&c);
			record_sai_name(&s);
			record_rnc_id_name(&r);
			for (size_t k = 0; k < ARRAY_SIZE(rac_edges); k++) {
				struct osmo_routing_area_id ra = rai(some_plmns[i], u16_edges[j], rac_edges[k]);
				record_rai_name(&ra);
			}
		}
		for (size_t j = 0; j < 2; j++)
			for (size_t k = 0; k < 2; k++)
				for (size_t m = 0; m < 3; m++) {
					const uint16_t lacs[] = { 0, 65535 }, cis[] = { 0, 1, 65535 };
					const uint8_t racs[] = { 0, 255 };
					struct osmo_cell_global_id_ps c = {
						.rai = rai(some_plmns[i], lacs[j], racs[k]),
						.cell_identity = cis[m],
					};
					record_cgi_ps_name(&c);
				}
		for (size_t j = 0; j < 4; j++)
			for (size_t k = 0; k < 4; k++) {
				const uint16_t groups[] = { 0, 1, 0xa123, 0xffff };
				const uint8_t codes[] = { 0, 1, 0xb1, 0xff };
				struct osmo_gummei g = { .plmn = some_plmns[i],
							 .mme = { .group_id = groups[j], .code = codes[k] } };
				record_gummei_name(&g);
			}
	}
}

static void sweep_cmps(void)
{
	/* Two values per field, so every field decides some comparison. */
	const struct osmo_plmn_id pa = plmn(1, 1, false), pb = plmn(1, 1, true);
	const struct osmo_plmn_id plmns[] = {
		plmn(1, 1, false), plmn(1, 1, true), plmn(1, 70, false), plmn(1, 100, false),
		plmn(1, 100, true), plmn(262, 1, false), plmn(262, 1, true), plmn(262, 70, false),
		plmn(262, 100, false), plmn(262, 100, true), plmn(1000, 1, false), plmn(65535, 65535, true),
	};
	struct osmo_location_area_id lais[4];
	struct osmo_routing_area_id rais[8];
	struct osmo_cell_global_id cgis[8];
	struct osmo_cell_global_id_ps cgi_pss[16];
	struct osmo_rnc_id rncs[4];
	struct osmo_mme_id mmes[9];
	struct osmo_gummei gummeis[8];
	size_t n;

	for (size_t i = 0; i < ARRAY_SIZE(mncs); i++)
		for (int ia = 0; ia <= 1; ia++)
			for (size_t j = 0; j < ARRAY_SIZE(mncs); j++)
				for (int jb = 0; jb <= 1; jb++)
					record_mnc_cmp(mncs[i], ia, mncs[j], jb);
	for (size_t i = 0; i < ARRAY_SIZE(plmns); i++)
		for (size_t j = 0; j < ARRAY_SIZE(plmns); j++)
			RECORD_CMP("plmn", plmn_fields, osmo_plmn_cmp, &plmns[i], &plmns[j]);
	/* The same PLMN by pointer, which C short-cuts. */
	RECORD_CMP("plmn", plmn_fields, osmo_plmn_cmp, &plmns[0], &plmns[0]);

	n = 0;
	for (int p = 0; p < 2; p++)
		for (int l = 1; l <= 2; l++)
			lais[n++] = lai(p ? pb : pa, l);
	for (size_t i = 0; i < n; i++)
		for (size_t j = 0; j < n; j++)
			RECORD_CMP("lai", lai_fields, osmo_lai_cmp, &lais[i], &lais[j]);

	n = 0;
	for (int p = 0; p < 2; p++)
		for (int l = 1; l <= 2; l++)
			for (int r = 1; r <= 2; r++)
				rais[n++] = rai(p ? pb : pa, l, r);
	for (size_t i = 0; i < n; i++)
		for (size_t j = 0; j < n; j++)
			RECORD_CMP("rai", rai_fields, osmo_rai_cmp, &rais[i], &rais[j]);

	n = 0;
	for (int p = 0; p < 2; p++)
		for (int l = 1; l <= 2; l++)
			for (int c = 1; c <= 2; c++)
				cgis[n++] = (struct osmo_cell_global_id){ .lai = lai(p ? pb : pa, l),
									  .cell_identity = c };
	for (size_t i = 0; i < n; i++)
		for (size_t j = 0; j < n; j++)
			RECORD_CMP("cgi", cgi_fields, osmo_cgi_cmp, &cgis[i], &cgis[j]);

	n = 0;
	for (int p = 0; p < 2; p++)
		for (int l = 1; l <= 2; l++)
			for (int r = 1; r <= 2; r++)
				for (int c = 1; c <= 2; c++)
					cgi_pss[n++] = (struct osmo_cell_global_id_ps){
						.rai = rai(p ? pb : pa, l, r), .cell_identity = c };
	for (size_t i = 0; i < n; i++)
		for (size_t j = 0; j < n; j++)
			RECORD_CMP("cgi_ps", cgi_ps_fields, osmo_cgi_ps_cmp, &cgi_pss[i], &cgi_pss[j]);

	n = 0;
	for (int p = 0; p < 2; p++)
		for (int r = 1; r <= 2; r++)
			rncs[n++] = (struct osmo_rnc_id){ .plmn = p ? pb : pa, .rnc_id = r };
	for (size_t i = 0; i < n; i++)
		for (size_t j = 0; j < n; j++)
			RECORD_CMP("rnc_id", rnc_id_fields, osmo_rnc_id_cmp, &rncs[i], &rncs[j]);

	n = 0;
	for (int g = 0; g < 3; g++)
		for (int c = 0; c < 3; c++) {
			const uint16_t groups[] = { 0, 1, 0xffff };
			const uint8_t codes[] = { 0, 1, 0xff };
			mmes[n++] = (struct osmo_mme_id){ .group_id = groups[g], .code = codes[c] };
		}
	for (size_t i = 0; i < n; i++)
		for (size_t j = 0; j < n; j++)
			RECORD_CMP("mme_id", mme_id_fields, osmo_mme_id_cmp, &mmes[i], &mmes[j]);

	n = 0;
	for (int p = 0; p < 2; p++)
		for (int g = 1; g <= 2; g++)
			for (int c = 1; c <= 2; c++)
				gummeis[n++] = (struct osmo_gummei){
					.plmn = p ? pb : pa, .mme = { .group_id = g, .code = c } };
	for (size_t i = 0; i < n; i++)
		for (size_t j = 0; j < n; j++)
			RECORD_CMP("gummei", gummei_fields, osmo_gummei_cmp, &gummeis[i], &gummeis[j]);
}

static void sweep_gen(void)
{
	char out[OUT_SIZE];

	for (size_t i = 0; i < ARRAY_SIZE(mccs); i++)
		for (size_t j = 0; j < ARRAY_SIZE(mncs); j++)
			for (int three = 0; three <= 1; three++) {
				struct osmo_plmn_id p = plmn(mccs[i], mncs[j], three);
				record_gen_home(&p, out);
			}
	for (size_t i = 0; i < ARRAY_SIZE(some_plmns); i++) {
		const uint16_t groups[] = { 0, 1, 0xa123, 0xffff };
		const uint8_t codes[] = { 0, 1, 0xb1, 0xff };
		for (size_t j = 0; j < ARRAY_SIZE(groups); j++) {
			record_gen_mme_group(groups[j], &some_plmns[i]);
			for (size_t k = 0; k < ARRAY_SIZE(codes); k++) {
				struct osmo_gummei g = { .plmn = some_plmns[i],
							 .mme = { .group_id = groups[j], .code = codes[k] } };
				record_gen_mme(&g, out);
			}
		}
	}
	/* An MCC or MNC past three digits fails, whatever the other is. */
	{
		struct osmo_gummei g = { .plmn = plmn(1000, 1, false), .mme = { 1, 1 } };
		record_gen_mme(&g, out);
		record_gen_mme_group(1, &g.plmn);
		g.plmn = plmn(1, 1000, false);
		record_gen_mme(&g, out);
		record_gen_mme_group(1, &g.plmn);
	}
}

static void sweep_parse(void)
{
	static const char *const homes[] = {
		"epc.mnc001.mcc001.3gppnetwork.org", "epc.mnc000.mcc000.3gppnetwork.org",
		"epc.mnc999.mcc999.3gppnetwork.org", "epc.mnc100.mcc262.3gppnetwork.org",
		"epc.mnc70.mcc901.3gppnetwork.org", "epc.mnc7.mcc9.3gppnetwork.org",
		"epc.mnc0700.mcc901.3gppnetwork.org", "epc.mnc070.mcc9010.3gppnetwork.org",
		"epc.mnc070.mcc901.example.com", "epc.mnc070.mcc901", "epc.mnc070.mcc901.",
		"epc.mnc070.mcc901.3gppnetwork.org.extra", "EPC.MNC070.MCC901.3GPPNETWORK.ORG",
		"epc.mnc-01.mcc901.3gppnetwork.org", "epc.mnc-1.mcc-1.3gppnetwork.org",
		"epc.mnc+70.mcc901.3gppnetwork.org", "epc.mnc 70.mcc901.3gppnetwork.org",
		"epc.mnc070.mcc 901.3gppnetwork.org", "epc.mnc07a.mcc901.3gppnetwork.org",
		"epc.mncabc.mcc901.3gppnetwork.org", "epc.mnc070.mcc.3gppnetwork.org",
		"epc.mnc070", "epc.mnc", "epc.", "", "x", " epc.mnc070.mcc901.3gppnetwork.org",
		"mmec01.mmegia001.mme.epc.mnc070.mcc901.3gppnetwork.org",
	};
	static const char *const mmes[] = {
		"mmec01.mmegia001.mme.epc.mnc070.mcc901.3gppnetwork.org",
		"mmecb1.mmegia123.mme.epc.mnc070.mcc901.3gppnetwork.org",
		"mmecB1.mmegiA123.mme.epc.mnc070.mcc901.3gppnetwork.org",
		"mmec00.mmegi0000.mme.epc.mnc000.mcc000.3gppnetwork.org",
		"mmecff.mmegiffff.mme.epc.mnc999.mcc999.3gppnetwork.org",
		"mmec1.mmegi1.mme.epc.mnc70.mcc901.3gppnetwork.org",
		"mmec001.mmegi0001.mme.epc.mnc070.mcc901.3gppnetwork.org",
		"mmec01.mmegi00001.mme.epc.mnc070.mcc901.3gppnetwork.org",
		"mmec0x.mmegi0001.mme.epc.mnc070.mcc901.3gppnetwork.org",
		"mmec-1.mmegi-1.mme.epc.mnc070.mcc901.3gppnetwork.org",
		"mmecg1.mmegi0001.mme.epc.mnc070.mcc901.3gppnetwork.org",
		"mmec01.mmegi0001.epc.mnc070.mcc901.3gppnetwork.org",
		"mmec01.mmegi0001.mme.epc.mnc070.mcc901.example.com",
		"mmec01.mmegi0001.mme.epc.mnc070.mcc901",
		"mmec01.mmegi0001.mme.epc.mnc070", "mmec01", "mmec", "", "x",
		"epc.mnc070.mcc901.3gppnetwork.org",
	};
	struct osmo_plmn_id p;
	struct osmo_gummei g;

	for (size_t i = 0; i < ARRAY_SIZE(homes); i++)
		record_parse_home(homes[i], &p);
	for (size_t i = 0; i < ARRAY_SIZE(mmes); i++)
		record_parse_mme(mmes[i], &g);
}

static void constants(void)
{
	static const struct {
		const char *name;
		int value;
	} values[] = {
		{ "GSM23003_IMSI_MIN_DIGITS", GSM23003_IMSI_MIN_DIGITS },
		{ "GSM23003_IMSI_MAX_DIGITS", GSM23003_IMSI_MAX_DIGITS },
		{ "GSM23003_MSISDN_MIN_DIGITS", GSM23003_MSISDN_MIN_DIGITS },
		{ "GSM23003_MSISDN_MAX_DIGITS", GSM23003_MSISDN_MAX_DIGITS },
		{ "GSM23003_IMEI_NUM_DIGITS", GSM23003_IMEI_NUM_DIGITS },
		{ "GSM23003_IMEI_NUM_DIGITS_NO_CHK", GSM23003_IMEI_NUM_DIGITS_NO_CHK },
		{ "GSM23003_HOME_NETWORK_DOMAIN_LEN", GSM23003_HOME_NETWORK_DOMAIN_LEN },
		{ "GSM23003_MME_DOMAIN_LEN", GSM23003_MME_DOMAIN_LEN },
		{ "EINVAL", EINVAL },
	};

	for (size_t i = 0; i < ARRAY_SIZE(values); i++)
		printf("{\"kind\":\"constant\",\"name\":\"%s\",\"value\":%d}\n", values[i].name,
		       values[i].value);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s gsm23003_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
	ctx = talloc_named_const(NULL, 0, "gsm23003_vector");
	CHECK(ctx);
	for (size_t i = 0; i < SECTION_COUNT; i++) {
		sections[i] = open_memstream(&section_text[i], &section_size[i]);
		CHECK(sections[i]);
	}

	upstream_replay(argv[1]);
	sweep_valid();
	sweep_from_str();
	sweep_names();
	sweep_name_bufs();
	sweep_cmps();
	sweep_gen();
	sweep_parse();

	printf("{\"schema\":\"gsm23003-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
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
