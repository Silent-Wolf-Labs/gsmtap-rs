/*
 * Reference harness for libosmogsm's TLV parser and encoder.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one
 * record per call. Value bytes follow a fixed pattern, so records name a length
 * instead of repeating them. Output longer than LONG_OUTPUT is recorded as its
 * length and FNV-1a hash.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <talloc.h>

#include <osmocom/core/logging.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/tlv.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define LONG_OUTPUT 256
#define UNTOUCHED (-1)
#define IS_NULL (-2)
#define FILL 0xa5
/* Total size, and headroom for appending or prepending. */
#define MSG_SIZE 65000
#define PUT_HEADROOM 8000
#define PUSH_HEADROOM 55000

static uint8_t storage[0x10000];
static uint8_t values[0x10000];
static void *ctx;
static uint32_t seed = 0x2545f491;

static struct tlv_definition mixed_def;
static struct tlv_definition empty_def;

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

static void output(const char *field, const void *data, size_t len)
{
	const uint8_t *bytes = data;
	uint64_t hash = UINT64_C(0xcbf29ce484222325);
	size_t i;

	if (len <= LONG_OUTPUT) {
		hex(field, data, len);
		return;
	}
	for (i = 0; i < len; i++)
		hash = (hash ^ bytes[i]) * UINT64_C(0x100000001b3);
	printf(",\"%s\":\"@%zu:%016" PRIx64 "\"", field, len, hash);
}

/* The input goes first in storage, with fill after it for reads past the end. */
static const uint8_t *place(const void *input, size_t len)
{
	memset(storage, FILL, len + 16);
	memcpy(storage, input, len);
	return storage;
}

static const char *type_name(enum tlv_type type)
{
	switch (type) {
	case TLV_TYPE_NONE: return "none";
	case TLV_TYPE_FIXED: return "fixed";
	case TLV_TYPE_T: return "t";
	case TLV_TYPE_TV: return "tv";
	case TLV_TYPE_TLV: return "tlv";
	case TLV_TYPE_TL16V: return "tl16v";
	case TLV_TYPE_TvLV: return "tvlv";
	case TLV_TYPE_SINGLE_TV: return "single_tv";
	case TLV_TYPE_vTvLV_GAN: return "vtvlv_gan";
	}
	abort();
}

static void def_entries(const char *field, const struct tlv_definition *def)
{
	int tag, first = 1;

	printf(",\"%s\":\"", field);
	for (tag = 0; tag < 256; tag++) {
		if (def->def[tag].type == TLV_TYPE_NONE)
			continue;
		printf("%s%d %s %u", first ? "" : " ", tag, type_name(def->def[tag].type),
		       def->def[tag].fixed_len);
		first = 0;
	}
	printf("\"");
}

static void def_record(const char *name, const struct tlv_definition *def)
{
	printf("{\"kind\":\"def\",\"name\":\"%s\"", name);
	def_entries("entries", def);
	printf("}\n");
}

static void init_defs(void)
{
	static const struct { uint8_t tag; enum tlv_type type; uint8_t fixed; } mixed[] = {
		{ 0x01, TLV_TYPE_T, 0 },
		{ 0x02, TLV_TYPE_TV, 0 },
		{ 0x03, TLV_TYPE_FIXED, 3 },
		{ 0x04, TLV_TYPE_TLV, 0 },
		{ 0x05, TLV_TYPE_TL16V, 0 },
		{ 0x06, TLV_TYPE_TvLV, 0 },
		{ 0x07, TLV_TYPE_vTvLV_GAN, 0 },
		{ 0x08, TLV_TYPE_FIXED, 0 },
		{ 0x09, TLV_TYPE_FIXED, 255 },
		{ 0x0a, TLV_TYPE_SINGLE_TV, 0 },
		{ 0x20, TLV_TYPE_TLV, 0 },
		{ 0x80, TLV_TYPE_T, 0 },
		{ 0x81, TLV_TYPE_TL16V, 0 },
		{ 0xc0, TLV_TYPE_SINGLE_TV, 0 },
		{ 0xff, TLV_TYPE_TLV, 0 },
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(mixed); i++) {
		mixed_def.def[mixed[i].tag].type = mixed[i].type;
		mixed_def.def[mixed[i].tag].fixed_len = mixed[i].fixed;
	}
	for (i = 0; i < sizeof(values); i++)
		values[i] = (i * 37 + 0x5b) & 0xff;
}

static const struct tlv_definition *named_def(const char *name)
{
	if (!strcmp(name, "mixed"))
		return &mixed_def;
	if (!strcmp(name, "empty"))
		return &empty_def;
	if (!strcmp(name, "tvlv_att"))
		return &tvlv_att_def;
	if (!strcmp(name, "vtvlv_gan_att"))
		return &vtvlv_gan_att_def;
	abort();
}

