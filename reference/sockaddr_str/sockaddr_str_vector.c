/*
 * Reference harness for libosmocore's IP address and port strings.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one section per
 * kind of record. The first argument is upstream's sockaddr_str_test.ok. Upstream's test is
 * replayed with the same calls, and the run fails unless its output matches that file.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include <osmocom/core/sockaddr_str.h>
#include <osmocom/core/utils.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
/* Bytes of a sockaddr_storage recorded; the harness checks the rest stay zero. */
#define STORAGE_RECORDED 28
#define JUNK_AF 99
#define JUNK_PORT 4242

enum section { TYPE, CASE, FROM, CMP, SECTION_COUNT };

static FILE *sections[SECTION_COUNT];
static char *section_text[SECTION_COUNT];
static size_t section_size[SECTION_COUNT];
/* Upstream's printed output, compared with sockaddr_str_test.ok at the end. */
static FILE *upstream_out;
static char *upstream_text;
static size_t upstream_size;
static const char *source = "sweep";

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
	fprintf(sections[section], "{\"kind\":\"%s\",\"source\":\"%s\",", kind, source);
}

/* The ip field past its NUL must stay zero for the recorded text to describe it fully. */
static void check_ip_tail(const struct osmo_sockaddr_str *s)
{
	size_t length = strnlen(s->ip, sizeof(s->ip));

	CHECK(length < sizeof(s->ip));
	for (size_t i = length; i < sizeof(s->ip); i++)
		CHECK(s->ip[i] == 0);
}

static void struct_fields(FILE *f, const char *prefix, const struct osmo_sockaddr_str *s)
{
	char key[16];

	check_ip_tail(s);
	fprintf(f, "\"%saf\":%d,", prefix, s->af);
	snprintf(key, sizeof(key), "%sip", prefix);
	hex_text(f, key, s->ip);
	fprintf(f, ",\"%sport\":%u", prefix, s->port);
}

static struct osmo_sockaddr_str make(int af, const char *ip, uint16_t port)
{
	struct osmo_sockaddr_str s = { .af = af, .port = port };

	CHECK(strlen(ip) < sizeof(s.ip));
	strcpy(s.ip, ip);
	return s;
}

static struct osmo_sockaddr_str junk(void)
{
	return make(JUNK_AF, "junk", JUNK_PORT);
}

/* ---- Recorders ---- */

static int record_type(const char *ip)
{
	int af = osmo_ip_str_type(ip);
	FILE *f = sections[TYPE];

	start(TYPE, "type");
	hex_text(f, "ip", ip);
	fprintf(f, ",\"af\":%d}\n", af);
	return af;
}

/* Finish a from record: the destination after the call. */
static void from_result(FILE *f, int rc, const struct osmo_sockaddr_str *out)
{
	fprintf(f, ",\"rc\":%d,", rc);
	struct_fields(f, "out_", out);
	fprintf(f, "}\n");
}

static void from_start(const char *op, const void *input, size_t length)
{
	FILE *f = sections[FROM];

	start(FROM, "from");
	fprintf(f, "\"op\":\"%s\",", op);
	hex_bytes(f, "input", input, length);
}

static int from_in_addr(struct osmo_sockaddr_str *out, const struct in_addr *a, uint16_t port)
{
	int rc;

	*out = junk();
	rc = osmo_sockaddr_str_from_in_addr(out, a, port);
	from_start("in_addr", a, sizeof(*a));
	fprintf(sections[FROM], ",\"port\":%u", port);
	from_result(sections[FROM], rc, out);
	return rc;
}

static int from_in6_addr(struct osmo_sockaddr_str *out, const struct in6_addr *a, uint16_t port)
{
	int rc;

	*out = junk();
	rc = osmo_sockaddr_str_from_in6_addr(out, a, port);
	from_start("in6_addr", a, sizeof(*a));
	fprintf(sections[FROM], ",\"port\":%u", port);
	from_result(sections[FROM], rc, out);
	return rc;
}

