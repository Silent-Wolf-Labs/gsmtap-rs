/*
 * Reference harness for libosmocore's use_count.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one record
 * per event. A call to osmo_use_count_get_put() gives a "call" record, a "callback"
 * record for each callback it runs, and a "result" record, so nested calls show up in
 * order. The first argument is upstream's use_count_test.err. The harness runs the
 * sequence of upstream's use_count_test.c, with its FSM reduced to the two states the
 * test goes through, and the run fails if a use count line differs.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/use_count.h>
#include <osmocom/core/utils.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define GUARD 0xa5
#define TEXT_SIZE 1024

enum policy {
	POLICY_NONE, /* No callback. */
	POLICY_OK, /* Returns 0. */
	POLICY_FAIL, /* Returns -5. */
	POLICY_CLAMP, /* Above 1, sets the count back to 1 and returns -ERANGE. */
	POLICY_FREE_ZERO, /* Frees an entry that reaches 0. */
	POLICY_FOO, /* What upstream's test does. */
};

static const char *const policy_names[] = { "none", "ok", "fail", "clamp", "free_zero", "foo" };

struct object {
	const char *name;
	enum policy policy;
	struct osmo_use_count uc;
	struct osmo_use_count_entry statics[32];
	size_t statics_used;
	bool releasing;
	bool released;
};

static const char *source = "sweep";

/* Lines upstream's test prints about use counts, in order. */
static char replay_lines[256][256];
static unsigned int replay_count;

static void fail(const char *what)
{
	fprintf(stderr, "reference expectation failed: %s\n", what);
	exit(EXIT_FAILURE);
}

static void replay_line(const char *line)
{
	if (replay_count == ARRAY_SIZE(replay_lines))
		fail("too many replay lines");
	snprintf(replay_lines[replay_count++], sizeof(replay_lines[0]), "%s", line);
}

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

static void print_use(const char *use)
{
	if (use)
		print_hex("use", use);
	else
		printf("\"use\":null");
}

static const char *state_text(const struct osmo_use_count *uc)
{
	static char text[TEXT_SIZE];

	if (osmo_use_count_name_buf(text, sizeof(text), uc) != text)
		fail("osmo_use_count_name_buf returned another buffer");
	return text;
}

/* Every entry in list order, including idle ones. A NULL token shows as <null>. */
static const char *entries_text(const struct osmo_use_count *uc)
{
	static char text[TEXT_SIZE];
	const struct osmo_use_count_entry *e;
	size_t used = 0;

	text[0] = 0;
	if (!uc->use_counts.next)
		return text;
	llist_for_each_entry(e, &uc->use_counts, entry)
		used += (size_t)snprintf(text + used, sizeof(text) - used, "%s=%" PRId32 ";",
					 e->use ? e->use : "<null>", e->count);
	return text;
}

static void print_state(const struct object *object)
{
	char *talloced = osmo_use_count_to_str_c(NULL, &object->uc);

	if (strcmp(talloced, state_text(&object->uc)))
		fail("osmo_use_count_to_str_c differs");
	talloc_free(talloced);
	printf("\"total\":%" PRId32 ",", osmo_use_count_total(&object->uc));
	print_hex("text", state_text(&object->uc));
	printf(",");
	print_hex("entries", entries_text(&object->uc));
}

static void event_start(const char *op, const struct object *object)
{
	printf("{\"kind\":\"event\",\"source\":\"%s\",\"op\":\"%s\",\"object\":\"%s\",", source, op,
	       object->name);
}

static int get_put(struct object *object, const char *use, int32_t change);

static int foo_callback(struct object *object, struct osmo_use_count_entry *e, int32_t old)
{
	const char *use = e->use;
	int32_t count = e->count;
	char line[256];

	if (use && (!strcmp(use, "barring") || !strcmp(use, "releasing")) && count > 1) {
		snprintf(line, sizeof(line), "Attempt to get more than one %s", use);
		replay_line(line);
		e->count = 1;
		return -ERANGE;
	}
	snprintf(line, sizeof(line), "%s %+d %s: now used by %s", object->name, count - old,
		 use ? use : "NULL", state_text(&object->uc));
	replay_line(line);
	if (count < 0) {
		snprintf(line, sizeof(line), "Negative use count on %s: %s", use ? use : "NULL",
			 state_text(&object->uc));
		replay_line(line);
	}
	if (osmo_use_count_total(&object->uc) == 0) {
		if (object->releasing) {
			object->released = true;
		} else {
			object->releasing = true;
			int rc = get_put(object, "releasing", 1);

			if (rc)
				fail("getting the releasing use");
		}
	}
	return 0;
}