static void parse_one_case(const char *def_name, const uint8_t *input, int len)
{
	static const uint8_t sentinel;
	const uint8_t *buf = place(input, len);
	const uint8_t *val = &sentinel;
	uint8_t tag = FILL;
	uint16_t value_len = 0xa5a5;
	long offset;
	int rc;

	rc = tlv_parse_one(&tag, &value_len, &val, named_def(def_name), buf, len);
	offset = val == &sentinel ? UNTOUCHED : val ? (long)(val - buf) : IS_NULL;
	printf("{\"kind\":\"parse_one\",\"def\":\"%s\"", def_name);
	hex("input", buf, len);
	printf(",\"rc\":%d,\"tag\":%u,\"len\":%u,\"val\":%ld}\n", rc, tag, value_len, offset);
}

static void parse_one_records(void)
{
	static const uint8_t seconds[] = { 0x00, 0x01, 0x02, 0x7f, 0x80, 0x81, 0xff };
	static const uint8_t thirds[] = { 0x00, 0x01, 0xff };
	static const int lengths[] = { 1, 2, 3, 4, 5, 6, 130, 259 };
	static const uint8_t att_tags[] = { 0x00, 0x01, 0x7f, 0x80, 0xff };
	static const char *const att_defs[] = { "tvlv_att", "vtvlv_gan_att" };
	uint8_t input[300] = { 0 };
	size_t s, t, l, a, i;
	int tag;

	parse_one_case("mixed", input, 0);
	parse_one_case("empty", input, 0);
	for (i = 0; i < sizeof(input); i++)
		input[i] = values[i];
	for (tag = 0; tag < 256; tag++) {
		input[0] = tag;
		input[1] = 2;
		input[2] = 0;
		for (l = 0; l < 4; l++)
			parse_one_case("mixed", input, lengths[l]);
		parse_one_case("empty", input, 3);
		if (!mixed_def.def[tag].type)
			continue;
		for (s = 0; s < ARRAY_SIZE(seconds); s++) {
			for (t = 0; t < ARRAY_SIZE(thirds); t++) {
				input[1] = seconds[s];
				input[2] = thirds[t];
				for (l = 0; l < ARRAY_SIZE(lengths); l++)
					parse_one_case("mixed", input, lengths[l]);
			}
		}
	}
	for (a = 0; a < ARRAY_SIZE(att_defs); a++) {
		parse_one_case(att_defs[a], input, 0);
		for (i = 0; i < ARRAY_SIZE(att_tags); i++) {
			input[0] = att_tags[i];
			for (s = 0; s < ARRAY_SIZE(seconds); s++) {
				for (t = 0; t < ARRAY_SIZE(thirds); t++) {
					input[1] = seconds[s];
					input[2] = thirds[t];
					for (l = 0; l < ARRAY_SIZE(lengths); l++)
						parse_one_case(att_defs[a], input, lengths[l]);
				}
			}
		}
	}
	/* Exact, one short and one over for two-byte lengths. */
	for (i = 0; i < 3; i++) {
		static const size_t sizes[] = { 0x7f, 0x80, 0xff, 0x100 };
		static const uint8_t tags[] = { 0x05, 0x06, 0x07, 0x81 };
		size_t z, g;
		uint8_t *large = malloc(0x8010);

		for (z = 0; z < ARRAY_SIZE(sizes); z++) {
			for (g = 0; g < ARRAY_SIZE(tags); g++) {
				size_t value_len = sizes[z];

				memcpy(large, values, 0x8010);
				large[0] = tags[g];
				/* 0x06 reads a clear high bit as two length bytes. */
				large[1] = tags[g] == 0x07 ? 0x80 | (value_len >> 8) : value_len >> 8;
				large[2] = value_len & 0xff;
				if (value_len + 3 + i - 1 <= 300)
					parse_one_case("mixed", large, value_len + 3 + i - 1);
			}
		}
		free(large);
	}
}

static void parsed_entries(const char *field, const struct tlv_parsed *dec, int multiples,
			   const uint8_t *buf)
{
	int i, tag, first = 1;

	printf(",\"%s\":\"", field);
	for (i = 0; i < multiples; i++) {
		for (tag = 0; tag < 256; tag++) {
			if (!dec[i].lv[tag].val)
				continue;
			printf("%s%d %d %ld %u", first ? "" : " ", i, tag,
			       (long)(dec[i].lv[tag].val - buf), dec[i].lv[tag].len);
			first = 0;
		}
	}
	printf("\"");
}