/* The input is the address's bytes in memory, which are in network order. */
static int from_32(struct osmo_sockaddr_str *out, uint32_t ip, uint16_t port)
{
	int rc;

	*out = junk();
	rc = osmo_sockaddr_str_from_32(out, ip, port);
	from_start("32", &ip, sizeof(ip));
	fprintf(sections[FROM], ",\"port\":%u", port);
	from_result(sections[FROM], rc, out);
	return rc;
}

/* The input is the value, most significant byte first. */
static int from_32h(struct osmo_sockaddr_str *out, uint32_t ip, uint16_t port)
{
	const uint8_t value[4] = { ip >> 24, ip >> 16, ip >> 8, ip };
	int rc;

	*out = junk();
	rc = osmo_sockaddr_str_from_32h(out, ip, port);
	from_start("32h", value, sizeof(value));
	fprintf(sections[FROM], ",\"port\":%u", port);
	from_result(sections[FROM], rc, out);
	return rc;
}

static int from_sockaddr_in(struct osmo_sockaddr_str *out, const struct sockaddr_in *a)
{
	int rc;

	*out = junk();
	rc = osmo_sockaddr_str_from_sockaddr_in(out, a);
	from_start("sockaddr_in", a, sizeof(*a));
	fprintf(sections[FROM], ",\"port\":null");
	from_result(sections[FROM], rc, out);
	return rc;
}

static int from_sockaddr_in6(struct osmo_sockaddr_str *out, const struct sockaddr_in6 *a)
{
	int rc;

	*out = junk();
	rc = osmo_sockaddr_str_from_sockaddr_in6(out, a);
	from_start("sockaddr_in6", a, sizeof(*a));
	fprintf(sections[FROM], ",\"port\":null");
	from_result(sections[FROM], rc, out);
	return rc;
}

static int from_sockaddr(struct osmo_sockaddr_str *out, const struct sockaddr_storage *a)
{
	int rc;

	for (size_t i = STORAGE_RECORDED; i < sizeof(*a); i++)
		CHECK(((const unsigned char *)a)[i] == 0);
	*out = junk();
	rc = osmo_sockaddr_str_from_sockaddr(out, a);
	from_start("sockaddr", a, STORAGE_RECORDED);
	fprintf(sections[FROM], ",\"port\":null");
	from_result(sections[FROM], rc, out);
	return rc;
}

/* A NULL ip is recorded as null. */
static int from_str(struct osmo_sockaddr_str *out, const char *ip, uint16_t port)
{
	FILE *f = sections[FROM];
	int rc;

	*out = junk();
	rc = osmo_sockaddr_str_from_str(out, ip, port);
	start(FROM, "from");
	fprintf(f, "\"op\":\"str\",");
	hex_text(f, "input", ip);
	fprintf(f, ",\"port\":%u", port);
	from_result(f, rc, out);
	return rc;
}

static int from_str2(struct osmo_sockaddr_str *out, const char *ip)
{
	FILE *f = sections[FROM];
	int rc;

	*out = junk();
	rc = osmo_sockaddr_str_from_str2(out, ip);
	start(FROM, "from");
	fprintf(f, "\"op\":\"str2\",");
	hex_text(f, "input", ip);
	fprintf(f, ",\"port\":null");
	from_result(f, rc, out);
	return rc;
}

/* Every conversion from one stored address, with what each wrote. */
struct case_result {
	bool set, nonzero;
	int in_rc, in6_rc, u32_rc, u32h_rc, sin_rc, sin6_rc, ss_rc;
	struct in_addr in;
	struct in6_addr in6;
	uint32_t u32, u32h;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_storage ss;
};

static struct case_result record_case(const struct osmo_sockaddr_str *x)
{
	struct case_result r = {};
	FILE *f = sections[CASE];
	char fmt[128];

