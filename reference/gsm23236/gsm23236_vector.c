/*
 * Reference harness for libosmogsm's 3GPP TS 23.236 Network Resource Indicators (NRI) of
 * gsm23236.c: NRI bits in a TMSI, NRI value and range validation, range lists with their
 * add, delete, match and overlap rules, limiting an NRI to a list, the list's text, and the
 * VTY argument parsers.
 *
 * Calls the API directly and writes NDJSON: a metadata line, then one section per kind of
 * record. The first argument is upstream's gsm23236_test.ok. Upstream's gsm23236_test is
 * replayed with the same calls, and the run fails unless its output matches the file byte
 * for byte.
 *
 * osmo_nri_v_limit_by_ranges() divides by the number of NRI values its list has within the
 * bit length, which can be 0. Every limit call runs in a child process, and a record holds
 * the signal that ended it, or its results.
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
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <talloc.h>

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm23236.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0x55
/* What out-parameters hold beforehand, to show whether a call wrote them. */
#define SENTINEL16 ((int16_t)0x5555)
/* The values 0 to DOMAIN - 1, whose every subset is one of the swept lists. */
#define DOMAIN 5
#define RANGES_TEXT 512

enum section {
	TMSI_GET, TMSI_SET, VALIDATE, RANGE_VALIDATE, ADD, DEL, MATCHES, OVERLAPS, LIMIT, TMSI_LIMIT,
	TO_STR, TO_STR_C, VTY_ADD, VTY_DEL, SECTION_COUNT
};

static const char *const kind_names[SECTION_COUNT] = {
	"tmsi_get", "tmsi_set", "validate", "range_validate", "add", "del", "matches", "overlaps",
	"limit", "tmsi_limit", "to_str", "to_str_c", "vty_add", "vty_del",
};

static FILE *sections[SECTION_COUNT];
static char *section_text[SECTION_COUNT];
static size_t section_size[SECTION_COUNT];
static const char *source = "sweep";
static void *ctx;

static void fail(const char *what)
{
	fprintf(stderr, "reference expectation failed: %s\n", what);
	exit(EXIT_FAILURE);
}

#define CHECK(condition) do { if (!(condition)) fail(#condition); } while (0)

static FILE *start(enum section section)
{
	fprintf(sections[section], "{\"kind\":\"%s\",\"source\":\"%s\"", kind_names[section], source);
	return sections[section];
}

static void finish(FILE *f)
{
	fprintf(f, "}\n");
}

static void hex_bytes(FILE *f, const char *key, const void *data, size_t length)
{
	fprintf(f, ",\"%s\":\"", key);
	for (size_t i = 0; i < length; i++)
		fprintf(f, "%02x", ((const unsigned char *)data)[i]);
	fprintf(f, "\"");
}

/* A C string as hex, or null. */
static void hex_string(FILE *f, const char *key, const char *text)
{
	if (text)
		hex_bytes(f, key, text, strlen(text));
	else
		fprintf(f, ",\"%s\":null", key);
}

/* The list as the harness writes it: "[first..last]" for each entry, in list order. */
static void ranges_text(char *out, const struct osmo_nri_ranges *nri_ranges)
{
	struct osmo_nri_range *range;
	size_t at = 0;

	out[0] = '\0';
	llist_for_each_entry(range, &nri_ranges->entries, entry) {
		int n = snprintf(out + at, RANGES_TEXT - at, "[%d..%d]", range->first, range->last);
		CHECK(n > 0 && (size_t)n < RANGES_TEXT - at);
		at += n;
	}
}

static void ranges_field(FILE *f, const char *key, const struct osmo_nri_ranges *nri_ranges)
{
	char text[RANGES_TEXT];

	ranges_text(text, nri_ranges);
	fprintf(f, ",\"%s\":\"%s\"", key, text);
}

static struct osmo_nri_ranges *ranges_of(const int16_t (*pairs)[2], size_t count)
{
	struct osmo_nri_ranges *nri_ranges = osmo_nri_ranges_alloc(ctx);

	for (size_t i = 0; i < count; i++) {
		struct osmo_nri_range range = { .first = pairs[i][0], .last = pairs[i][1] };
		CHECK(osmo_nri_ranges_add(nri_ranges, &range) == 0);
	}
	return nri_ranges;
}

/* The list of the values whose bits are set in `mask`. */
static struct osmo_nri_ranges *ranges_of_mask(unsigned mask)
{
	struct osmo_nri_ranges *nri_ranges = osmo_nri_ranges_alloc(ctx);

	for (int v = 0; v < DOMAIN; v++) {
		struct osmo_nri_range range = { .first = v };
		if (!(mask & (1u << v)))
			continue;
		while (v + 1 < DOMAIN && (mask & (1u << (v + 1))))
			v++;
		range.last = v;
		CHECK(osmo_nri_ranges_add(nri_ranges, &range) == 0);
	}
	return nri_ranges;
}

/* ---- Recorded calls ---- */

static int rec_tmsi_get(int16_t *nri_v, uint32_t tmsi, uint8_t nri_bitlen)
{
	int rc;
	FILE *f;

	*nri_v = SENTINEL16;
	rc = osmo_tmsi_nri_v_get(nri_v, tmsi, nri_bitlen);
	f = start(TMSI_GET);
	fprintf(f, ",\"tmsi\":%u,\"nri_bitlen\":%u,\"rc\":%d,\"nri_v\":%d", tmsi, nri_bitlen, rc, *nri_v);
	finish(f);
	return rc;
}

static int rec_tmsi_set(uint32_t *tmsi, int16_t nri_v, uint8_t nri_bitlen)
{
	uint32_t before = *tmsi;
	int rc = osmo_tmsi_nri_v_set(tmsi, nri_v, nri_bitlen);
	FILE *f = start(TMSI_SET);

	fprintf(f, ",\"tmsi\":%u,\"nri_v\":%d,\"nri_bitlen\":%u,\"rc\":%d,\"out\":%u", before, nri_v,
		nri_bitlen, rc, *tmsi);
	finish(f);
	return rc;
}

static int rec_validate(int16_t nri_v, uint8_t nri_bitlen)
{
	int rc = osmo_nri_v_validate(nri_v, nri_bitlen);
	FILE *f = start(VALIDATE);

	fprintf(f, ",\"nri_v\":%d,\"nri_bitlen\":%u,\"rc\":%d", nri_v, nri_bitlen, rc);
	finish(f);
	return rc;
}