static const uint8_t mix_message[] = {
	0x01,
	0x02, 0xaa,
	0x03, 0xb1, 0xb2, 0xb3,
	0x04, 0x02, 0xc1, 0xc2,
	0x05, 0x00, 0x02, 0xd1, 0xd2,
	0x06, 0x82, 0xe1, 0xe2,
	0x06, 0x00, 0x02, 0xf1, 0xf2,
	0x07, 0x02, 0x11, 0x12,
	0x07, 0x80, 0x02, 0x13, 0x14,
	0x08,
	0xa7,
	0xc5,
	0x20, 0x01, 0x99,
	0x04, 0x00,
	0x01,
	0x02, 0xbb,
	0x04, 0x01, 0x77,
	0x04, 0x01, 0x78,
};

static void parse_case(const char *def_name, const uint8_t *input, int len, uint8_t lv1,
		       uint8_t lv2, int multiples)
{
	struct tlv_parsed dec[3];
	const uint8_t *buf = place(input, len);
	int rc;

	rc = tlv_parse2(dec, multiples, named_def(def_name), buf, len, lv1, lv2);
	printf("{\"kind\":\"parse\",\"def\":\"%s\"", def_name);
	hex("input", buf, len);
	printf(",\"lv1\":%u,\"lv2\":%u,\"multiples\":%d,\"rc\":%d", lv1, lv2, multiples, rc);
	parsed_entries("dec", dec, multiples, buf);
	printf("}\n");
}

static void parse_records(void)
{
	static const uint8_t lv_prefix[] = { 0x02, 0xaa, 0xbb, 0x01, 0xcc, 0x04, 0x01, 0x55 };
	static const uint8_t lvs[][2] = {
		{ 0x30, 0 }, { 0, 0x31 }, { 0x30, 0x31 }, { 0x30, 0x30 }, { 0x04, 0x05 }, { 0xff, 0x01 },
	};
	static const char *const defs[] = { "mixed", "tvlv_att", "vtvlv_gan_att", "empty" };
	uint8_t input[80];
	size_t i, l;
	int multiples, len, d;

	for (multiples = 0; multiples <= 3; multiples++) {
		for (len = 0; len <= (int)sizeof(mix_message); len++)
			parse_case("mixed", mix_message, len, 0, 0, multiples);
		for (l = 0; l < ARRAY_SIZE(lvs); l++) {
			for (len = 0; len <= (int)sizeof(lv_prefix); len++)
				parse_case("mixed", lv_prefix, len, lvs[l][0], lvs[l][1], multiples);
		}
	}
	for (i = 0; i < 400; i++) {
		size_t b;

		len = next_random() % 61;
		for (b = 0; b < (size_t)len; b++)
			input[b] = next_random() & 3 ? next_random() & 0x0f : next_random();
		d = i % 3 == 0 ? 0 : next_random() % ARRAY_SIZE(defs);
		parse_case(defs[d], input, len, i % 7 == 0 ? 0x30 : 0, i % 21 == 0 ? 0x31 : 0, i % 4);
	}
}

static void encode_one_case(enum tlv_type type, uint8_t tag, unsigned int len)
{
	struct msgb *msg = msgb_alloc_headroom(MSG_SIZE, PUT_HEADROOM, "tlv");
	int rc = tlv_encode_one(msg, type, tag, len, values);

	printf("{\"kind\":\"encode_one\",\"type\":\"%s\",\"tag\":%u,\"len\":%u,\"rc\":%d", type_name(type), tag, len, rc);
	output("out", msgb_data(msg), msgb_length(msg));
	printf("}\n");
	msgb_free(msg);
}

static void encode_one_records(void)
{
	static const enum tlv_type types[] = {
		TLV_TYPE_NONE, TLV_TYPE_FIXED, TLV_TYPE_T, TLV_TYPE_TV, TLV_TYPE_TLV, TLV_TYPE_TL16V,
		TLV_TYPE_TvLV, TLV_TYPE_SINGLE_TV, TLV_TYPE_vTvLV_GAN,
	};
	static const uint8_t tags[] = { 0x00, 0x05, 0x7f, 0x80, 0xff };
	static const unsigned int lengths[] = { 0, 1, 2, 0x7f, 0x80, 0xff, 0x100, 0x101, 0x7fff, 0x8000, 0x8001, 0xc000 };
	size_t t, g, l;

	for (t = 0; t < ARRAY_SIZE(types); t++)
		for (g = 0; g < ARRAY_SIZE(tags); g++)
			for (l = 0; l < ARRAY_SIZE(lengths); l++)
				encode_one_case(types[t], tags[g], lengths[l]);
}

