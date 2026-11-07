/*
 * Reference harness for libosmocore's string ringbuffer.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one record
 * per event on a named ringbuffer. The first argument is upstream's strrb_test.ok. The
 * checks upstream's strrb_test.c makes are replayed, and the run fails if one differs.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/strrb.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define MAX_VALID_RECORDS 40

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

static void print_hex(const char *key, const char *text)
{
	printf("\"%s\":\"", key);
	for (const char *c = text; *c; c++)
		printf("%02x", (unsigned char)*c);
	printf("\"");
}

static void event_start(const char *op, const char *object)
{
	printf("{\"kind\":\"event\",\"source\":\"%s\",\"op\":\"%s\",\"object\":\"%s\",", source, op,
	       object);
}

static void record_nth(const char *object, const struct osmo_strrb *rb, unsigned int index)
{
	const char *text = osmo_strrb_get_nth(rb, index);

	event_start("nth", object);
	printf("\"index\":%u,", index);
	if (text)
		print_hex("text", text);
	else
		printf("\"text\":null");
	printf("}\n");
}

/* The ringbuffer's fields and results, then what each index gives. */
static void record_state(const char *object, const struct osmo_strrb *rb)
{
	unsigned int slots = rb->size + 2u;
	unsigned int indexes = rb->size > 32 ? 8 : 2u * rb->size + 3u;

	if (slots > MAX_VALID_RECORDS)
		slots = MAX_VALID_RECORDS;
	event_start("state", object);
	printf("\"start\":%u,\"end\":%u,\"elements\":%zu,\"empty\":%d,\"valid\":\"", rb->start, rb->end,
	       osmo_strrb_elements(rb), osmo_strrb_is_empty(rb));
	for (unsigned int bufi = 0; bufi < slots; bufi++)
		printf("%d", _osmo_strrb_is_bufindex_valid(rb, bufi));
	printf("\"}\n");

	for (unsigned int index = 0; index < indexes; index++)
		record_nth(object, rb, index);
	/* Near the top of the range, adding the start wraps around. */
	record_nth(object, rb, UINT_MAX);
	record_nth(object, rb, UINT_MAX - 1);
	record_nth(object, rb, UINT_MAX - rb->start + 1);
}

static struct osmo_strrb *create(const char *object, size_t requested)
{
	struct osmo_strrb *rb = osmo_strrb_create(NULL, requested);

	if (!rb)
		fail("osmo_strrb_create");
	event_start("create", object);
	printf("\"requested\":%zu,\"size\":%u}\n", requested, rb->size);
	record_state(object, rb);
	return rb;
}

static int add(const char *object, struct osmo_strrb *rb, const char *text)
{
	int rc = osmo_strrb_add(rb, text);

	event_start("add", object);
	print_hex("text", text);
	printf(",\"rc\":%d}\n", rc);
	record_state(object, rb);
	return rc;
}