static int rec_range_validate(const struct osmo_nri_range *range, uint8_t nri_bitlen)
{
	int rc = osmo_nri_range_validate(range, nri_bitlen);
	FILE *f = start(RANGE_VALIDATE);

	fprintf(f, ",\"first\":%d,\"last\":%d,\"nri_bitlen\":%u,\"rc\":%d", range->first, range->last,
		nri_bitlen, rc);
	finish(f);
	return rc;
}

static int rec_add_del(enum section section, struct osmo_nri_ranges *nri_ranges, const struct osmo_nri_range *range)
{
	char before[RANGES_TEXT];
	int rc;
	FILE *f;

	ranges_text(before, nri_ranges);
	rc = section == ADD ? osmo_nri_ranges_add(nri_ranges, range) : osmo_nri_ranges_del(nri_ranges, range);
	f = start(section);
	fprintf(f, ",\"ranges\":\"%s\",\"first\":%d,\"last\":%d,\"rc\":%d", before, range->first, range->last, rc);
	ranges_field(f, "out", nri_ranges);
	finish(f);
	return rc;
}

static bool rec_matches(int16_t nri_v, const struct osmo_nri_ranges *nri_ranges)
{
	bool matches = osmo_nri_v_matches_ranges(nri_v, nri_ranges);
	FILE *f = start(MATCHES);

	ranges_field(f, "ranges", nri_ranges);
	fprintf(f, ",\"nri_v\":%d,\"matches\":%s", nri_v, matches ? "true" : "false");
	finish(f);
	return matches;
}

static bool rec_overlaps(const struct osmo_nri_range *range, const struct osmo_nri_ranges *nri_ranges)
{
	bool overlaps = osmo_nri_range_overlaps_ranges(range, nri_ranges);
	FILE *f = start(OVERLAPS);

	ranges_field(f, "ranges", nri_ranges);
	fprintf(f, ",\"first\":%d,\"last\":%d,\"overlaps\":%s", range->first, range->last,
		overlaps ? "true" : "false");
	finish(f);
	return overlaps;
}

/* A child reports a fatal signal by its exit status, without a core dump. */
#define SIGNAL_EXIT 100

static void exit_on_signal(int signal)
{
	_exit(SIGNAL_EXIT + signal);
}

struct child_result {
	int rc;
	int64_t out;
};

/* Runs a limit call in a child. Returns the signal that ended it, or 0 with its results. */
static int in_child(struct child_result *result, bool tmsi_call, const struct osmo_nri_ranges *nri_ranges,
		    int64_t value, uint32_t nri_bitlen)
{
	int fds[2], status;
	pid_t pid;

	fflush(NULL);
	CHECK(pipe(fds) == 0);
	pid = fork();
	CHECK(pid >= 0);
	if (pid == 0) {
		static const int fatal[] = { SIGFPE, SIGSEGV, SIGBUS, SIGILL, SIGABRT };
		struct sigaction action = { .sa_handler = exit_on_signal };
		struct child_result r;

		close(fds[0]);
		for (size_t i = 0; i < ARRAY_SIZE(fatal); i++)
			CHECK(sigaction(fatal[i], &action, NULL) == 0);
		if (tmsi_call) {
			uint32_t tmsi = value;
			r.rc = osmo_tmsi_nri_v_limit_by_ranges(&tmsi, nri_ranges, nri_bitlen);
			r.out = tmsi;
		} else {
			int16_t nri_v = value;
			r.rc = osmo_nri_v_limit_by_ranges(&nri_v, nri_ranges, nri_bitlen);
			r.out = nri_v;
		}
		_exit(write(fds[1], &r, sizeof(r)) == sizeof(r) ? 0 : 1);
	}
	close(fds[1]);
	CHECK(waitpid(pid, &status, 0) == pid);
	CHECK(WIFEXITED(status));
	if (WEXITSTATUS(status) > SIGNAL_EXIT) {
		close(fds[0]);
		return WEXITSTATUS(status) - SIGNAL_EXIT;
	}
	CHECK(WEXITSTATUS(status) == 0);
	CHECK(read(fds[0], result, sizeof(*result)) == sizeof(*result));
	close(fds[0]);
	return 0;
}

static void limit_results(FILE *f, int signal, const struct child_result *result)
{
	if (signal)
		fprintf(f, ",\"signal\":%d,\"rc\":null,\"out\":null", signal);
	else
		fprintf(f, ",\"signal\":0,\"rc\":%d,\"out\":%lld", result->rc, (long long)result->out);
	finish(f);
}

static int rec_limit(int16_t *nri_v, const struct osmo_nri_ranges *nri_ranges, uint32_t nri_bitlen)
{
	struct child_result result;
	int signal = in_child(&result, false, nri_ranges, *nri_v, nri_bitlen);
	FILE *f = start(LIMIT);

	ranges_field(f, "ranges", nri_ranges);
	fprintf(f, ",\"nri_v\":%d,\"nri_bitlen\":%u", *nri_v, nri_bitlen);
	limit_results(f, signal, &result);
	CHECK(!signal || !strcmp(source, "sweep"));
	if (!signal)
		*nri_v = result.out;
	return signal ? -1 : result.rc;
}

static int rec_tmsi_limit(uint32_t *tmsi, const struct osmo_nri_ranges *nri_ranges, uint8_t nri_bitlen)
{
	struct child_result result;
	int signal = in_child(&result, true, nri_ranges, *tmsi, nri_bitlen);
	FILE *f = start(TMSI_LIMIT);

	ranges_field(f, "ranges", nri_ranges);
	fprintf(f, ",\"tmsi\":%u,\"nri_bitlen\":%u", *tmsi, nri_bitlen);
	limit_results(f, signal, &result);
	CHECK(!signal || !strcmp(source, "sweep"));
	if (!signal)
		*tmsi = result.out;
	return signal ? -1 : result.rc;
}

static void rec_to_str(const struct osmo_nri_ranges *nri_ranges, size_t buflen)
{
	char buf[RANGES_TEXT];
	int rc;
	FILE *f;

	CHECK(buflen < sizeof(buf));
	memset(buf, GUARD, sizeof(buf));
	rc = osmo_nri_ranges_to_str_buf(buf, buflen, nri_ranges);
	for (size_t i = buflen; i < sizeof(buf); i++)
		CHECK(buf[i] == GUARD);
	f = start(TO_STR);
	ranges_field(f, "ranges", nri_ranges);
	fprintf(f, ",\"buflen\":%zu,\"rc\":%d", buflen, rc);
	hex_bytes(f, "out", buf, buflen + 1);
	finish(f);
}