static void encode_record(const char *def_name, const char *encode_def, const uint8_t *input, int len,
			  const uint8_t *order, unsigned int order_len)
{
	struct msgb *msg = msgb_alloc_headroom(MSG_SIZE, PUT_HEADROOM, "tlv");
	const uint8_t *buf = place(input, len);
	struct tlv_parsed tp;
	unsigned int i;
	int rc;

	if (tlv_parse(&tp, named_def(def_name), buf, len, 0, 0) < 0)
		abort();
	rc = order ? tlv_encode_ordered(msg, named_def(encode_def), &tp, order, order_len)
		   : tlv_encode(msg, named_def(encode_def), &tp);
	printf("{\"kind\":\"encode\",\"def\":\"%s\",\"encode_def\":\"%s\"", def_name, encode_def);
	hex("input", buf, len);
	printf(",\"ordered\":%d,\"order\":\"", order != NULL);
	for (i = 0; i < order_len; i++)
		printf("%s%u", i ? " " : "", order[i]);
	printf("\",\"rc\":%d", rc);
	output("out", msgb_data(msg), msgb_length(msg));
	printf("}\n");
	msgb_free(msg);
}

static void encode_records(void)
{
	static const uint8_t forward[] = { 1, 2, 3, 4, 5, 6, 7, 8, 0x0a, 0xc0, 0x20 };
	static const uint8_t backward[] = { 0x20, 0xc0, 0x0a, 8, 7, 6, 5, 4, 3, 2, 1 };
	static const uint8_t repeats[] = { 4, 4, 0x33, 1, 1 };
	static const uint8_t att[] = { 0x10, 0x82, 0x01, 0x02, 0x20, 0x00, 0x01, 0x03 };

	encode_record("mixed", "mixed", mix_message, sizeof(mix_message), NULL, 0);
	encode_record("mixed", "mixed", mix_message, sizeof(mix_message), forward, sizeof(forward));
	encode_record("mixed", "mixed", mix_message, sizeof(mix_message), backward, sizeof(backward));
	encode_record("mixed", "mixed", mix_message, sizeof(mix_message), repeats, sizeof(repeats));
	encode_record("mixed", "mixed", mix_message, sizeof(mix_message), forward, 0);
	encode_record("mixed", "empty", mix_message, sizeof(mix_message), NULL, 0);
	encode_record("mixed", "tvlv_att", mix_message, sizeof(mix_message), NULL, 0);
	encode_record("mixed", "vtvlv_gan_att", mix_message, sizeof(mix_message), NULL, 0);
	encode_record("tvlv_att", "tvlv_att", att, sizeof(att), NULL, 0);
	encode_record("tvlv_att", "mixed", att, sizeof(att), NULL, 0);
}

enum op_args { ARG_TAG8_LEN8, ARG_TAG8_LEN16, ARG_TAG16_LEN8, ARG_TAG16_LEN16, ARG_TAG8_NUM, ARG_NUM };

static long run_op(struct msgb *msg, const char *op, unsigned int tag, unsigned int len, uint32_t num)
{
	const uint8_t *start;

#define OP(name) (!strcmp(op, name))
	if (OP("lv")) msgb_lv_put(msg, len, values);
	else if (OP("tlv")) msgb_tlv_put(msg, tag, len, values);
	else if (OP("tlv_zeroed")) msgb_tlv_put(msg, tag, len, NULL);
	else if (OP("tl")) { start = msgb_tl_put(msg, tag); return start - msg->data; }
	else if (OP("tlv16")) msgb_tlv16_put(msg, tag, len, (const uint16_t *)(const void *)values);
	else if (OP("tl16v")) msgb_tl16v_put(msg, tag, len, values);
	else if (OP("t16lv")) msgb_t16lv_put(msg, tag, len, values);
	else if (OP("tvl")) { start = msgb_tvl_put(msg, tag, len); return start - msg->data; }
	else if (OP("tvlv")) msgb_tvlv_put(msg, tag, len, values);
	else if (OP("tvlv_16be")) msgb_tvlv_put_16be(msg, tag, num);
	else if (OP("tvlv_32be")) msgb_tvlv_put_32be(msg, tag, num);
	else if (OP("vtvlv_gan")) msgb_vtvlv_gan_put(msg, tag, len, values);
	else if (OP("l16tv")) msgb_l16tv_put(msg, len, tag, values);
	else if (OP("tv")) msgb_tv_put(msg, tag, num);
	else if (OP("tv_fixed")) msgb_tv_fixed_put(msg, tag, len, values);
	else if (OP("v")) msgb_v_put(msg, num);
	else if (OP("tv16")) msgb_tv16_put(msg, tag, num);
	else if (OP("tv32")) msgb_tv32_put(msg, tag, num);
	else if (OP("tlv_push")) msgb_tlv_push(msg, tag, len, values);
	else if (OP("tlv1_push")) msgb_tlv1_push(msg, tag, num);
	else if (OP("tv_push")) msgb_tv_push(msg, tag, num);
	else if (OP("tv16_push")) msgb_tv16_push(msg, tag, num);
	else if (OP("tv32_push")) msgb_tv32_push(msg, tag, num);
	else if (OP("tvlv_push")) msgb_tvlv_push(msg, tag, len, values);
	else if (OP("vtvl_gan_push")) msgb_vtvl_gan_push(msg, tag, len);
	else if (OP("vtvlv_gan_push")) msgb_vtvlv_gan_push(msg, tag, len, values);
	else abort();
#undef OP
	return UNTOUCHED;
}