static int callback(struct osmo_use_count_entry *e, int32_t old, const char *file, int line)
{
	struct object *object = container_of(e->use_count, struct object, uc);

	(void)file;
	(void)line;
	event_start("callback", object);
	print_use(e->use);
	printf(",\"old\":%" PRId32 ",\"new\":%" PRId32 ",", old, e->count);
	print_state(object);
	printf("}\n");

	switch (object->policy) {
	case POLICY_FAIL:
		return -5;
	case POLICY_CLAMP:
		if (e->count > 1) {
			e->count = 1;
			return -ERANGE;
		}
		return 0;
	case POLICY_FREE_ZERO:
		if (!e->count)
			osmo_use_count_free(e);
		return 0;
	case POLICY_FOO:
		return foo_callback(object, e, old);
	default:
		return 0;
	}
}

static void object_new(struct object *object, const char *name, enum policy policy)
{
	memset(object, 0, sizeof(*object));
	object->name = name;
	object->policy = policy;
	object->uc.use_cb = policy == POLICY_NONE ? NULL : callback;
	event_start("new", object);
	printf("\"policy\":\"%s\",", policy_names[policy]);
	print_state(object);
	printf("}\n");
}

static int get_put(struct object *object, const char *use, int32_t change)
{
	int rc;

	event_start("call", object);
	print_use(use);
	printf(",\"change\":%" PRId32 "}\n", change);
	rc = osmo_use_count_get_put(&object->uc, use, change);
	event_start("result", object);
	printf("\"rc\":%d,\"by\":%" PRId32 ",", rc, osmo_use_count_by(&object->uc, use));
	print_state(object);
	printf("}\n");
	return rc;
}

static void add_statics(struct object *object, size_t count)
{
	if (object->statics_used + count > ARRAY_SIZE(object->statics))
		fail("too many static entries");
	osmo_use_count_make_static_entries(&object->uc, object->statics + object->statics_used, count);
	object->statics_used += count;
	event_start("static", object);
	printf("\"n\":%zu,", count);
	print_state(object);
	printf("}\n");
}

static void free_entry(struct object *object, const char *use)
{
	osmo_use_count_free(osmo_use_count_find(&object->uc, use));
	event_start("free", object);
	print_use(use);
	printf(",");
	print_state(object);
	printf("}\n");
}

static void to_str(const struct object *object, size_t n)
{
	static char buf[TEXT_SIZE];
	int needed;

	memset(buf, GUARD, sizeof(buf));
	needed = osmo_use_count_to_str_buf(buf, n, &object->uc);
	for (size_t i = n; i < sizeof(buf); i++)
		if ((unsigned char)buf[i] != GUARD)
			fail("osmo_use_count_to_str_buf wrote past its size");
	if (n && strnlen(buf, n) == n)
		fail("osmo_use_count_to_str_buf left no terminator");
	event_start("to_str", object);
	printf("\"n\":%zu,\"rc\":%d,", n, needed);
	if (n)
		print_hex("text", buf);
	else
		printf("\"text\":\"\"");
	printf("}\n");
}

/* Upstream's foo_get_put() macro. */
static void foo_get_put(struct object *foo, const char *use, int32_t change)
{
	int rc = get_put(foo, use, change);
	char line[256];

	if (!rc)
		return;
	snprintf(line, sizeof(line), "osmo_use_count_get_put(%s, %s, %" PRId32 ") returned error: %d %s",
		 foo->name, use ? use : "NULL", change, rc, strerror(-rc));
	replay_line(line);
}

static void print_foos(struct object *foos, size_t count)
{
	char line[256];
	int listed = 0;

	for (size_t i = 0; i < count; i++) {
		if (foos[i].released)
			continue;
		snprintf(line, sizeof(line), "%s: %s", foos[i].name, state_text(&foos[i].uc));
		replay_line(line);
		listed++;
	}
	snprintf(line, sizeof(line), "%d foos", listed);
	replay_line(line);
}