static void rec_to_str_c(const struct osmo_nri_ranges *nri_ranges)
{
	char *text = osmo_nri_ranges_to_str_c(ctx, nri_ranges);
	FILE *f;

	CHECK(text);
	f = start(TO_STR_C);
	ranges_field(f, "ranges", nri_ranges);
	hex_bytes(f, "out", text, strlen(text));
	finish(f);
	talloc_free(text);
}

/* osmo_nri_ranges_vty_add() or, for nri_bitlen < 0, osmo_nri_ranges_vty_del(), with argv[0]
 * and argv[1] as given and argc of 1 or 2. */
static int rec_vty(struct osmo_nri_ranges *nri_ranges, int argc, const char *arg0, const char *arg1,
		   int nri_bitlen)
{
	const char *argv[2] = { arg0, arg1 };
	const char *message = "untouched";
	struct osmo_nri_range parsed = { .first = SENTINEL16, .last = SENTINEL16 };
	char before[RANGES_TEXT];
	bool add = nri_bitlen >= 0;
	int rc;
	FILE *f;

	CHECK(argc == 1 || argc == 2);
	CHECK(argc == 2 || !arg1);
	ranges_text(before, nri_ranges);
	if (add)
		rc = osmo_nri_ranges_vty_add(&message, &parsed, nri_ranges, argc, argv, nri_bitlen);
	else
		rc = osmo_nri_ranges_vty_del(&message, &parsed, nri_ranges, argc, argv);
	CHECK(!message || strcmp(message, "untouched"));
	CHECK(!message || (!strchr(message, '"') && !strchr(message, ',')));
	f = start(add ? VTY_ADD : VTY_DEL);
	fprintf(f, ",\"ranges\":\"%s\",\"argc\":%d", before, argc);
	hex_string(f, "argv0", arg0);
	hex_string(f, "argv1", arg1);
	if (add)
		fprintf(f, ",\"nri_bitlen\":%d", nri_bitlen);
	fprintf(f, ",\"rc\":%d", rc);
	if (message)
		fprintf(f, ",\"message\":\"%s\"", message);
	else
		fprintf(f, ",\"message\":null");
	fprintf(f, ",\"parsed_first\":%d,\"parsed_last\":%d", parsed.first, parsed.last);
	ranges_field(f, "out", nri_ranges);
	finish(f);
	return rc;
}

/* ---- Upstream's test, replayed ---- */

static bool ok = true;

static void bitdump(FILE *out, uint8_t count, uint32_t val)
{
	uint32_t bit;

	if (count < 1)
		return;
	for (bit = ((uint32_t)1) << (count - 1); bit; bit >>= 1)
		fprintf(out, "%c", (val & bit) ? '1' : '0');
}

static const struct nri_v_get_set_test {
	uint32_t tmsi;
	uint8_t nri_bitlen;
	int16_t expect_get_nri;
	int expect_get_rc;
	int16_t set_nri_v;
	uint32_t expect_tmsi;
	int expect_set_rc;
} nri_v_get_set_tests[] = {
	{ .tmsi = 0, .nri_bitlen = 10, .expect_get_nri = 0, .set_nri_v = 0, .expect_tmsi = 0 },
	{ .tmsi = 0, .nri_bitlen = 10, .expect_get_nri = 0, .set_nri_v = 0x7fff, .expect_tmsi = 0x00ffc000 },
	{ .tmsi = 0xffffffff, .nri_bitlen = 10, .expect_get_nri = 0x3ff, .set_nri_v = 0, .expect_tmsi = 0xff003fff },
	{ .tmsi = 0xffffffff, .nri_bitlen = 10, .expect_get_nri = 0x3ff, .set_nri_v = 0x7fff,
	  .expect_tmsi = 0xffffffff },
	{ .tmsi = 0, .nri_bitlen = 5, .expect_get_nri = 0, .set_nri_v = 0, .expect_tmsi = 0 },
	{ .tmsi = 0, .nri_bitlen = 5, .expect_get_nri = 0, .set_nri_v = 0x7fff, .expect_tmsi = 0x00f80000 },
	{ .tmsi = 0xffffffff, .nri_bitlen = 5, .expect_get_nri = 0x1f, .set_nri_v = 0, .expect_tmsi = 0xff07ffff },
	{ .tmsi = 0xffffffff, .nri_bitlen = 5, .expect_get_nri = 0x1f, .set_nri_v = 0x7fff,
	  .expect_tmsi = 0xffffffff },
	{ .tmsi = 0x01234567, .nri_bitlen = 8, .expect_get_nri = 0x23, .set_nri_v = 0x42,
	  .expect_tmsi = 0x01424567 },
	{ .tmsi = 0x01234567, .nri_bitlen = 15, .expect_get_nri = 0x2345 >> 1, .set_nri_v = 0x7fff,
	  .expect_tmsi = 0x01ffff67 },
	{ .tmsi = 0x01234567, .nri_bitlen = 16, .expect_get_rc = -1, .expect_get_nri = -1, .set_nri_v = 0x7fff,
	  .expect_set_rc = -1, .expect_tmsi = 0x01234567 },
	{ .tmsi = 0x01234567, .nri_bitlen = 0, .expect_get_rc = -1, .expect_get_nri = -1, .set_nri_v = 0x7fff,
	  .expect_set_rc = -1, .expect_tmsi = 0x01234567 },
};

static void test_nri_v_get_set(FILE *out)
{
	for (size_t i = 0; i < ARRAY_SIZE(nri_v_get_set_tests); i++) {
		const struct nri_v_get_set_test *t = &nri_v_get_set_tests[i];
		int16_t nri_v = 0;
		uint32_t tmsi2;
		int rc;

		rc = rec_tmsi_get(&nri_v, t->tmsi, t->nri_bitlen);
		fprintf(out, "\nosmo_tmsi_nri_v_get(0x%08x, %u) -> nri_v=0x%x rc=%d\n", t->tmsi, t->nri_bitlen,
			nri_v, rc);
		if (!rc) {
			fprintf(out, "........|NRI->..................\n");
			bitdump(out, 32, t->tmsi);
			fprintf(out, " tmsi  nri_bitlen=%u\n", t->nri_bitlen);
			fprintf(out, "        ");
			bitdump(out, t->nri_bitlen, nri_v);
			fprintf(out, " = 0x%x", nri_v);
		}
		if (nri_v == t->expect_get_nri && rc == t->expect_get_rc) {
			fprintf(out, " ok\n");
		} else {
			fprintf(out, " ERROR: expected nri_v=0x%x rc=%d\n", t->expect_get_nri, t->expect_get_rc);
			ok = false;
		}

		tmsi2 = t->tmsi;
		rc = rec_tmsi_set(&tmsi2, t->set_nri_v, t->nri_bitlen);
		fprintf(out, "osmo_tmsi_nri_v_set(0x%08x, 0x%x, %u) -> tmsi=0x%08x rc=%d\n", t->tmsi, t->set_nri_v,
			t->nri_bitlen, tmsi2, rc);
		if (!rc) {
			fprintf(out, "        ");
			bitdump(out, t->nri_bitlen, t->set_nri_v);
			fprintf(out, "\n");
			bitdump(out, 32, tmsi2);
		}
		if (tmsi2 == t->expect_tmsi && rc == t->expect_set_rc) {
			fprintf(out, " ok\n");
		} else {
			fprintf(out, " ERROR: expected tmsi=0x%08x rc=%d\n", t->expect_tmsi, t->expect_set_rc);
			ok = false;
		}
	}
}