static void msgb_case(const char *op, unsigned int tag, unsigned int len, uint32_t num)
{
	size_t op_len = strlen(op);
	int push = op_len > 5 && !strcmp(op + op_len - 5, "_push");
	struct msgb *msg = msgb_alloc_headroom(MSG_SIZE, push ? PUSH_HEADROOM : PUT_HEADROOM, "tlv");
	long ret;

	msgb_put_u8(msg, 0xc0);
	msgb_put_u8(msg, 0xc1);
	memset(msg->tail, FILL, msgb_tailroom(msg));
	ret = run_op(msg, op, tag, len, num);
	printf("{\"kind\":\"msgb\",\"op\":\"%s\",\"tag\":%u,\"len\":%u,\"num\":%u,\"ret\":%ld,\"headroom\":%d",
	       op, tag, len, num, ret, msgb_headroom(msg));
	output("out", msgb_data(msg), msgb_length(msg));
	printf("}\n");
	msgb_free(msg);
}

static void msgb_records(void)
{
	static const char *const len8_ops[] = { "lv", "tlv", "tlv_zeroed", "tlv16", "tv_fixed", "tlv_push" };
	static const char *const len16_ops[] = { "tl16v", "tvl", "tvlv", "tvlv_push" };
	static const char *const tag16_len8_ops[] = { "t16lv" };
	static const char *const tag16_len16_ops[] = { "vtvlv_gan", "vtvl_gan_push", "vtvlv_gan_push" };
	static const char *const tag_ops[] = { "tl" };
	static const char *const num_ops[] = { "tv", "tv16", "tv32", "tlv1_push", "tv_push", "tv16_push",
		"tv32_push", "tvlv_16be", "tvlv_32be" };
	static const uint8_t tags8[] = { 0x00, 0x7f, 0x80, 0xff };
	static const uint16_t tags16[] = { 0x0000, 0x7f, 0x80, 0xff, 0x100, 0x1234, 0x7fff, 0x8000, 0xffff };
	static const uint8_t lens8[] = { 0, 1, 2, 0x7f, 0x80, 0xff };
	static const uint16_t lens16[] = { 0, 1, 0x7f, 0x80, 0xff, 0x100, 0x7fff, 0x8000, 0xc000 };
	static const uint32_t nums[] = { 0, 1, 0xab, 0x1234, 0x89abcdef };
	size_t o, t, l;

#define FOR(ops, tags, lens) \
	for (o = 0; o < ARRAY_SIZE(ops); o++) \
		for (t = 0; t < ARRAY_SIZE(tags); t++) \
			for (l = 0; l < ARRAY_SIZE(lens); l++) \
				msgb_case(ops[o], tags[t], lens[l], 0)
	FOR(len8_ops, tags8, lens8);
	FOR(len16_ops, tags8, lens16);
	FOR(tag16_len8_ops, tags16, lens8);
	FOR(tag16_len16_ops, tags16, lens16);
#undef FOR
	for (l = 0; l < ARRAY_SIZE(lens16); l++)
		msgb_case("l16tv", 0x5a, lens16[l], 0);
	for (o = 0; o < ARRAY_SIZE(tag_ops); o++)
		for (t = 0; t < ARRAY_SIZE(tags8); t++)
			msgb_case(tag_ops[o], tags8[t], 0, 0);
	for (o = 0; o < ARRAY_SIZE(num_ops); o++)
		for (t = 0; t < ARRAY_SIZE(tags8); t++)
			for (l = 0; l < ARRAY_SIZE(nums); l++)
				msgb_case(num_ops[o], tags8[t], 0, nums[l]);
	for (l = 0; l < ARRAY_SIZE(nums); l++)
		msgb_case("v", 0, 0, nums[l] & 0xff);
}

static void gross_records(void)
{
	static const unsigned int lens[] = { 0, 1, 0x7f, 0x80, 0xff, 0x100, 0x7fff, 0x8000, 0xfffc, 0xfffd, 0xffff };
	static const unsigned int tags[] = { 0, 0x7f, 0x80, 0xffff };
	size_t l, t;

	for (l = 0; l < ARRAY_SIZE(lens); l++) {
		printf("{\"kind\":\"gross\",\"len\":%u,\"lv\":%u,\"tlv\":%u,\"tlv16\":%u,\"tl16v\":%u,"
		       "\"l16tv\":%u,\"t16lv\":%u,\"tvlv\":%u}\n",
		       lens[l], LV_GROSS_LEN(lens[l]), TLV_GROSS_LEN(lens[l]), TLV16_GROSS_LEN(lens[l]),
		       TL16V_GROSS_LEN(lens[l]), L16TV_GROSS_LEN(lens[l]), T16LV_GROSS_LEN(lens[l]),
		       TVLV_GROSS_LEN(lens[l]));
		for (t = 0; t < ARRAY_SIZE(tags); t++)
			printf("{\"kind\":\"gross_gan\",\"tag\":%u,\"len\":%u,\"vtvl\":%u,\"vtvlv\":%u}\n",
			       tags[t], lens[l], VTVL_GAN_GROSS_LEN(tags[t], lens[l]),
			       VTVLV_GAN_GROSS_LEN(tags[t], lens[l]));
	}
}