	snprintf(fmt, sizeof(fmt), OSMO_SOCKADDR_STR_FMT, OSMO_SOCKADDR_STR_FMT_ARGS(x));
	r.set = osmo_sockaddr_str_is_set(x);
	r.nonzero = osmo_sockaddr_str_is_nonzero(x);
	r.in_rc = osmo_sockaddr_str_to_in_addr(x, &r.in);
	r.in6_rc = osmo_sockaddr_str_to_in6_addr(x, &r.in6);
	r.u32_rc = osmo_sockaddr_str_to_32(x, &r.u32);
	r.u32h_rc = osmo_sockaddr_str_to_32h(x, &r.u32h);
	r.sin_rc = osmo_sockaddr_str_to_sockaddr_in(x, &r.sin);
	r.sin6_rc = osmo_sockaddr_str_to_sockaddr_in6(x, &r.sin6);
	r.ss_rc = osmo_sockaddr_str_to_sockaddr(x, &r.ss);
	for (size_t i = STORAGE_RECORDED; i < sizeof(r.ss); i++)
		CHECK(((const unsigned char *)&r.ss)[i] == 0);

	start(CASE, "case");
	struct_fields(f, "", x);
	fprintf(f, ",");
	hex_text(f, "fmt", fmt);
	fprintf(f, ",\"set\":%d,\"nonzero\":%d", r.set, r.nonzero);
	fprintf(f, ",\"in_rc\":%d,", r.in_rc);
	hex_bytes(f, "in", &r.in, sizeof(r.in));
	fprintf(f, ",\"in6_rc\":%d,", r.in6_rc);
	hex_bytes(f, "in6", &r.in6, sizeof(r.in6));
	fprintf(f, ",\"u32_rc\":%d,", r.u32_rc);
	hex_bytes(f, "u32", &r.u32, sizeof(r.u32));
	fprintf(f, ",\"u32h_rc\":%d,\"u32h\":%u", r.u32h_rc, r.u32h);
	fprintf(f, ",\"sin_rc\":%d,", r.sin_rc);
	hex_bytes(f, "sin", &r.sin, sizeof(r.sin));
	fprintf(f, ",\"sin6_rc\":%d,", r.sin6_rc);
	hex_bytes(f, "sin6", &r.sin6, sizeof(r.sin6));
	fprintf(f, ",\"ss_rc\":%d,", r.ss_rc);
	hex_bytes(f, "ss", &r.ss, STORAGE_RECORDED);
	fprintf(f, "}\n");
	return r;
}

static int sign(int value)
{
	return (value > 0) - (value < 0);
}

/* C's result is a strcmp() or memcmp() difference, so only its sign is recorded. */
static int record_cmp(const struct osmo_sockaddr_str *a, const struct osmo_sockaddr_str *b)
{
	FILE *f = sections[CMP];
	int rc = sign(osmo_sockaddr_str_cmp(a, b));

	start(CMP, "cmp");
	struct_fields(f, "a_", a);
	fprintf(f, ",");
	struct_fields(f, "b_", b);
	fprintf(f, ",\"sign\":%d}\n", rc);
	return rc;
}

/* ---- Upstream replay ---- */