static const struct nri_validate_tc {
	int16_t nri;
	uint8_t nri_bitlen;
	int expect_rc;
} nri_validate_tests[] = {
	{ INT16_MIN, 10, -1 }, { -23, 10, -1 }, { -1, 10, -1 }, { 0, 10, 0 }, { (1 << 10) - 1, 10, 0 },
	{ (1 << 10), 10, 1 }, { INT16_MAX, 10, 1 },
	{ INT16_MIN, 5, -1 }, { -23, 5, -1 }, { -1, 5, -1 }, { 0, 5, 0 }, { (1 << 5) - 1, 5, 0 },
	{ (1 << 5), 5, 1 }, { INT16_MAX, 5, 1 },
	{ INT16_MIN, 1, -1 }, { -23, 1, -1 }, { -1, 1, -1 }, { 0, 1, 0 }, { 1, 1, 0 }, { 2, 1, 1 },
	{ INT16_MAX, 1, 1 },
	{ INT16_MIN, 0, -1 }, { -23, 0, -1 }, { -1, 0, -1 }, { 0, 0, 1 }, { 1, 0, 1 }, { INT16_MAX, 0, 1 },
};

static void test_nri_validate(FILE *out)
{
	fprintf(out, "\n%s()\n", "test_nri_validate");
	for (size_t i = 0; i < ARRAY_SIZE(nri_validate_tests); i++) {
		const struct nri_validate_tc *t = &nri_validate_tests[i];
		int rc = rec_validate(t->nri, t->nri_bitlen);
		fprintf(out, "osmo_nri_v_validate(%d, %u) = %d ", t->nri, t->nri_bitlen, rc);
		if (rc == t->expect_rc) {
			fprintf(out, "ok\n");
		} else {
			fprintf(out, "ERROR, expected rc = %d\n", t->expect_rc);
			ok = false;
		}
	}
}

static const struct nri_range_validate_tc {
	int16_t first;
	int16_t last;
	uint8_t nri_bitlen;
	int expect_rc;
} nri_range_validate_tests[] = {
	{ INT16_MIN, INT16_MIN, 10, -1 }, { -23, -23, 10, -1 }, { -1, -1, 10, -1 }, { 0, 0, 10, 0 },
	{ (1 << 10) - 1, (1 << 10) - 1, 10, 0 }, { (1 << 10), (1 << 10), 10, 1 }, { INT16_MAX, INT16_MAX, 10, 1 },
	{ INT16_MIN, INT16_MIN, 5, -1 }, { -23, -23, 5, -1 }, { -1, -1, 5, -1 }, { 0, 0, 5, 0 },
	{ (1 << 5) - 1, (1 << 5) - 1, 5, 0 }, { (1 << 5), (1 << 5), 5, 1 }, { INT16_MAX, INT16_MAX, 5, 1 },
	{ INT16_MIN, INT16_MIN, 1, -1 }, { -23, -23, 1, -1 }, { -1, -1, 1, -1 }, { 0, 0, 1, 0 }, { 1, 1, 1, 0 },
	{ 2, 2, 1, 1 }, { INT16_MAX, INT16_MAX, 1, 1 },
	{ INT16_MIN, INT16_MIN, 0, -1 }, { -23, -23, 0, -1 }, { -1, -1, 0, -1 }, { 0, 0, 0, 1 }, { 1, 1, 0, 1 },
	{ INT16_MAX, INT16_MAX, 0, 1 },
	{ 0, INT16_MIN, 10, -2 }, { 0, -23, 10, -2 }, { 0, -1, 10, -2 }, { 0, 0, 10, 0 },
	{ 0, (1 << 10) - 1, 10, 0 }, { 0, (1 << 10), 10, 2 }, { 0, INT16_MAX, 10, 2 },
	{ 0, INT16_MIN, 5, -2 }, { 0, -23, 5, -2 }, { 0, -1, 5, -2 }, { 0, 0, 5, 0 }, { 0, (1 << 5) - 1, 5, 0 },
	{ 0, (1 << 5), 5, 2 }, { 0, INT16_MAX, 5, 2 },
	{ 0, INT16_MIN, 1, -2 }, { 0, -23, 1, -2 }, { 0, -1, 1, -2 }, { 0, 0, 1, 0 }, { 0, 1, 1, 0 },
	{ 0, 2, 1, 2 }, { 0, INT16_MAX, 1, 2 },
	{ 0, INT16_MIN, 0, 1 }, { 0, -23, 0, 1 }, { 0, -1, 0, 1 }, { 0, 0, 0, 1 }, { 0, 1, 0, 1 },
	{ 0, INT16_MAX, 0, 1 },
	{ 0, 0, 10, 0 }, { 1, 0, 10, -3 }, { (1 << 10) - 1, (1 << 10) - 1, 10, 0 },
	{ (1 << 10) - 1, (1 << 10) - 2, 10, -3 }, { (1 << 10) - 1, 0, 10, -3 },
	{ 0, 0, 5, 0 }, { 1, 0, 5, -3 }, { (1 << 5) - 1, (1 << 5) - 1, 5, 0 }, { (1 << 5) - 1, (1 << 5) - 2, 5, -3 },
	{ (1 << 5) - 1, 0, 5, -3 },
	{ 0, 0, 1, 0 }, { 1, 1, 1, 0 }, { 1, 0, 1, -3 },
};