static void shift_case(const char *op, const uint8_t *input, size_t len, unsigned int arg)
{
	static uint8_t sentinel;
	uint8_t *data, *value = &sentinel, tag = FILL;
	size_t data_len = len, value_len = 0xa5a5;
	int rc;

	place(input, len);
	data = storage;
	if (!strcmp(op, "v_fixed"))
		rc = osmo_shift_v_fixed(&data, &data_len, arg, &value);
	else if (!strcmp(op, "match_tv_fixed"))
		rc = osmo_match_shift_tv_fixed(&data, &data_len, arg & 0xff, arg >> 8, &value);
	else if (!strcmp(op, "tlv"))
		rc = osmo_shift_tlv(&data, &data_len, &tag, &value, &value_len);
	else if (!strcmp(op, "match_tlv"))
		rc = osmo_match_shift_tlv(&data, &data_len, arg, &value, &value_len);
	else if (!strcmp(op, "lv"))
		rc = osmo_shift_lv(&data, &data_len, &value, &value_len);
	else
		abort();
	printf("{\"kind\":\"shift\",\"op\":\"%s\",\"arg\":%u", op, arg);
	hex("input", storage, len);
	printf(",\"rc\":%d,\"consumed\":%zu,\"remaining\":%zu,\"tag\":%u,\"val\":%ld,\"val_len\":%zu}\n", rc,
	       (size_t)(data - storage), data_len, tag, value == &sentinel ? UNTOUCHED : (long)(value - storage),
	       value_len);
}

static void shift_records(void)
{
	static const char *const ops[] = { "tlv", "lv" };
	uint8_t input[12];
	size_t i, o;
	unsigned int len, length, tag, arg;

	for (i = 0; i < sizeof(input); i++)
		input[i] = values[i];
	for (len = 0; len <= sizeof(input); len++) {
		for (arg = 0; arg <= 13; arg++) {
			shift_case("v_fixed", input, len, arg);
			for (tag = 0; tag < 2; tag++)
				shift_case("match_tv_fixed", input, len, (arg << 8) | (tag ? input[0] : (input[0] ^ 1)));
		}
		for (length = 0; length <= 11; length++) {
			input[0] = length;
			input[1] = length;
			for (o = 0; o < ARRAY_SIZE(ops); o++)
				shift_case(ops[o], input, len, 0);
			for (tag = 0; tag < 2; tag++)
				shift_case("match_tlv", input, len, tag ? input[0] : (input[0] ^ 1));
			input[0] = values[0];
			input[1] = values[1];
		}
	}
}

static void merge_copy_records(void)
{
	static const uint8_t first[] = { 0x04, 0x02, 0xc1, 0xc2, 0x05, 0x00, 0x01, 0xd1, 0x01, 0x02, 0xaa };
	static const uint8_t second[] = { 0x04, 0x01, 0xe1, 0x20, 0x00, 0x01, 0xff, 0x01, 0xf1 };
	struct tlv_parsed *dst, *copy, src, base;
	uint8_t *second_buf = talloc_memdup(ctx, second, sizeof(second));
	const uint8_t *buf = place(first, sizeof(first));
	int tag, rc;

	tlv_parse(&base, &mixed_def, buf, sizeof(first), 0, 0);
	tlv_parse(&src, &mixed_def, second_buf, sizeof(second), 0, 0);

	dst = talloc_zero(ctx, struct tlv_parsed);
	for (tag = 0; tag < 256; tag++) {
		if (!base.lv[tag].val)
			continue;
		dst->lv[tag].len = base.lv[tag].len;
		dst->lv[tag].val = talloc_memdup(dst, base.lv[tag].val, base.lv[tag].len + 1);
	}
	rc = osmo_tlvp_merge(dst, &src);
	printf("{\"kind\":\"merge\",\"rc\":%d", rc);
	hex("dst_input", first, sizeof(first));
	hex("src_input", second, sizeof(second));
	/* Each present tag, its length and its bytes. The allocation holds src's length. */
	printf(",\"result\":\"");
	{
		int printed = 0;

		for (tag = 0; tag < 256; tag++) {
			unsigned int b, bytes;

			if (!dst->lv[tag].val)
				continue;
			bytes = src.lv[tag].val && src.lv[tag].len ? src.lv[tag].len : dst->lv[tag].len;
			printf("%s%d %u ", printed ? " " : "", tag, dst->lv[tag].len);
			if (!bytes)
				printf("-");
			for (b = 0; b < bytes; b++)
				printf("%02x", dst->lv[tag].val[b]);
			printed = 1;
		}
	}
	printf("\"}\n");
	talloc_free(dst);

	copy = osmo_tlvp_copy(&base, ctx);
	printf("{\"kind\":\"copy\"");
	hex("input", first, sizeof(first));
	printf(",\"lens\":\"");
	{
		int printed = 0;

		for (tag = 0; tag < 256; tag++) {
			if (!copy->lv[tag].len)
				continue;
			printf("%s%d %u", printed ? " " : "", tag, copy->lv[tag].len);
			printed = 1;
		}
	}
	printf("\",\"present\":\"");
	{
		int printed = 0;

		for (tag = 0; tag < 256; tag++) {
			if (!TLVP_PRESENT(copy, tag))
				continue;
			printf("%s%d", printed ? " " : "", tag);
			printed = 1;
		}
	}
	printf("\"}\n");
	talloc_free(copy);
	talloc_free(second_buf);
}