/* upstream's oip_data[] */
static const struct {
	int af;
	const char *ip;
	uint16_t port;
} upstream_data[] = {
	{ AF_INET, "1.2.3.4", 5 },
	{ AF_INET, "0.0.0.0", 0 },
	{ AF_INET, "255.255.255.255", 65535 },
	{ AF_INET, "0.0.0.256", 1 },
	{ AF_INET, "not an ip address", 1 },
	{ AF_INET6, "1:2:3::4", 5 },
	{ AF_INET6, "::", 0 },
	{ AF_INET6, "::1", 0 },
	{ AF_INET6, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", 65535 },
	{ AF_INET6, "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF", 65535 },
	{ AF_INET6, "::fffff", 1 },
	{ AF_INET6, "not an ip address", 1 },
	{ AF_INET6, "1.2.3.4", 5 },
	{ AF_INET, "1:2:3::4", 5 },
	{ AF_UNSPEC, "1.2.3.4", 5 },
	{ AF_INET, "", 5 },
	{ AF_INET6, "", 5 },
	{ AF_INET, "1.2.3.4", 0 },
	{ AF_INET, "1.2.3:4:5", 0 },
	{ AF_INET6, "::1:10.9.8.7", 1 },
	{ AF_INET, "0.0.0.0", 5 },
	{ AF_INET6, "::", 5 },
	{ AF_INET6, "0::", 5 },
};

static const char *af_name(int af)
{
	switch (af) {
	case AF_INET:
		return "AF_INET";
	case AF_INET6:
		return "AF_INET6";
	case AF_UNSPEC:
		return "AF_UNSPEC";
	default:
		return "?";
	}
}

static const char *rc_name(int rc)
{
	switch (rc) {
	case -EINVAL:
		return "rc == -EINVAL";
	case -EAFNOSUPPORT:
		return "rc == -EAFNOSUPPORT";
	case 0:
		return "rc == 0";
	default:
		return rc < 0 ? "rc < 0" : "rc > 0";
	}
}

static void dump_oip(const struct osmo_sockaddr_str *oip)
{
	fprintf(upstream_out, "{ .af = %s, .ip = %s, .port = %u }\n", af_name(oip->af),
		osmo_quote_str(oip->ip, -1), oip->port);
}

static const char *hexdump(const void *data, size_t length)
{
	static char buf[1024];

	return osmo_hexdump_buf(buf, sizeof(buf), data, length, "", false);
}

static void back(const char *name, int rc, const struct osmo_sockaddr_str *x,
		 const struct osmo_sockaddr_str *out)
{
	fprintf(upstream_out, "   -> osmo_sockaddr_str_from_%s() %s ", name, rc_name(rc));
	dump_oip(out);
	if (memcmp(x, out, sizeof(*out)))
		fprintf(upstream_out, "      DIFFERS!\n");
}

/* upstream's sockaddr_str_test_conversions() */
static void upstream_conversions(const struct osmo_sockaddr_str *x)
{
	FILE *o = upstream_out;
	struct case_result r = record_case(x);
	struct osmo_sockaddr_str out;
	int rc;

	fprintf(o, "\n\n");
	dump_oip(x);
	fprintf(o, "  OSMO_SOCKADDR_STR_FMT: '" OSMO_SOCKADDR_STR_FMT "'\n", OSMO_SOCKADDR_STR_FMT_ARGS(x));
	fprintf(o, "  osmo_sockaddr_str_is_set() = %s\n", r.set ? "true" : "false");
	fprintf(o, "  osmo_sockaddr_str_is_nonzero() = %s\n", r.nonzero ? "true" : "false");

	fprintf(o, "  osmo_sockaddr_str_to_in_addr() %s in_addr=%s\n", rc_name(r.in_rc),
		hexdump(&r.in, sizeof(r.in)));
	if (!r.in_rc)
		back("in_addr", from_in_addr(&out, &r.in, x->port), x, &out);

	fprintf(o, "  osmo_sockaddr_str_to_in6_addr() %s in6_addr=%s\n", rc_name(r.in6_rc),
		hexdump(&r.in6, sizeof(r.in6)));
	if (!r.in6_rc)
		back("in6_addr", from_in6_addr(&out, &r.in6, x->port), x, &out);

	fprintf(o, "  osmo_sockaddr_str_to_32() %s uint8_t[4]=[ %s]\n", rc_name(r.u32_rc),
		osmo_hexdump((void *)&r.u32, sizeof(r.u32)));
	if (!r.u32_rc)
		back("32", from_32(&out, r.u32, x->port), x, &out);

	fprintf(o, "  osmo_sockaddr_str_to_32h() %s uint8_t[4]=[ %s]\n", rc_name(r.u32h_rc),
		osmo_hexdump((void *)&r.u32h, sizeof(r.u32h)));
	if (!r.u32h_rc)
		back("32h", from_32h(&out, r.u32h, x->port), x, &out);

	fprintf(o, "  osmo_sockaddr_str_to_sockaddr_in() %s sockaddr_in=%s\n", rc_name(r.sin_rc),
		hexdump(&r.sin, sizeof(r.sin)));
	if (!r.sin_rc)
		back("sockaddr_in", from_sockaddr_in(&out, &r.sin), x, &out);

	fprintf(o, "  osmo_sockaddr_str_to_sockaddr_in6() %s sockaddr_in6=%s\n", rc_name(r.sin6_rc),
		hexdump(&r.sin6, sizeof(r.sin6)));
	if (!r.sin6_rc)
		back("sockaddr_in6", from_sockaddr_in6(&out, &r.sin6), x, &out);

	fprintf(o, "  osmo_sockaddr_str_to_sockaddr() %s sockaddr_storage=%s\n", rc_name(r.ss_rc),
		hexdump(&r.ss, sizeof(r.ss)));
	if (!r.ss_rc)
		back("sockaddr", from_sockaddr(&out, &r.ss), x, &out);

	rc = from_str(&out, x->ip, x->port);
	fprintf(o, "  osmo_sockaddr_str_from_str() %s ", rc_name(rc));
	dump_oip(&out);
	if (rc == 0 && memcmp(x, &out, sizeof(out)))
		fprintf(o, "      DIFFERS!\n");
}

static void upstream_replay(const char *ok_path)
{
	struct osmo_sockaddr_str data[ARRAY_SIZE(upstream_data)];
	char *expected = NULL;
	size_t expected_size = 0;
	FILE *file;

	source = "upstream";
	upstream_out = open_memstream(&upstream_text, &upstream_size);
	CHECK(upstream_out);
	for (size_t i = 0; i < ARRAY_SIZE(data); i++)
		data[i] = make(upstream_data[i].af, upstream_data[i].ip, upstream_data[i].port);

	for (size_t i = 0; i < ARRAY_SIZE(data); i++)
		upstream_conversions(&data[i]);

	/* upstream's test_osmo_sockaddr_str_cmp() */
	fprintf(upstream_out, "\n\n%s\n", "test_osmo_sockaddr_str_cmp");
	for (size_t i = 0; i < ARRAY_SIZE(data); i++) {
		struct osmo_sockaddr_str a = data[i];

		fprintf(upstream_out, "[%2zu]\n", i);
		for (size_t j = 0; j < ARRAY_SIZE(data); j++) {
			const struct osmo_sockaddr_str *b = &data[j];
			int rc = record_cmp(&a, b);

			fprintf(upstream_out,
				"  osmo_sockaddr_str_cmp(): " OSMO_SOCKADDR_STR_FMT "%s %s "
				OSMO_SOCKADDR_STR_FMT "%s\n",
				OSMO_SOCKADDR_STR_FMT_ARGS(&a),
				osmo_sockaddr_str_is_nonzero(&a) ? "" : "(zero)",
				rc < 0 ? "<" : (rc == 0 ? "==" : ">"),
				OSMO_SOCKADDR_STR_FMT_ARGS(b),
				osmo_sockaddr_str_is_nonzero(b) ? "" : "(zero)");
		}
	}
	fclose(upstream_out);

	file = fopen(ok_path, "r");
	if (!file) {
		perror(ok_path);
		exit(EXIT_FAILURE);
	}
	expected = malloc(1 << 20);
	CHECK(expected);
	expected_size = fread(expected, 1, 1 << 20, file);
	fclose(file);
	if (expected_size != upstream_size || memcmp(expected, upstream_text, upstream_size))
		fail("upstream output");
	free(expected);
	free(upstream_text);
	source = "sweep";
}

/* ---- Sweeps ---- */

/* Text for classifying and parsing: valid, nearly valid and invalid forms of both families. */
static const char *const texts[] = {
	"", "1", ".", ":", "1.2.3.4", "0.0.0.0", "255.255.255.255", "127.1", "1.2.3",
	"1.2.3.4.5", "256.0.0.0", "1.2.3.256", "999.1.1.1", "01.2.3.4", "1.2.3.04", "0.0.0.00",
	"00.0.0.0", "1..2.3", ".1.2.3", "1.2.3.", "1.2.3.4 ", " 1.2.3.4", "0x1.2.3.4",
	"1.2.3.-4", "+1.2.3.4", "1.2.3.4/24", "1.2.3.2555", "1.2.3.0255", "4294967295",
	"1.2.3.4a", "not an ip address",
	"::", "::1", "1::", "0::", "::0", "1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7::", "::2:3:4:5:6:7:8",
	"1::3:4:5:6:7:8", "1::2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8:9", "1:2:3:4:5:6:7",
	"::ffff:1.2.3.4", "::1.2.3.4", "1:2:3:4:5:6:1.2.3.4", "1:2:3:4:5:6:7:1.2.3.4",
	"1:2:3:4:5::1.2.3.4", "::ffff:1.2.3", "::ffff:01.2.3.4", "::ffff:256.1.1.1",
	"::1.2.3.4:5", "1.2.3.4::", "1:2:3::4:5::6", ":1:2::", "1:2::3:", ":::", "1:::2",
	"::fffff", "fffff::", "0000:0000::", "00000::", "0:0:0:0:0:0:0:0", "FfFf::aBcD",
	"g::", "1::%eth0", "fe80::1%1", "[::1]", "2001:db8::", "2001:db8:0:0:1:0:0:1",
	"::ffff:0:1.2.3.4", "64:ff9b::1.2.3.4", "1:2:3:4:5:6:7:8 ", "::1:10.9.8.7",
	"1.2.3:4:5", "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255",
	"\xff", "\x7f", "1.2.3.\xc3\xa9",
};

static void sweep_texts(void)
{
	static const int afs[] = { AF_INET, AF_INET6, AF_UNSPEC, JUNK_AF };
	static const uint16_t ports[] = { 0, 4660 };
	struct osmo_sockaddr_str out;

	record_type(NULL);
	for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
		record_type(texts[i]);
		for (size_t a = 0; a < ARRAY_SIZE(afs); a++)
			for (size_t p = 0; p < ARRAY_SIZE(ports); p++) {
				struct osmo_sockaddr_str x = make(afs[a], texts[i], ports[p]);

				record_case(&x);
			}
		from_str(&out, texts[i], 4660);
		from_str2(&out, texts[i]);
	}
}