/* The checks of upstream's strrb_test.c, on the same ringbuffers. */
static void upstream_checks(const char *ok_path)
{
	static const char *const strings[] = { "hello", "a", "world", "sky", "moon" };
	struct osmo_strrb *rbs[6];
	char line[64];
	FILE *file;

	source = "upstream";
	for (size_t i = 0; i < ARRAY_SIZE(rbs); i++) {
		static const char *const names[] = { "rb0", "rb1", "rb2", "rb3", "rb4", "rb5" };

		rbs[i] = create(names[i], 3);
		for (size_t j = 0; j < i; j++)
			add(names[i], rbs[i], strings[j]);
	}

	CHECK(_osmo_strrb_is_bufindex_valid(rbs[1], 0));
	CHECK(!_osmo_strrb_is_bufindex_valid(rbs[1], 1));
	CHECK(!_osmo_strrb_is_bufindex_valid(rbs[1], 2));
	CHECK(!_osmo_strrb_is_bufindex_valid(rbs[3], 0));
	CHECK(_osmo_strrb_is_bufindex_valid(rbs[3], 1));
	CHECK(_osmo_strrb_is_bufindex_valid(rbs[3], 2));
	CHECK(_osmo_strrb_is_bufindex_valid(rbs[4], 0));
	CHECK(!_osmo_strrb_is_bufindex_valid(rbs[4], 1));
	CHECK(_osmo_strrb_is_bufindex_valid(rbs[4], 2));
	CHECK(_osmo_strrb_is_bufindex_valid(rbs[5], 0));
	CHECK(_osmo_strrb_is_bufindex_valid(rbs[5], 1));
	CHECK(!_osmo_strrb_is_bufindex_valid(rbs[5], 2));

	for (size_t i = 0; i < 3; i++)
		CHECK(osmo_strrb_elements(rbs[i]) == i);
	CHECK(osmo_strrb_elements(rbs[3]) == 2);

	CHECK(!osmo_strrb_get_nth(rbs[0], 0));
	CHECK(!strcmp("hello", osmo_strrb_get_nth(rbs[2], 0)));
	CHECK(!strcmp("a", osmo_strrb_get_nth(rbs[2], 1)));
	CHECK(!strcmp("a", osmo_strrb_get_nth(rbs[3], 0)));
	CHECK(!strcmp("world", osmo_strrb_get_nth(rbs[3], 1)));
	CHECK(!osmo_strrb_get_nth(rbs[3], 2));
	CHECK(!strcmp("world", osmo_strrb_get_nth(rbs[4], 0)));
	CHECK(!strcmp("sky", osmo_strrb_get_nth(rbs[4], 1)));
	CHECK(!strcmp("sky", osmo_strrb_get_nth(rbs[5], 0)));
	CHECK(!strcmp("moon", osmo_strrb_get_nth(rbs[5], 1)));
	for (size_t i = 0; i < ARRAY_SIZE(rbs); i++)
		talloc_free(rbs[i]);

	/* test_add: the start and end after each string. */
	static const struct {
		const char *text;
		unsigned int start, end;
	} steps[] = {
		{ "a", 0, 1 }, { "b", 0, 2 }, { "c", 0, 3 }, { "d", 1, 0 },
		{ "e", 2, 1 }, { "f", 3, 2 }, { "g", 0, 3 }, { "h", 1, 0 },
	};
	struct osmo_strrb *rb = create("add", 4);

	CHECK(rb->start == 0 && rb->end == 0);
	for (size_t i = 0; i < ARRAY_SIZE(steps); i++) {
		add("add", rb, steps[i].text);
		CHECK(rb->start == steps[i].start && rb->end == steps[i].end);
		if (i >= 2) {
			CHECK(osmo_strrb_elements(rb) == 3);
			for (unsigned int n = 0; n < 3; n++)
				CHECK(!strcmp(steps[i - 2 + n].text, osmo_strrb_get_nth(rb, n)));
		}
	}
	talloc_free(rb);

	/* test_long_msg */
	char long_text[RB_MAX_MESSAGE_SIZE + 7];
	const char *content;
	int i;

	for (i = 0; i < RB_MAX_MESSAGE_SIZE; i += 2) {
		long_text[i] = 'a';
		long_text[i + 1] = 'b';
	}
	long_text[i] = 0;
	rb = create("long_msg", 2);
	CHECK(add("long_msg", rb, long_text) == 1);
	content = osmo_strrb_get_nth(rb, 0);
	CHECK(!strncmp(long_text, content, RB_MAX_MESSAGE_SIZE - 1));
	CHECK(!content[RB_MAX_MESSAGE_SIZE - 1]);
	CHECK(strlen(content) == RB_MAX_MESSAGE_SIZE - 1);
	talloc_free(rb);

	file = fopen(ok_path, "r");
	if (!file) {
		perror(ok_path);
		exit(EXIT_FAILURE);
	}
	if (!fgets(line, sizeof(line), file) || strcmp(line, "All tests passed\n"))
		fail("upstream output");
	fclose(file);
	source = "sweep";
}

static void lengths(void)
{
	static const size_t sizes[] = { 0, 1, 2, 238, 239, 240, 241, 1000 };
	static char text[1001];
	struct osmo_strrb *rb = create("lengths", 3);

	for (size_t i = 0; i < ARRAY_SIZE(sizes); i++) {
		memset(text, (int)('a' + i), sizes[i]);
		text[sizes[i]] = 0;
		add("lengths", rb, text);
	}
	talloc_free(rb);
}

/* The field is 16 bits, so larger sizes truncate. No string goes in at size 0: C crashes. */
static void sizes(void)
{
	static const struct {
		const char *name;
		size_t requested;
	} cases[] = {
		{ "size_0", 0 }, { "size_65535", 65535 }, { "size_65536", 65536 },
		{ "size_65537", 65537 }, { "size_65539", 65539 },
	};

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		struct osmo_strrb *rb = create(cases[i].name, cases[i].requested);

		if (rb->size) {
			add(cases[i].name, rb, "one");
			add(cases[i].name, rb, "two");
		}
		talloc_free(rb);
	}
}

static void random_runs(void)
{
	static const struct {
		const char *name;
		size_t size;
	} runs[] = {
		{ "random_1", 1 }, { "random_2", 2 }, { "random_3", 3 }, { "random_4", 4 },
		{ "random_5", 5 }, { "random_8", 8 }, { "random_16", 16 },
	};
	uint32_t state = 0x57bb;

	for (size_t i = 0; i < ARRAY_SIZE(runs); i++) {
		struct osmo_strrb *rb = create(runs[i].name, runs[i].size);

		for (size_t step = 0; step < 3 * runs[i].size + 3; step++) {
			char text[32];
			size_t length = next_random(&state) % sizeof(text);

			for (size_t j = 0; j < length; j++)
				text[j] = (char)(next_random(&state) % 255 + 1);
			text[length] = 0;
			add(runs[i].name, rb, text);
		}
		talloc_free(rb);
	}
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s strrb_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}

	printf("{\"schema\":\"strrb-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	printf("{\"kind\":\"constant\",\"name\":\"RB_MAX_MESSAGE_SIZE\",\"value\":%d}\n", RB_MAX_MESSAGE_SIZE);
	upstream_checks(argv[1]);
	lengths();
	sizes();
	random_runs();
	return EXIT_SUCCESS;
}