static void test_nri_range_validate(FILE *out)
{
	fprintf(out, "\n%s()\n", "test_nri_range_validate");
	for (size_t i = 0; i < ARRAY_SIZE(nri_range_validate_tests); i++) {
		const struct nri_range_validate_tc *t = &nri_range_validate_tests[i];
		struct osmo_nri_range range = { .first = t->first, .last = t->last };
		int rc = rec_range_validate(&range, t->nri_bitlen);
		fprintf(out, "osmo_nri_range_validate({%d,%d}, %u) = %d ", range.first, range.last, t->nri_bitlen,
			rc);
		if (rc == t->expect_rc) {
			fprintf(out, "ok\n");
		} else {
			fprintf(out, "ERROR, expected rc = %d\n", t->expect_rc);
			ok = false;
		}
	}
}

/* Upstream's dump_list() checks each entry with osmo_nri_range_validate(); it is not recorded. */
static void dump_list(FILE *out, const struct osmo_nri_ranges *nri_ranges)
{
	struct osmo_nri_range *r;

	fprintf(out, "nri_ranges = {\n");
	llist_for_each_entry(r, &nri_ranges->entries, entry) {
		fprintf(out, "  { %d, %d },\n", r->first, r->last);
		if (osmo_nri_range_validate(r, 255)) {
			ok = false;
			fprintf(out, "    ^^^^^ ERROR: invalid range\n");
		}
	}
	fprintf(out, "};\n");
}

#define ADD(FIRST, LAST) do { \
		struct osmo_nri_range r = { .first = FIRST, .last = LAST }; \
		int rc = rec_add_del(ADD, nri_ranges, &r); \
		fprintf(out, "osmo_nri_ranges_add(%d, %d) -> %d\n", r.first, r.last, rc); \
		dump_list(out, nri_ranges); \
	} while (0)

#define DEL(FIRST, LAST) do { \
		struct osmo_nri_range r = { .first = FIRST, .last = LAST }; \
		int rc = rec_add_del(DEL, nri_ranges, &r); \
		fprintf(out, "osmo_nri_ranges_del(%d, %d) -> %d\n", r.first, r.last, rc); \
		dump_list(out, nri_ranges); \
	} while (0)

#define MATCHES(NRI, EXPECT_MATCH) do { \
		bool matches = rec_matches(NRI, nri_ranges); \
		fprintf(out, "osmo_nri_v_matches_ranges(%d) -> %s\n", NRI, matches ? "true" : "false"); \
		if (matches != EXPECT_MATCH) { \
			ok = false; \
			fprintf(out, "  ^ ERROR: expected " #EXPECT_MATCH "\n"); \
		} \
	} while (0)

#define OVERLAPS(FIRST, LAST, EXPECT_OVERLAP) do { \
		struct osmo_nri_range r = { .first = FIRST, .last = LAST }; \
		bool overlaps = rec_overlaps(&r, nri_ranges); \
		fprintf(out, "osmo_nri_range_overlaps_ranges(%d, %d) -> %s\n", r.first, r.last, \
			overlaps ? "true" : "false"); \
		if (overlaps != EXPECT_OVERLAP) { \
			ok = false; \
			fprintf(out, "  ^ ERROR: expected " #EXPECT_OVERLAP "\n"); \
		} \
	} while (0)

static void test_nri_list(FILE *out)
{
	struct osmo_nri_ranges *nri_ranges = osmo_nri_ranges_alloc(ctx);

	fprintf(out, "\n%s()\n", "test_nri_list");
	dump_list(out, nri_ranges);
	MATCHES(INT16_MIN, false);
	MATCHES(-1, false);
	MATCHES(0, false);
	MATCHES(INT16_MAX, false);
	MATCHES(100, false);
	OVERLAPS(INT16_MIN, -1, false);
	OVERLAPS(-100, 100, false);
	OVERLAPS(10, 20, false);

	ADD(100, 200);
	MATCHES(INT16_MIN, false);
	MATCHES(-1, false);
	MATCHES(0, false);
	MATCHES(INT16_MAX, false);
	MATCHES(99, false);
	MATCHES(100, true);
	MATCHES(101, true);
	MATCHES(199, true);
	MATCHES(200, true);
	MATCHES(201, false);
	OVERLAPS(INT16_MIN, -1, false);
	OVERLAPS(-100, 100, true);
	OVERLAPS(10, 20, false);
	OVERLAPS(10, 99, false);
	OVERLAPS(10, 100, true);
	OVERLAPS(10, 150, true);
	OVERLAPS(99, 99, false);
	OVERLAPS(100, 100, true);
	OVERLAPS(150, 300, true);
	OVERLAPS(200, 300, true);
	OVERLAPS(201, 300, false);

	fprintf(out, "\ndel from start:\n");
	DEL(0, 110);
	DEL(111, 111);
	DEL(112, 199);
	MATCHES(INT16_MIN, false);
	MATCHES(-1, false);
	MATCHES(0, false);
	MATCHES(INT16_MAX, false);
	MATCHES(199, false);
	MATCHES(200, true);
	MATCHES(201, false);
	OVERLAPS(INT16_MIN, -1, false);
	OVERLAPS(-1000, 1000, true);
	OVERLAPS(0, 199, false);
	OVERLAPS(0, 200, true);
	OVERLAPS(0, 201, true);
	OVERLAPS(0, 1000, true);
	OVERLAPS(199, 199, false);
	OVERLAPS(200, 200, true);
	OVERLAPS(201, 201, false);

	fprintf(out, "\ndel from end:\n");
	ADD(100, 200);
	DEL(190, INT16_MAX);
	DEL(189, 189);
	DEL(101, 188);
	MATCHES(INT16_MIN, false);
	MATCHES(-1, false);
	MATCHES(0, false);
	MATCHES(INT16_MAX, false);
	MATCHES(99, false);
	MATCHES(100, true);
	MATCHES(101, false);

	fprintf(out, "\ndel from middle:\n");
	ADD(100, 200);
	DEL(150, 160);
	DEL(110, 120);
	DEL(130, 130);
	DEL(180, 190);
	MATCHES(INT16_MIN, false);
	MATCHES(-1, false);
	MATCHES(0, false);
	MATCHES(INT16_MAX, false);
	MATCHES(99, false);
	MATCHES(100, true);
	MATCHES(109, true);
	MATCHES(110, false);
	MATCHES(120, false);
	MATCHES(121, true);
	MATCHES(129, true);
	MATCHES(130, false);
	MATCHES(131, true);
	MATCHES(148, true);
	MATCHES(149, true);
	MATCHES(150, false);
	MATCHES(160, false);
	MATCHES(161, true);
	MATCHES(170, true);
	MATCHES(179, true);
	MATCHES(180, false);
	MATCHES(185, false);
	MATCHES(190, false);
	MATCHES(191, true);
	MATCHES(195, true);
	MATCHES(200, true);
	MATCHES(201, false);
	MATCHES(1000, false);
	OVERLAPS(110, 120, false);
	OVERLAPS(110, 130, true);
	OVERLAPS(100, 200, true);

	fprintf(out, "\ndel across whole chunks:\n");
	DEL(115, 185);
	DEL(105, 195);
	DEL(0, 1000);

	fprintf(out, "\nadd to join chunks:\n");
	ADD(0, 100);
	DEL(11, 19);
	DEL(23, 23);
	DEL(30, 41);
	ADD(23, 23);
	ADD(11, 41);
	MATCHES(0, true);
	MATCHES(10, true);
	MATCHES(11, true);
	MATCHES(24, true);
	MATCHES(41, true);
	MATCHES(42, true);
	MATCHES(100, true);
	MATCHES(101, false);

	fprintf(out, "\nborder cases:\n");
	ADD(0, 0);
	ADD(INT16_MAX, INT16_MAX);
	ADD(1, INT16_MAX - 1);
	MATCHES(INT16_MIN, false);
	MATCHES(-1, false);
	MATCHES(0, true);
	MATCHES(INT16_MAX, true);
	DEL(0, 0);
	DEL(INT16_MAX, INT16_MAX);
	DEL(1, INT16_MAX - 1);

	fprintf(out, "\nrange errors:\n");
	ADD(-1, -1);
	ADD(-20, -10);
	ADD(100, 1);
	ADD(0, INT16_MAX);
	DEL(-1, -1);
	DEL(-20, -10);
	DEL(100, 1);
}