/* Text around the 46-byte field, and NULL. */
static void sweep_lengths(void)
{
	static const size_t lengths[] = { 44, 45, 46, 47, 100 };
	struct osmo_sockaddr_str out;
	char text[101];

	from_str(&out, NULL, 7);
	from_str2(&out, NULL);
	for (size_t i = 0; i < ARRAY_SIZE(lengths); i++) {
		memset(text, '0', lengths[i]);
		text[1] = ':';
		text[lengths[i]] = 0;
		from_str(&out, text, 7);
		from_str2(&out, text);
		text[1] = '.';
		from_str(&out, text, 7);
	}
}

static void sweep_in_addr(void)
{
	static const uint8_t fixed[][4] = {
		{ 0, 0, 0, 0 }, { 255, 255, 255, 255 }, { 1, 2, 3, 4 }, { 127, 0, 0, 1 },
		{ 10, 0, 0, 255 }, { 0, 0, 0, 1 }, { 1, 0, 0, 0 }, { 192, 168, 100, 200 },
	};
	static const uint16_t ports[] = { 0, 1, 80, 4660, 65535 };
	uint32_t state = 0x5ad4;
	struct osmo_sockaddr_str out;

	for (size_t i = 0; i < ARRAY_SIZE(fixed) + 24; i++) {
		struct in_addr a;
		uint32_t value;
		uint16_t port = ports[i % ARRAY_SIZE(ports)];

		if (i < ARRAY_SIZE(fixed))
			memcpy(&a, fixed[i], 4);
		else
			a.s_addr = next_random(&state);
		memcpy(&value, &a, 4);
		from_in_addr(&out, &a, port);
		from_32(&out, value, port);
		from_32h(&out, value, port);
	}
}