static void upstream_sequence(void)
{
	static struct object foos[3];
	struct object *a = &foos[0], *b = &foos[1], *c = &foos[2];

	source = "upstream";
	object_new(a, "a", POLICY_FOO);
	add_statics(a, 0);
	object_new(b, "b", POLICY_FOO);
	add_statics(b, 2);
	object_new(c, "c", POLICY_FOO);
	add_statics(c, 10);
	print_foos(foos, 3);

	foo_get_put(a, "barring", 1);
	foo_get_put(b, "barring", 1);
	foo_get_put(b, "fighting", 1);
	print_foos(foos, 3);

	foo_get_put(b, "barring", 1);
	print_foos(foos, 3);

	foo_get_put(b, "barring", -1);
	print_foos(foos, 3);

	foo_get_put(a, NULL, 1);
	print_foos(foos, 3);
	foo_get_put(a, NULL, -1);
	print_foos(foos, 3);

	foo_get_put(a, "barring", -1);
	print_foos(foos, 3);

	foo_get_put(a, "releasing", -1);
	print_foos(foos, 3);

	foo_get_put(b, "kungfoo", 1);
	foo_get_put(b, "kungfoo", 1);
	foo_get_put(b, "kungfoo", -1);
	foo_get_put(b, "kungfoo", 1);
	foo_get_put(b, "kungfoo", 1);
	print_foos(foos, 3);

	foo_get_put(b, "kungfoo", INT32_MAX - 1 - osmo_use_count_by(&b->uc, "kungfoo"));
	print_foos(foos, 3);
	foo_get_put(b, "kungfoo", 1);
	foo_get_put(b, "kungfoo", 1);
	foo_get_put(b, "kungfoo", 1);
	foo_get_put(b, "fighting", 2);
	foo_get_put(b, "kungfoo", -3);
	foo_get_put(b, "kungfoo", -1);
	foo_get_put(b, "kungfoo", -1);
	foo_get_put(b, "fighting", 1);
	foo_get_put(b, "fighting", 1);
	foo_get_put(b, "fighting", 1);
	print_foos(foos, 3);

	foo_get_put(b, "kungfoo", -osmo_use_count_by(&b->uc, "kungfoo"));
	foo_get_put(b, "fighting", -osmo_use_count_by(&b->uc, "fighting"));

	foo_get_put(b, "releasing", -1);
	print_foos(foos, 3);

	foo_get_put(c, "kungfoo", -1);
	print_foos(foos, 3);
	foo_get_put(c, "kungfoo", -1);
	foo_get_put(c, "kungfoo", -1);
	print_foos(foos, 3);

	foo_get_put(c, "kungfoo", 4);
	foo_get_put(c, "kungfoo", -1);
	foo_get_put(c, "releasing", -1);
	print_foos(foos, 3);
	source = "sweep";
}

static bool is_use_count_line(const char *line)
{
	unsigned int count;
	char rest[8];

	return !strncmp(line, "a: ", 3) || !strncmp(line, "b: ", 3) || !strncmp(line, "c: ", 3) ||
	       sscanf(line, "%u foos%7s", &count, rest) == 1 || strstr(line, "now used by") ||
	       strstr(line, "Negative use count on") || strstr(line, "}: Attempt to get more than one") ||
	       !strncmp(line, "osmo_use_count_get_put(", 23);
}

static void replay_upstream_output(const char *err_path)
{
	FILE *file = fopen(err_path, "r");
	char line[512];
	unsigned int seen = 0;

	if (!file) {
		perror(err_path);
		exit(EXIT_FAILURE);
	}
	while (fgets(line, sizeof(line), file)) {
		if (!is_use_count_line(line))
			continue;
		if (seen >= replay_count || !strstr(line, replay_lines[seen]))
			fail(line);
		seen++;
	}
	fclose(file);
	if (seen != replay_count)
		fail("harness has more use count lines than upstream");
}