static void test_nri_limit_by_ranges(FILE *out)
{
	const uint8_t nri_bitlen = 8;
	const int16_t expect_nri_vals[] = { 10, 20, 21, 30, 31, 32 };
	struct osmo_nri_ranges *nri_ranges = osmo_nri_ranges_alloc(ctx);
	int i;

	fprintf(out, "\n%s()\n", "test_nri_limit_by_ranges");
	ADD(10, 10);
	ADD(20, 21);
	ADD(30, 32);

	for (i = 0; i < 19; i++) {
		int16_t expect_nri_v = expect_nri_vals[i % ARRAY_SIZE(expect_nri_vals)];
		int16_t nri_v = i;
		int rc = rec_limit(&nri_v, nri_ranges, nri_bitlen);
		fprintf(out, "osmo_nri_v_limit_by_ranges(%d) -> nri_v=%d rc=%d", i, nri_v, rc);
		if (!rc && nri_v == expect_nri_v) {
			fprintf(out, " ok\n");
		} else {
			fprintf(out, " ERROR: expected nri_v=%d rc=0\n", expect_nri_v);
			ok = false;
		}
	}

	for (i = 0; i < 19; i++) {
		int16_t expect_nri_v = expect_nri_vals[i % ARRAY_SIZE(expect_nri_vals)];
		int16_t nri_v;
		uint32_t tmsi = 0, tmsi2;
		int rc;

		rec_tmsi_set(&tmsi, i, nri_bitlen);
		tmsi2 = tmsi;
		rc = rec_tmsi_limit(&tmsi2, nri_ranges, nri_bitlen);
		rec_tmsi_get(&nri_v, tmsi2, nri_bitlen);
		fprintf(out, "osmo_tmsi_nri_v_limit_by_ranges(0x%08x, %u) -> tmsi=0x%08x nri_v=%d rc=%d",
			tmsi, nri_bitlen, tmsi2, nri_v, rc);
		if (!rc && nri_v == expect_nri_v) {
			fprintf(out, " ok\n");
		} else {
			fprintf(out, " ERROR: expected nri_v=%d rc=0\n", expect_nri_v);
			ok = false;
		}
	}
}

static void replay(FILE *out)
{
	test_nri_v_get_set(out);
	test_nri_validate(out);
	test_nri_range_validate(out);
	test_nri_list(out);
	test_nri_limit_by_ranges(out);
	CHECK(ok);
	fprintf(out, "\npass\n");
}

static void upstream_replay(const char *ok_path)
{
	FILE *file, *replayed_out;
	char *expected, *replayed;
	size_t expected_size, replayed_size;

	replayed_out = open_memstream(&replayed, &replayed_size);
	CHECK(replayed_out);
	source = "upstream";
	replay(replayed_out);
	source = "sweep";
	fclose(replayed_out);

	file = fopen(ok_path, "r");
	CHECK(file);
	CHECK(fseek(file, 0, SEEK_END) == 0);
	expected_size = ftell(file);
	CHECK(fseek(file, 0, SEEK_SET) == 0);
	expected = calloc(1, expected_size + 1);
	CHECK(expected && fread(expected, 1, expected_size, file) == expected_size);
	fclose(file);
	if (replayed_size != expected_size || memcmp(replayed, expected, expected_size)) {
		fwrite(replayed, 1, replayed_size, stderr);
		fail("replayed output differs from gsm23236_test.ok");
	}
	free(expected);
	free(replayed);
}

/* ---- Sweeps ---- */

static const uint8_t bitlens[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 255 };

static const uint32_t tmsis[] = {
	0, 0xffffffff, 0x01234567, 0x00800000, 0x007fffff, 0x80000000, 0x000001ff, 0xc0ffee00, 0x12345678,
};

static void sweep_tmsi(void)
{
	static const int16_t set_values[] = { INT16_MIN, -1, 0, 1, 0x155, 0x3ff, 0x400, 0x7fff };

	for (size_t t = 0; t < ARRAY_SIZE(tmsis); t++) {
		for (size_t b = 0; b < ARRAY_SIZE(bitlens); b++) {
			int16_t nri_v;
			rec_tmsi_get(&nri_v, tmsis[t], bitlens[b]);
			for (size_t v = 0; t < 3 && v < ARRAY_SIZE(set_values); v++) {
				uint32_t tmsi = tmsis[t];
				rec_tmsi_set(&tmsi, set_values[v], bitlens[b]);
			}
		}
	}
}