static void put_words(struct in6_addr *a, const uint16_t words[8])
{
	for (size_t i = 0; i < 8; i++) {
		a->s6_addr[2 * i] = words[i] >> 8;
		a->s6_addr[2 * i + 1] = words[i];
	}
}

/* Where inet_ntop() shortens runs of zero groups and writes an IPv4 tail. */
static void sweep_in6_addr(void)
{
	static const uint16_t fixed[][8] = {
		{ 0, 0, 0, 0, 0, 0, 0, 1 }, { 0, 0, 0, 0, 0, 0, 1, 0 },
		{ 0, 0, 0, 0, 0, 0, 0x0102, 0x0304 }, { 0, 0, 0, 0, 0, 0xffff, 0x0102, 0x0304 },
		{ 0, 0, 0, 0, 0, 0xffff, 0, 0 }, { 0, 0, 0, 0, 0, 0xfffe, 0x0102, 0x0304 },
		{ 0, 0, 0, 0, 0xffff, 0, 0x0102, 0x0304 }, { 0x64, 0xff9b, 0, 0, 0, 0, 0x0102, 0x0304 },
		{ 0, 0, 0, 0, 0, 0, 0xffff, 0xffff }, { 0, 0, 0, 0, 0, 0, 0, 0x0102 },
		{ 0, 0, 0, 0, 0, 0, 0x0100, 0 }, { 1, 0, 0, 0, 0, 0xffff, 0x0102, 0x0304 },
		{ 0, 0, 0, 0, 0, 1, 0x0102, 0x0304 }, { 0x2001, 0xdb8, 0, 0, 1, 0, 0, 1 },
		{ 0x2001, 0, 0, 1, 0, 0, 0, 1 }, { 0x0abc, 0x00de, 0x000f, 0x1000, 0, 0, 0xff00, 0xa },
		{ 0xfe80, 0, 0, 0, 0, 0, 0, 1 }, { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff },
	};
	uint32_t state = 0x6c3e;
	struct osmo_sockaddr_str out;
	struct in6_addr a;

	/* Each group zero or not: every position and length of zero runs. */
	for (unsigned int mask = 0; mask < 256; mask++) {
		uint16_t words[8];

		for (size_t i = 0; i < 8; i++)
			words[i] = (mask >> (7 - i)) & 1 ? (uint16_t)(0x1111 * (i + 1)) : 0;
		put_words(&a, words);
		from_in6_addr(&out, &a, mask);
	}
	for (size_t i = 0; i < ARRAY_SIZE(fixed); i++) {
		put_words(&a, fixed[i]);
		from_in6_addr(&out, &a, 1);
	}
	for (size_t i = 0; i < 64; i++) {
		for (size_t j = 0; j < 16; j++) {
			uint32_t r = next_random(&state);

			/* Mostly zero bytes, so runs of zero groups are common. */
			a.s6_addr[j] = r % 3 ? 0 : (uint8_t)(r >> 8);
		}
		from_in6_addr(&out, &a, (uint16_t)next_random(&state));
	}
}