static void lifecycle(void)
{
	static struct object object;

	/* Before any entry exists, the text has no "-". */
	object_new(&object, "fresh", POLICY_NONE);
	if (get_put(&object, "a", 0))
		fail("a zero change returned an error");
	to_str(&object, TEXT_SIZE);
	get_put(&object, "a", 1);
	get_put(&object, "a", -1);
	free_entry(&object, "a");
	free_entry(&object, "absent");

	/* Adding no static entries still counts as having a list. */
	object_new(&object, "no_statics", POLICY_NONE);
	add_statics(&object, 0);

	/* A token at zero keeps its place until it's used, then moves to the end. */
	object_new(&object, "order", POLICY_NONE);
	get_put(&object, "a", 1);
	get_put(&object, "b", 1);
	get_put(&object, "c", 1);
	get_put(&object, "b", -1);
	get_put(&object, "a", -1);
	get_put(&object, "a", 1);
	get_put(&object, "d", 1);
	get_put(&object, "b", 1);
	get_put(&object, "e", 2);
	get_put(&object, "a", -5);

	/* NULL tokens match idle static entries, which also take new tokens. */
	object_new(&object, "statics", POLICY_NONE);
	add_statics(&object, 3);
	get_put(&object, "a", 1);
	get_put(&object, NULL, 1);
	get_put(&object, "b", 1);
	get_put(&object, "c", 1);
	get_put(&object, "", 1);
	get_put(&object, "NULL", 1);
	add_statics(&object, 1);
	get_put(&object, "a", 0);

	/* The limits of one count, and of the total. */
	object_new(&object, "limits", POLICY_NONE);
	get_put(&object, "a", INT32_MAX);
	get_put(&object, "a", 1);
	get_put(&object, "b", 1);
	get_put(&object, "a", -INT32_MAX);
	get_put(&object, "a", -INT32_MAX);
	get_put(&object, "a", -2);
	get_put(&object, "a", 1);
	get_put(&object, "c", -INT32_MAX);
	get_put(&object, "d", -INT32_MAX);

	/* One count can reach INT32_MIN if no negative total comes before it. */
	object_new(&object, "minimum", POLICY_NONE);
	get_put(&object, "a", INT32_MIN);
	get_put(&object, "a", -1);
	get_put(&object, "a", 1);
	get_put(&object, "b", INT32_MAX);

	/* Sizes around the text's length, with a token at each kind of count. */
	object_new(&object, "sizes", POLICY_NONE);
	get_put(&object, "a", 1);
	get_put(&object, "b", 3);
	get_put(&object, "c", -2);
	get_put(&object, NULL, 1);
	for (size_t n = 0; n <= strlen(state_text(&object.uc)) + 2; n++)
		to_str(&object, n);
}

static void random_run(const char *name, enum policy policy, size_t statics, bool frees, uint32_t seed)
{
	static struct object object;
	static const char *const tokens[] = { "a", "b", "c", "", "NULL", NULL };
	static const int32_t changes[] = {
		1, 1, 1, -1, -1, -1, 2, -2, 3, 0, INT32_MAX, INT32_MAX - 1, -INT32_MAX, -(INT32_MAX - 1),
	};
	uint32_t state = seed;

	object_new(&object, name, policy);
	if (statics)
		add_statics(&object, statics);
	for (unsigned int i = 0; i < 150; i++) {
		const char *use = tokens[next_random(&state) % ARRAY_SIZE(tokens)];
		uint32_t choice = next_random(&state) % 20;

		if (choice == 0 && frees) {
			free_entry(&object, use);
		} else if (choice == 1) {
			to_str(&object, next_random(&state) % 24);
		} else if (choice == 2 && !frees) {
			if (object.statics_used < ARRAY_SIZE(object.statics))
				add_statics(&object, 1);
		} else {
			int32_t change = changes[next_random(&state) % ARRAY_SIZE(changes)];
			int64_t sum = (int64_t)osmo_use_count_by(&object.uc, use) + change;

			/*
			 * C negates a change and adds counts in ways that overflow when a total is
			 * negative and a count is INT32_MIN, so no count gets there.
			 */
			if (sum != INT32_MIN)
				get_put(&object, use, change);
		}
	}
}

static void random_runs(void)
{
	random_run("random_none", POLICY_NONE, 0, true, 0x1a2b);
	random_run("random_none_statics", POLICY_NONE, 2, false, 0x2b3c);
	random_run("random_ok", POLICY_OK, 0, true, 0x3c4d);
	random_run("random_fail", POLICY_FAIL, 1, false, 0x4d5e);
	random_run("random_clamp", POLICY_CLAMP, 0, true, 0x5e6f);
	random_run("random_free_zero", POLICY_FREE_ZERO, 0, true, 0x6f70);
}

static void no_object(void)
{
	printf("{\"kind\":\"no_object\",\"rc\":%d,\"total\":%" PRId32 ",\"by\":%" PRId32 "}\n",
	       osmo_use_count_get_put(NULL, "a", 1), osmo_use_count_total(NULL),
	       osmo_use_count_by(NULL, "a"));
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s use_count_test.err\n", argv[0]);
		return EXIT_FAILURE;
	}

	printf("{\"schema\":\"use-count-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	upstream_sequence();
	replay_upstream_output(argv[1]);
	lifecycle();
	random_runs();
	no_object();
	return EXIT_SUCCESS;
}