static void sweep_validate(void)
{
	int16_t values[64];
	size_t count = 0;

	values[count++] = INT16_MIN;
	values[count++] = -23;
	values[count++] = -1;
	values[count++] = 0;
	for (int k = 1; k <= 15; k++) {
		values[count++] = (1 << k) - 1;
		if (k < 15)
			values[count++] = 1 << k;
	}
	for (size_t b = 0; b < ARRAY_SIZE(bitlens); b++)
		for (size_t v = 0; v < count; v++)
			rec_validate(values[v], bitlens[b]);

	static const int16_t ends[] = { INT16_MIN, -1, 0, 1, 2, 3, 4, 1023, 1024, INT16_MAX };
	static const uint8_t range_bitlens[] = { 0, 1, 2, 10, 15, 16, 255 };
	for (size_t b = 0; b < ARRAY_SIZE(range_bitlens); b++) {
		for (size_t i = 0; i < ARRAY_SIZE(ends); i++) {
			for (size_t j = 0; j < ARRAY_SIZE(ends); j++) {
				struct osmo_nri_range range = { .first = ends[i], .last = ends[j] };
				rec_range_validate(&range, range_bitlens[b]);
			}
		}
	}
}

/* Every list of values below DOMAIN, with every range of ends from -1 to DOMAIN. */
static void sweep_lists(void)
{
	for (unsigned mask = 0; mask < (1u << DOMAIN); mask++) {
		struct osmo_nri_ranges *nri_ranges = ranges_of_mask(mask);

		for (int16_t v = -1; v <= DOMAIN; v++)
			rec_matches(v, nri_ranges);
		rec_matches(INT16_MIN, nri_ranges);
		rec_matches(INT16_MAX, nri_ranges);
		for (int16_t first = -1; first <= DOMAIN; first++) {
			for (int16_t last = -1; last <= DOMAIN; last++) {
				struct osmo_nri_range range = { .first = first, .last = last };
				struct osmo_nri_ranges *changed;

				rec_overlaps(&range, nri_ranges);
				changed = ranges_of_mask(mask);
				rec_add_del(ADD, changed, &range);
				osmo_nri_ranges_free(changed);
				changed = ranges_of_mask(mask);
				rec_add_del(DEL, changed, &range);
				osmo_nri_ranges_free(changed);
			}
		}
		osmo_nri_ranges_free(nri_ranges);
	}

	/* Where the ends of ranges meet the ends of int16_t. */
	static const int16_t big[][2] = { { 0, 0 }, { 2, 32765 }, { 32767, 32767 } };
	static const int16_t edges[][2] = {
		{ 0, 32767 }, { 1, 1 }, { 32766, 32766 }, { 1, 32766 }, { 32767, 32767 }, { 0, 0 },
		{ INT16_MIN, INT16_MAX }, { -1, 0 }, { 32767, 0 }, { 32766, 32767 },
	};
	for (size_t i = 0; i < ARRAY_SIZE(edges); i++) {
		struct osmo_nri_range range = { .first = edges[i][0], .last = edges[i][1] };
		struct osmo_nri_ranges *nri_ranges = ranges_of(big, ARRAY_SIZE(big));
		rec_overlaps(&range, nri_ranges);
		rec_add_del(ADD, nri_ranges, &range);
		osmo_nri_ranges_free(nri_ranges);
		nri_ranges = ranges_of(big, ARRAY_SIZE(big));
		rec_add_del(DEL, nri_ranges, &range);
		osmo_nri_ranges_free(nri_ranges);
	}
}

static void sweep_limit(void)
{
	static const int16_t values[] = { INT16_MIN, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 100, INT16_MAX };
	static const uint32_t limit_bitlens[] = { 0, 1, 2, 3, 15, 16, 17, 30 };
	static const uint8_t tmsi_bitlens[] = { 0, 1, 2, 3, 8, 15, 16 };
	static const uint32_t limit_tmsis[] = { 0, 0xffffffff, 0x12345678, 0x00400000 };

	for (unsigned mask = 0; mask < (1u << DOMAIN); mask++) {
		struct osmo_nri_ranges *nri_ranges = ranges_of_mask(mask);

		for (size_t b = 0; b < ARRAY_SIZE(limit_bitlens); b++) {
			for (size_t v = 0; v < ARRAY_SIZE(values); v++) {
				int16_t nri_v = values[v];
				rec_limit(&nri_v, nri_ranges, limit_bitlens[b]);
			}
		}
		for (size_t b = 0; b < ARRAY_SIZE(tmsi_bitlens); b++) {
			for (size_t t = 0; t < ARRAY_SIZE(limit_tmsis); t++) {
				uint32_t tmsi = limit_tmsis[t];
				rec_tmsi_limit(&tmsi, nri_ranges, tmsi_bitlens[b]);
			}
		}
		osmo_nri_ranges_free(nri_ranges);
	}

	/* Ranges partly and wholly past the bit length, and the widest ranges. */
	static const int16_t lists[][3][2] = {
		{ { 250, 300 } },
		{ { 300, 400 } },
		{ { 100, 200 }, { 300, 400 } },
		{ { 0, 32767 } },
		{ { 0, 0 }, { 32767, 32767 } },
		{ { 1000, 1023 }, { 2000, 3000 } },
	};
	static const size_t list_lens[] = { 1, 1, 2, 1, 2, 2 };
	static const int16_t big_values[] = { 0, 1, 5, 6, 50, 101, 255, 256, 1023, 1024, 32766, INT16_MAX };
	static const uint32_t big_bitlens[] = { 8, 9, 10, 11, 15 };
	static const uint32_t big_tmsis[] = { 0, 0xffffffff, 0x12345678 };
	for (size_t l = 0; l < ARRAY_SIZE(lists); l++) {
		struct osmo_nri_ranges *nri_ranges = ranges_of(lists[l], list_lens[l]);

		for (size_t b = 0; b < ARRAY_SIZE(big_bitlens); b++) {
			for (size_t v = 0; v < ARRAY_SIZE(big_values); v++) {
				int16_t nri_v = big_values[v];
				rec_limit(&nri_v, nri_ranges, big_bitlens[b]);
			}
			for (size_t t = 0; t < ARRAY_SIZE(big_tmsis); t++) {
				uint32_t tmsi = big_tmsis[t];
				rec_tmsi_limit(&tmsi, nri_ranges, big_bitlens[b]);
			}
		}
		osmo_nri_ranges_free(nri_ranges);
	}
}