static void sweep_sockaddrs(void)
{
	static const int families[] = { AF_INET, AF_INET6, AF_UNSPEC, AF_UNIX, JUNK_AF };
	struct osmo_sockaddr_str out;

	for (size_t i = 0; i < ARRAY_SIZE(families); i++) {
		struct sockaddr_in sin = {
			.sin_family = families[i],
			.sin_port = htons(0x1234),
			.sin_addr.s_addr = htonl(0x0a000001),
			.sin_zero = { 1, 2, 3, 4, 5, 6, 7, 8 },
		};
		struct sockaddr_in6 sin6 = {
			.sin6_family = families[i],
			.sin6_port = htons(0xabcd),
			.sin6_flowinfo = 0x11223344,
			.sin6_scope_id = 0x55667788,
		};
		struct sockaddr_storage ss = {};

		sin6.sin6_addr.s6_addr[0] = 0x20;
		sin6.sin6_addr.s6_addr[1] = 0x01;
		sin6.sin6_addr.s6_addr[15] = 0x09;
		from_sockaddr_in(&out, &sin);
		from_sockaddr_in6(&out, &sin6);
		memcpy(&ss, &sin, sizeof(sin));
		from_sockaddr(&out, &ss);
		memset(&ss, 0, sizeof(ss));
		memcpy(&ss, &sin6, sizeof(sin6));
		from_sockaddr(&out, &ss);
	}
}