static void patch_records(void)
{
	struct tlv_definition dst = { 0 }, src = { 0 };

	dst.def[1].type = TLV_TYPE_T;
	dst.def[2].type = TLV_TYPE_FIXED;
	dst.def[2].fixed_len = 7;
	src.def[2].type = TLV_TYPE_FIXED;
	src.def[2].fixed_len = 9;
	src.def[3].type = TLV_TYPE_FIXED;
	src.def[3].fixed_len = 5;
	src.def[0xff].type = TLV_TYPE_TLV;
	printf("{\"kind\":\"patch\"");
	def_entries("dst", &dst);
	def_entries("src", &src);
	tlv_def_patch(&dst, &src);
	def_entries("result", &dst);
	printf("}\n");
}

static void access_records(void)
{
	static const uint8_t input[] = {
		0x01, 0x02, 0xaa, 0x04, 0x00, 0x20, 0x01, 0x99, 0x05, 0x00, 0x02, 0x12, 0x34,
		0x81, 0x00, 0x04, 0x89, 0xab, 0xcd, 0xef, 0xff, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05,
	};
	static const uint8_t tags[] = { 0x01, 0x02, 0x04, 0x20, 0x05, 0x81, 0xff, 0x33 };
	const uint8_t *buf = place(input, sizeof(input));
	struct tlv_parsed tp;
	size_t t;
	unsigned int min;

	tlv_parse(&tp, &mixed_def, buf, sizeof(input), 0, 0);
	for (t = 0; t < ARRAY_SIZE(tags); t++) {
		uint8_t tag = tags[t];
		int len = TLVP_PRESENT(&tp, tag) ? TLVP_LEN(&tp, tag) : -1;

		printf("{\"kind\":\"access\",\"tag\":%u,\"present\":%d,\"len\":%d,\"val8\":%u,\"val16be\":%d,"
		       "\"val32be\":%" PRId64 ",\"val16_unal\":%d,\"val32_unal\":%" PRId64 ",\"min_len\":\"",
		       tag, TLVP_PRESENT(&tp, tag), len, tlvp_val8(&tp, tag, 0x5a),
		       len >= 2 ? tlvp_val16be(&tp, tag) : -1,
		       len >= 4 ? (int64_t)tlvp_val32be(&tp, tag) : -1,
		       len >= 2 ? tlvp_val16_unal(&tp, tag) : -1,
		       len >= 4 ? (int64_t)tlvp_val32_unal(&tp, tag) : -1);
		for (min = 0; min <= 6; min++) {
			const struct tlv_p_entry *entry = TLVP_GET_MINLEN(&tp, tag, min);
			const uint8_t *value = TLVP_VAL_MINLEN(&tp, tag, min);
			int present = TLVP_PRES_LEN(&tp, tag, min);

			if (present != (entry != NULL) || present != (value != NULL))
				abort();
			printf("%s%d", min ? " " : "", present);
		}
		printf("\"}\n");
	}
}

static const uint8_t attach_ies[] = { 0x04, 0x05 };
static const uint8_t detach_ies[] = { 0x04 };
static const struct value_string msg_names[] = {
	{ 1, "One" },
	{ 7, "Seven" },
	{ 0, NULL },
};