static void sweep_to_str(void)
{
	static const int16_t lists[][3][2] = {
		{ { 0, 0 } },
		{ { 1, 3 }, { 5, 5 } },
		{ { 0, 32767 } },
		{ { 23, 42 }, { 123, 142 } },
		{ { 100, 200 }, { 300, 400 }, { 1000, 1023 } },
	};
	static const size_t list_lens[] = { 1, 2, 1, 2, 3 };
	struct osmo_nri_ranges *nri_ranges = osmo_nri_ranges_alloc(ctx);

	for (size_t buflen = 0; buflen <= 7; buflen++)
		rec_to_str(nri_ranges, buflen);
	rec_to_str(nri_ranges, 64);
	rec_to_str_c(nri_ranges);
	osmo_nri_ranges_free(nri_ranges);
	for (size_t l = 0; l < ARRAY_SIZE(lists); l++) {
		char text[RANGES_TEXT];
		size_t len;

		nri_ranges = ranges_of(lists[l], list_lens[l]);
		len = osmo_nri_ranges_to_str_buf(text, sizeof(text), nri_ranges);
		for (size_t buflen = 0; buflen <= len + 2; buflen++)
			rec_to_str(nri_ranges, buflen);
		rec_to_str_c(nri_ranges);
		osmo_nri_ranges_free(nri_ranges);
	}
	/* osmo_nri_ranges_to_str_c() starts with 16 bytes: texts of 15, 16 and 17 characters. */
	static const int16_t edges[][3][2] = {
		{ { 0, 1 }, { 3, 4 }, { 6, 10 } },
		{ { 0, 1 }, { 3, 4 }, { 10, 11 } },
		{ { 5, 5 }, { 10000, 10003 } },
		{ { 1000, 10000 }, { 20000, 30000 } },
	};
	static const size_t edge_lens[] = { 3, 3, 2, 2 };
	for (size_t l = 0; l < ARRAY_SIZE(edges); l++) {
		nri_ranges = ranges_of(edges[l], edge_lens[l]);
		rec_to_str(nri_ranges, 16);
		rec_to_str_c(nri_ranges);
		osmo_nri_ranges_free(nri_ranges);
	}
}

static const char *const vty_args[] = {
	"0", "1", "3", "4", "5", "10", "511", "0x1ff", "0x1FF", "0X1ff", "0x", "0x0", "0x00", "0x7fff",
	"0x8000", "0x10000", "32767", "32768", "65535", "-1", "-0", "+1", " 1", "\t2", "1 ", "01", "08",
	"0x08", "0x-1", "0x+1", "0x 1", "0x0x1", "abc", "", "1.5", "1e3", "99999999999999999999",
	"0xffffffffffffffffffff",
};

static void sweep_vty(void)
{
	static const int16_t base[][2] = { { 2, 3 } };
	static const char *const pairs[][2] = {
		{ "1", "2" }, { "2", "1" }, { "0", "7" }, { "0", "8" }, { "7", "8" }, { "8", "9" }, { "0x0", "0x7" },
		{ "8", "8" }, { "32767", "32767" }, { "0", "32767" }, { "4", "0x5" }, { "1", "x" }, { "x", "1" },
		{ "1", "" }, { "1", "32768" }, { "1", "-1" },
	};
	static const int vty_bitlens[] = { 0, 1, 3, 15, 16, 255 };

	for (int state = 0; state < 2; state++) {
		for (size_t a = 0; a < ARRAY_SIZE(vty_args); a++) {
			struct osmo_nri_ranges *nri_ranges = ranges_of(base, state);
			rec_vty(nri_ranges, 1, vty_args[a], NULL, 3);
			osmo_nri_ranges_free(nri_ranges);
			nri_ranges = ranges_of(base, state);
			rec_vty(nri_ranges, 2, NULL, vty_args[a], 3);
			osmo_nri_ranges_free(nri_ranges);
			nri_ranges = ranges_of(base, state);
			rec_vty(nri_ranges, 2, "0", vty_args[a], 3);
			osmo_nri_ranges_free(nri_ranges);
		}
		for (size_t p = 0; p < ARRAY_SIZE(pairs); p++) {
			for (size_t b = 0; b < ARRAY_SIZE(vty_bitlens); b++) {
				struct osmo_nri_ranges *nri_ranges = ranges_of(base, state);
				rec_vty(nri_ranges, 2, pairs[p][0], pairs[p][1], vty_bitlens[b]);
				osmo_nri_ranges_free(nri_ranges);
			}
		}
		struct osmo_nri_ranges *nri_ranges = ranges_of(base, state);
		rec_vty(nri_ranges, 1, NULL, NULL, 3);
		rec_vty(nri_ranges, 2, NULL, NULL, 3);
		osmo_nri_ranges_free(nri_ranges);
	}

	static const int16_t wide[][2] = { { 0, 10 } };
	for (size_t a = 0; a < ARRAY_SIZE(vty_args); a++) {
		struct osmo_nri_ranges *nri_ranges = ranges_of(wide, 1);
		rec_vty(nri_ranges, 1, vty_args[a], NULL, -1);
		osmo_nri_ranges_free(nri_ranges);
		nri_ranges = ranges_of(wide, 1);
		rec_vty(nri_ranges, 2, NULL, vty_args[a], -1);
		osmo_nri_ranges_free(nri_ranges);
		nri_ranges = ranges_of(wide, 1);
		rec_vty(nri_ranges, 2, "3", vty_args[a], -1);
		osmo_nri_ranges_free(nri_ranges);
	}
	for (size_t p = 0; p < ARRAY_SIZE(pairs); p++) {
		struct osmo_nri_ranges *nri_ranges = ranges_of(wide, 1);
		rec_vty(nri_ranges, 2, pairs[p][0], pairs[p][1], -1);
		osmo_nri_ranges_free(nri_ranges);
	}
	struct osmo_nri_ranges *nri_ranges = ranges_of(wide, 1);
	rec_vty(nri_ranges, 1, NULL, NULL, -1);
	rec_vty(nri_ranges, 2, NULL, NULL, -1);
	osmo_nri_ranges_free(nri_ranges);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s gsm23236_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
	ctx = talloc_named_const(NULL, 0, "gsm23236_vector");
	for (size_t i = 0; i < SECTION_COUNT; i++) {
		sections[i] = open_memstream(&section_text[i], &section_size[i]);
		CHECK(sections[i]);
	}

	upstream_replay(argv[1]);
	sweep_tmsi();
	sweep_validate();
	sweep_lists();
	sweep_limit();
	sweep_to_str();
	sweep_vty();

	printf("{\"schema\":\"gsm23236-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	for (size_t i = 0; i < SECTION_COUNT; i++) {
		fclose(sections[i]);
		fwrite(section_text[i], 1, section_size[i], stdout);
		free(section_text[i]);
	}
	talloc_free(ctx);
	return EXIT_SUCCESS;
}