/* Pairs beyond upstream's, including families it doesn't use and text that doesn't parse. */
static void sweep_cmp(void)
{
	static const struct {
		int af;
		const char *ip;
		uint16_t port;
	} extra[] = {
		/* Each pair is in order, yet the three go round in a cycle. */
		{ AF_INET, "2.0.0.0", 1 }, { AF_INET, "10.0.0.0", 1 }, { AF_INET, "1x", 1 },
		{ AF_INET6, "::ffff:1.2.3.4", 1 }, { AF_INET6, "::ffff:102:304", 1 },
		{ AF_INET6, "::ffff:102:304", 2 }, { AF_INET, "1.2.3.4", 1 }, { AF_INET, "1.2.3.4", 2 },
		{ JUNK_AF, "1.2.3.4", 1 }, { JUNK_AF, "1.2.3.4", 2 }, { JUNK_AF, "1.2.3.5", 0 },
		{ -1, "1.2.3.4", 1 }, { AF_INET, "\xff", 1 }, { AF_INET, "a", 1 },
		{ AF_INET6, "zz", 3 }, { AF_INET6, "::2", 1 },
	};
	const size_t upstream_count = ARRAY_SIZE(upstream_data);
	const size_t count = upstream_count + ARRAY_SIZE(extra);
	struct osmo_sockaddr_str all[ARRAY_SIZE(upstream_data) + ARRAY_SIZE(extra)];

	for (size_t i = 0; i < count; i++)
		all[i] = i < upstream_count
			? make(upstream_data[i].af, upstream_data[i].ip, upstream_data[i].port)
			: make(extra[i - upstream_count].af, extra[i - upstream_count].ip,
			       extra[i - upstream_count].port);
	for (size_t i = 0; i < count; i++)
		for (size_t j = 0; j < count; j++)
			if (i >= upstream_count || j >= upstream_count)
				record_cmp(&all[i], &all[j]);
	CHECK(osmo_sockaddr_str_cmp(&all[upstream_count], &all[upstream_count + 1]) < 0);
	CHECK(osmo_sockaddr_str_cmp(&all[upstream_count + 1], &all[upstream_count + 2]) < 0);
	CHECK(osmo_sockaddr_str_cmp(&all[upstream_count + 2], &all[upstream_count]) < 0);
}

static void constants(void)
{
	static const struct {
		const char *name;
		int value;
	} values[] = {
		{ "AF_UNSPEC", AF_UNSPEC }, { "AF_INET", AF_INET }, { "AF_INET6", AF_INET6 },
		{ "INET6_ADDRSTRLEN", INET6_ADDRSTRLEN }, { "EINVAL", EINVAL }, { "ENOSPC", ENOSPC },
		{ "EIO", EIO }, { "EAFNOSUPPORT", EAFNOSUPPORT },
	};

	for (size_t i = 0; i < ARRAY_SIZE(values); i++)
		printf("{\"kind\":\"constant\",\"name\":\"%s\",\"value\":%d}\n", values[i].name,
		       values[i].value);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s sockaddr_str_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
	CHECK(sizeof(struct osmo_sockaddr_str) == 52);
	for (size_t i = 0; i < SECTION_COUNT; i++) {
		sections[i] = open_memstream(&section_text[i], &section_size[i]);
		CHECK(sections[i]);
	}

	upstream_replay(argv[1]);
	sweep_texts();
	sweep_lengths();
	sweep_in_addr();
	sweep_in6_addr();
	sweep_sockaddrs();
	sweep_cmp();

	printf("{\"schema\":\"sockaddr_str-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	constants();
	for (size_t i = 0; i < SECTION_COUNT; i++) {
		fclose(sections[i]);
		fwrite(section_text[i], 1, section_size[i], stdout);
		free(section_text[i]);
	}
	return EXIT_SUCCESS;
}