static void prot_case(const struct osmo_tlv_prot_def *pdef, const char *name, uint8_t msg_type,
		      const uint8_t *input, unsigned int len, int multiples)
{
	struct tlv_parsed dec[2];
	const uint8_t *buf = place(input, len);
	int parse_rc, validate_rc;

	parse_rc = osmo_tlv_prot_parse(pdef, dec, multiples, msg_type, buf, len, 0, 0, DLGLOBAL, "t");
	validate_rc = parse_rc < 0 ? UNTOUCHED : osmo_tlv_prot_validate_tp(pdef, msg_type, dec, DLGLOBAL, "t");
	printf("{\"kind\":\"prot\",\"prot\":\"%s\",\"msg_type\":%u", name, msg_type);
	hex("input", buf, len);
	printf(",\"multiples\":%d,\"parse\":%d,\"validate\":%d}\n", multiples, parse_rc, validate_rc);
}

static void prot_records(void)
{
	static struct osmo_tlv_prot_def plain, named;
	static const struct { const char *hex_name; uint8_t len; uint8_t data[16]; } inputs[] = {
		{ "both", 9, { 0x04, 0x02, 0xc1, 0xc2, 0x05, 0x00, 0x02, 0xd1, 0xd2 } },
		{ "both_exact", 10, { 0x04, 0x02, 0xc1, 0xc2, 0x05, 0x00, 0x03, 0xd1, 0xd2, 0xd3 } },
		{ "only_4", 4, { 0x04, 0x02, 0xc1, 0xc2 } },
		{ "only_5", 6, { 0x05, 0x00, 0x03, 0xd1, 0xd2, 0xd3 } },
		{ "short_4", 3, { 0x04, 0x01, 0xc1 } },
		{ "empty_4", 2, { 0x04, 0x00 } },
		{ "none", 0, { 0 } },
		{ "truncated", 2, { 0x04, 0x02 } },
		{ "unknown", 1, { 0x33 } },
		{ "tail_ie_short", 8, { 0x20, 0x00, 0x04, 0x01, 0xc1, 0x05, 0x00, 0x00 } },
	};
	static const uint8_t types[] = { 1, 2, 3, 7, 8 };
	static const uint8_t name_types[] = { 0, 1, 2, 3, 7, 8, 0xff };
	static const uint8_t ies[] = { 0x04, 0x05, 0x06, 0x00, 0xff };
	size_t i, t, n;

	plain.name = "plain";
	plain.tlv_def = &mixed_def;
	plain.msg_def[1] = (struct osmo_tlv_prot_msg_def)MSG_DEF("Attach", attach_ies, 0x11);
	plain.msg_def[2] = (struct osmo_tlv_prot_msg_def)MSG_DEF(NULL, detach_ies, 0x02);
	plain.msg_def[3].flags = 0x04;
	plain.ie_def[0x04] = (struct osmo_tlv_prot_ie_def){ 2, "Four" };
	plain.ie_def[0x05] = (struct osmo_tlv_prot_ie_def){ 3, NULL };
	named = plain;
	named.name = "named";
	named.msgt_names = msg_names;

	for (i = 0; i < ARRAY_SIZE(inputs); i++)
		for (t = 0; t < ARRAY_SIZE(types); t++)
			for (n = 1; n <= 2; n++)
				prot_case(&plain, "plain", types[t], inputs[i].data, inputs[i].len, n);
	for (t = 0; t < ARRAY_SIZE(name_types); t++)
		printf("{\"kind\":\"prot_name\",\"msg_type\":%u,\"plain\":\"%s\",\"named\":\"%s\",\"flags\":%u}\n",
		       name_types[t], osmo_tlv_prot_msg_name(&plain, name_types[t]),
		       osmo_tlv_prot_msg_name(&named, name_types[t]),
		       osmo_tlv_prot_msgt_flags(&plain, name_types[t]));
	for (t = 0; t < ARRAY_SIZE(ies); t++)
		printf("{\"kind\":\"prot_ie\",\"iei\":%u,\"name\":\"%s\"}\n", ies[t],
		       osmo_tlv_prot_ie_name(&plain, ies[t]));
}

int main(void)
{
	static const struct log_info_cat categories[] = {
		[0] = { .name = "DTEST", .enabled = 0, .loglevel = LOGL_FATAL },
	};
	static const struct log_info info = { .cat = categories, .num_cat = ARRAY_SIZE(categories) };

	ctx = talloc_named_const(NULL, 0, "tlv");
	log_init(&info, ctx);
	init_defs();

	printf("{\"kind\":\"meta\",\"schema\":\"tlv-v1\",\"commit\":\"" COMMIT "\"}\n");
	def_record("mixed", &mixed_def);
	def_record("empty", &empty_def);
	def_record("tvlv_att", &tvlv_att_def);
	def_record("vtvlv_gan_att", &vtvlv_gan_att_def);
	parse_one_records();
	parse_records();
	encode_one_records();
	encode_records();
	msgb_records();
	gross_records();
	shift_records();
	merge_copy_records();
	patch_records();
	access_records();
	prot_records();
	return 0;
}
