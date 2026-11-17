/*
 * Reference harness for libosmocore's osmo-auc-gen utility. It runs the C program, built
 * from the checkout's utils/osmo-auc-gen.c, once per case and records its exit status,
 * stdout and stderr. It writes NDJSON: a metadata line, then one record per run.
 *
 * The upstream cases are the commands of tests/osmo-auc-gen/osmo-auc-gen_test.sh, in
 * order. The harness rebuilds that script's expected stdout and stderr from its own
 * records and fails if they differ from the given .ok and .err files. The harness cases
 * cover the options, algorithms and errors the script doesn't.
 *
 * Every case passes a RAND, or stops before printing one, and runs twice; the harness
 * fails if the two runs differ.
 *
 * Arguments: the osmo-auc-gen binary, osmo-auc-gen_test.ok and osmo-auc-gen_test.err.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define PROGRAM "osmo-auc-gen"
#define MAX_ARGS 24

/* The upstream script's inputs. */
#define BYTES1 "6a61050765caa32c90371370e5d6dc2d"
#define BYTES2 "1dc4f974325cce611e54f516dc1fec56"
#define BYTES3 "2a48162ff3edca4adf0b7b5e527d6c16"
#define K "EB215756028D60E3275E613320AEC880"
#define OPC "FB2A3D1B360F599ABAB99DB8669F8308"
#define RAND "39fa2f4e3d523d8619a73b4f65c3e14d"
#define AUTS "979498b1f72d3e28c59fa2e72f9c"

/* The upstream AUTS with its last byte changed, so MAC-S fails. */
#define AUTS_BAD "979498b1f72d3e28c59fa2e72f9d"
/* A 32-byte K and TOPc with an AUTS C accepts for them and an all-zero RAND (SQN_MS 31),
 * taken from the inputs of the TUAK harness. */
#define TUAK_K "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
#define TUAK_TOPC "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
#define TUAK_RAND "00000000000000000000000000000000"
#define TUAK_AUTS "98b6db96d11b87b0ea128674dc30"
/* A K, RAND and AUTS C accepts for XOR-3G, taken from the inputs of the auth harness. */
#define XOR_K "ae77de7ee2dc92db38ec776a416b1e55"
#define XOR_RAND "e7fba923e178f6a26895e6969fba626b"
#define XOR_AUTS "51b18293b0dd453e51aaca296479"

#define MILENAGE "-3", "-a", "milenage", "-k", K, "-o", OPC, "-r", RAND
#define TUAK "-3", "-a", "tuak", "-k", TUAK_K, "-o", TUAK_TOPC, "-r", TUAK_RAND
#define GSM(alg) "-2", "-a", alg, "-k", BYTES2, "-r", BYTES1

struct run_case {
	const char *source;
	const char *name;
	const char *args[MAX_ARGS];
};

static const struct run_case cases[] = {
	/* tests/osmo-auc-gen/osmo-auc-gen_test.sh, in order. */
	{ "upstream", "sqn-0", { "-3", "-a", "milenage", "-r", BYTES1, "-k", BYTES2, "-o", BYTES3, "-s", "0" } },
	{ "upstream", "sqn-1", { "-3", "-a", "milenage", "-r", BYTES1, "-k", BYTES2, "-o", BYTES3, "-s", "1" } },
	{ "upstream", "sqn-23", { "-3", "-a", "milenage", "-r", BYTES1, "-k", BYTES2, "-o", BYTES3, "-s", "23" } },
	{ "upstream", "sqn-42", { "-3", "-a", "milenage", "-r", BYTES2, "-k", BYTES3, "-o", BYTES1, "-s", "42" } },
	{ "upstream", "sqn-99", { "-3", "-a", "milenage", "-r", BYTES3, "-k", BYTES1, "-o", BYTES2, "-s", "99" } },
	{ "upstream", "sqn-max", { "-3", "-a", "milenage", "-r", BYTES1, "-k", BYTES3, "-o", BYTES2, "-s", "281474976710655" } },
	{ "upstream", "auts", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS } },
	{ "upstream", "auts-ind-5", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind", "5" } },
	{ "upstream", "auts-ind-23", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind", "23" } },
	{ "upstream", "auts-ind-31", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind", "31" } },
	{ "upstream", "auts-ind-len-0", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind-len", "0" } },
	{ "upstream", "auts-ind-len-1", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind-len", "1" } },
	{ "upstream", "auts-ind-len-1-ind-1", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind-len", "1", "--ind", "1" } },
	{ "upstream", "auts-ind-len-8", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind-len", "8" } },
	{ "upstream", "auts-ind-len-8-ind-1", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind-len", "8", "--ind", "1" } },
	{ "upstream", "auts-ind-minus-1", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind", "-1" } },
	{ "upstream", "auts-ind-32", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind", "32" } },
	{ "upstream", "auts-ind-42", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind", "42" } },
	{ "upstream", "auts-ind-len-0-ind-1", { "-3", "-a", "milenage", "-r", RAND, "-k", K, "-o", OPC, "-A", AUTS, "--ind-len", "0", "--ind", "1" } },

	/* GSM algorithms. */
	{ "harness", "gsm-comp128v1", { GSM("comp128v1") } },
	{ "harness", "gsm-comp128v2", { GSM("comp128v2") } },
	{ "harness", "gsm-comp128v3", { GSM("comp128v3") } },
	{ "harness", "gsm-xor-2g", { GSM("xor-2g") } },
	{ "harness", "gsm-xor-3g", { GSM("xor-3g") } },
	{ "harness", "gsm-name-case", { GSM("COMP128V3") } },
	{ "harness", "gsm-triplets-comp128v1", { GSM("comp128v1"), "-I" } },
	{ "harness", "gsm-triplets-xor-3g", { GSM("xor-3g"), "-I" } },
	{ "harness", "gsm-res-len-ignored", { GSM("comp128v1"), "-L", "4" } },
	{ "harness", "gsm-milenage", { GSM("milenage") } },
	{ "harness", "gsm-tuak", { GSM("tuak") } },
	{ "harness", "gsm-key-spaces", { "-2", "-a", "xor-2g", "-k", "1dc4 f974\t325cce611e54f516dc1fec56", "-r", BYTES1 } },
	{ "harness", "gsm-key-before-type", { "-k", BYTES2, "-2", "-a", "xor-2g", "-r", BYTES1 } },
	{ "harness", "gsm-key-short", { "-2", "-a", "xor-2g", "-k", "1dc4f974325cce611e54f516dc1fec", "-r", BYTES1 } },
	{ "harness", "gsm-key-long", { "-2", "-a", "xor-2g", "-k", BYTES2 "00", "-r", BYTES1 } },
	{ "harness", "gsm-key-odd", { "-2", "-a", "xor-2g", "-k", BYTES2 "0", "-r", BYTES1 } },
	{ "harness", "gsm-key-not-hex", { "-2", "-a", "xor-2g", "-k", "1dc4f974325cce611e54f516dc1fec5g", "-r", BYTES1 } },
	{ "harness", "gsm-opc", { GSM("milenage"), "-o", OPC } },
	{ "harness", "gsm-op", { GSM("milenage"), "-O", OPC } },
	{ "harness", "gsm-auts", { GSM("milenage"), "-A", AUTS } },
	{ "harness", "gsm-amf", { GSM("milenage"), "-f", "8000" } },
	{ "harness", "gsm-sqn", { GSM("milenage"), "-s", "1" } },
	{ "harness", "gsm-ind", { GSM("milenage"), "-i", "1" } },
	{ "harness", "gsm-ind-len", { GSM("milenage"), "-l", "1" } },
	{ "harness", "gsm-type-after-3g", { "-3", "-2", "-a", "xor-2g", "-k", BYTES2, "-r", BYTES1 } },
	{ "harness", "rand-short", { GSM("xor-2g"), "-r", "39fa2f4e3d523d8619a73b4f65c3e1" } },
	{ "harness", "rand-not-hex", { GSM("xor-2g"), "-r", "x9fa2f4e3d523d8619a73b4f65c3e14d" } },

	/* Milenage. */
	{ "harness", "milenage-default-sqn", { MILENAGE } },
	{ "harness", "milenage-op", { "-3", "-a", "milenage", "-k", K, "-O", OPC, "-r", RAND, "-s", "5" } },
	{ "harness", "milenage-amf", { MILENAGE, "-f", "8000", "-s", "5" } },
	{ "harness", "milenage-amf-short", { MILENAGE, "-f", "80" } },
	{ "harness", "milenage-amf-long", { MILENAGE, "-f", "800000" } },
	{ "harness", "milenage-res-len-4", { MILENAGE, "-L", "4" } },
	{ "harness", "milenage-res-len-8", { MILENAGE, "--res-len", "8" } },
	{ "harness", "milenage-res-len-16", { MILENAGE, "-L", "16" } },
	{ "harness", "milenage-res-len-5", { MILENAGE, "-L", "5" } },
	{ "harness", "milenage-res-len-text", { MILENAGE, "-L", "x" } },
	{ "harness", "milenage-res-len-negative", { MILENAGE, "-L", "-4" } },
	{ "harness", "milenage-triplets", { MILENAGE, "-I" } },
	{ "harness", "milenage-sqn-hex", { MILENAGE, "-s", "0x20" } },
	{ "harness", "milenage-sqn-octal", { MILENAGE, "-s", "010" } },
	{ "harness", "milenage-sqn-minus-1", { MILENAGE, "-s", "-1" } },
	{ "harness", "milenage-sqn-2-48", { MILENAGE, "-s", "281474976710656" } },
	{ "harness", "milenage-sqn-u64-max", { MILENAGE, "-s", "18446744073709551615" } },
	{ "harness", "milenage-sqn-overflow", { MILENAGE, "-s", "99999999999999999999" } },
	{ "harness", "milenage-sqn-text", { MILENAGE, "-s", "abc" } },
	{ "harness", "milenage-sqn-trailing", { MILENAGE, "-s", "12abc" } },
	{ "harness", "milenage-sqn-space", { MILENAGE, "-s", " +7" } },
	{ "harness", "milenage-sqn-and-ind", { MILENAGE, "-s", "5", "--ind", "3" } },
	{ "harness", "milenage-ind", { MILENAGE, "--ind", "7" } },
	{ "harness", "milenage-ind-text", { MILENAGE, "--ind", "7x" } },
	{ "harness", "milenage-ind-len-0-sqn", { MILENAGE, "-l", "0", "-s", "7" } },
	{ "harness", "milenage-ind-len-28", { MILENAGE, "-l", "28", "-s", "5" } },
	{ "harness", "milenage-ind-len-29", { MILENAGE, "-l", "29" } },
	{ "harness", "milenage-ind-len-30-ind", { MILENAGE, "-l", "30", "--ind", "1" } },
	{ "harness", "milenage-ind-len-63", { MILENAGE, "-l", "63" } },
	{ "harness", "milenage-3g-resets-ind-len", { "-3", "-l", "3", "-3", "-a", "milenage", "-k", K, "-o", OPC, "-r", RAND, "-s", "9" } },
	{ "harness", "milenage-key-32", { "-3", "-a", "milenage", "-k", TUAK_K, "-o", OPC, "-r", RAND } },
	{ "harness", "milenage-key-15", { "-3", "-a", "milenage", "-k", "EB215756028D60E3275E613320AEC8", "-o", OPC, "-r", RAND } },
	{ "harness", "milenage-opc-32", { "-3", "-a", "milenage", "-k", K, "-o", TUAK_TOPC, "-r", RAND } },
	{ "harness", "milenage-opc-15", { "-3", "-a", "milenage", "-k", K, "-o", "FB2A3D1B360F599ABAB99DB8669F83", "-r", RAND } },
	{ "harness", "milenage-op-15", { "-3", "-a", "milenage", "-k", K, "-O", "FB2A3D1B360F599ABAB99DB8669F83", "-r", RAND } },
	{ "harness", "milenage-no-key", { "-3", "-a", "milenage", "-o", OPC, "-r", RAND } },
	{ "harness", "milenage-no-opc", { "-3", "-a", "milenage", "-k", K, "-r", RAND } },
	{ "harness", "milenage-auts-bad", { MILENAGE, "-A", AUTS_BAD } },
	{ "harness", "milenage-auts-short", { MILENAGE, "-A", "979498b1f72d3e28c59fa2e7" } },
	{ "harness", "milenage-auts-long", { MILENAGE, "-A", AUTS "00" } },
	{ "harness", "milenage-auts-odd", { MILENAGE, "-A", AUTS "0" } },
	{ "harness", "milenage-auts-sqn", { MILENAGE, "-A", AUTS, "-s", "5" } },
	{ "harness", "milenage-auts-res-len-4", { MILENAGE, "-A", AUTS, "-L", "4" } },
	{ "harness", "milenage-auts-triplets", { MILENAGE, "-A", AUTS, "-I" } },
	{ "harness", "milenage-auts-amf", { MILENAGE, "-A", AUTS, "-f", "8000" } },

	/* XOR-3G. */
	{ "harness", "xor-3g", { "-3", "-a", "xor-3g", "-k", K, "-r", RAND } },
	{ "harness", "xor-3g-sqn", { "-3", "-a", "xor-3g", "-k", K, "-r", RAND, "-s", "100" } },
	{ "harness", "xor-3g-res-len-4", { "-3", "-a", "xor-3g", "-k", K, "-r", RAND, "-L", "4" } },
	{ "harness", "xor-3g-key-32", { "-3", "-a", "xor-3g", "-k", TUAK_K, "-r", RAND } },
	{ "harness", "xor-3g-auts", { "-3", "-a", "xor-3g", "-k", XOR_K, "-r", XOR_RAND, "-A", XOR_AUTS } },
	{ "harness", "xor-3g-auts-bad", { "-3", "-a", "xor-3g", "-k", XOR_K, "-r", XOR_RAND, "-A", AUTS } },
	{ "harness", "umts-xor-2g", { "-3", "-a", "xor-2g", "-k", K, "-r", RAND } },

	/* TUAK. */
	{ "harness", "tuak", { TUAK } },
	{ "harness", "tuak-sqn", { TUAK, "-s", "33" } },
	{ "harness", "tuak-res-len-4", { TUAK, "-L", "4" } },
	{ "harness", "tuak-res-len-16", { TUAK, "-L", "16" } },
	{ "harness", "tuak-op", { "-3", "-a", "tuak", "-k", TUAK_K, "-O", TUAK_TOPC, "-r", TUAK_RAND } },
	{ "harness", "tuak-key-16", { "-3", "-a", "tuak", "-k", K, "-o", TUAK_TOPC, "-r", RAND } },
	{ "harness", "tuak-opc-16", { "-3", "-a", "tuak", "-k", TUAK_K, "-o", OPC, "-r", RAND } },
	{ "harness", "tuak-auts", { TUAK, "-A", TUAK_AUTS } },
	{ "harness", "tuak-auts-ind", { TUAK, "-A", TUAK_AUTS, "--ind", "10" } },
	{ "harness", "tuak-auts-res-len-16", { TUAK, "-A", TUAK_AUTS, "-L", "16" } },
	{ "harness", "tuak-auts-bad", { TUAK, "-A", AUTS } },

	/* Option syntax. */
	{ "harness", "long-options", { "--3g", "--algorithm", "milenage", "--key", K, "--opc", OPC, "--rand", RAND, "--sqn", "5", "--amf", "8000" } },
	{ "harness", "long-options-equals", { "--3g", "--algorithm=milenage", "--key=" K, "--op=" OPC, "--rand=" RAND, "--ind-len=4", "--ind=3", "--res-len=4" } },
	{ "harness", "long-auts", { "--3g", "--algorithm", "milenage", "--key", K, "--opc", OPC, "--rand", RAND, "--auts", AUTS } },
	{ "harness", "long-2g", { "--2g", "--algorithm", "xor-2g", "--key", BYTES2, "--rand", BYTES1 } },
	{ "harness", "long-abbreviated", { "--3", "--alg", "milenage", "--ke", K, "--opc", OPC, "--ra", RAND, "--sq", "5", "--res", "4" } },
	{ "harness", "long-ambiguous", { "--3g", "--algorithm", "milenage", "--key", K, "--opc", OPC, "--rand", RAND, "--i", "3" } },
	{ "harness", "short-grouped", { "-3a", "milenage", "-k" K, "-o" OPC, "-r" RAND, "-s5" } },
	{ "harness", "short-grouped-flags", { "-3I", "-a", "milenage", "-k", K, "-o", OPC, "-r", RAND } },
	{ "harness", "options-after-positional", { "extra", MILENAGE } },
	{ "harness", "positional", { MILENAGE, "extra" } },
	{ "harness", "double-dash", { MILENAGE, "--", "-s" } },
	{ "harness", "double-dash-only", { MILENAGE, "--" } },
	{ "harness", "dash", { MILENAGE, "-" } },
	{ "harness", "invalid-option", { MILENAGE, "-x" } },
	{ "harness", "unrecognized-option", { MILENAGE, "--foo" } },
	{ "harness", "ipsec-long", { MILENAGE, "--ipsec" } },
	{ "harness", "missing-argument", { "-3", "-a", "milenage", "-k" } },
	{ "harness", "missing-long-argument", { "-3", "-a", "milenage", "--key" } },
	{ "harness", "flag-with-argument", { "--3g=1", "-a", "milenage" } },
	{ "harness", "help", { "-h" } },
	{ "harness", "help-long", { "--help" } },
	{ "harness", "help-after-options", { MILENAGE, "-h" } },
	{ "harness", "unknown-algorithm", { "-3", "-a", "nosuch", "-k", K } },
	{ "harness", "algorithm-none", { "-3", "-a", "none", "-k", K } },
	{ "harness", "no-arguments", { NULL } },
	{ "harness", "no-type", { "-a", "milenage", "-r", RAND } },
	{ "harness", "no-algorithm", { "-3", "-k", K, "-o", OPC, "-r", RAND } },
	{ "harness", "error-before-help", { "-2", "-o", OPC, "-h" } },
};

struct capture {
	int status;
	char *out;
	char *err;
};

static void fail(const char *what)
{
	fprintf(stderr, "osmo-auc-gen harness: %s\n", what);
	exit(1);
}

static void fail_errno(const char *what)
{
	fprintf(stderr, "osmo-auc-gen harness: %s: %s\n", what, strerror(errno));
	exit(1);
}

struct buffer {
	char *data;
	size_t len, cap;
};

static void append(struct buffer *b, const char *data, size_t len)
{
	if (b->len + len + 1 > b->cap) {
		size_t cap = b->cap ? b->cap : 4096;
		while (cap < b->len + len + 1)
			cap *= 2;
		b->data = realloc(b->data, cap);
		if (!b->data)
			fail("out of memory");
		b->cap = cap;
	}
	memcpy(b->data + b->len, data, len);
	b->len += len;
	b->data[b->len] = '\0';
}

static void append_str(struct buffer *b, const char *s)
{
	append(b, s, strlen(s));
}

static char *finish(struct buffer *b)
{
	if (!b->data)
		append(b, "", 0);
	return b->data;
}

static size_t argument_count(const struct run_case *c)
{
	size_t n = 0;
	while (n < MAX_ARGS && c->args[n])
		n++;
	if (n == MAX_ARGS)
		fail("a case has too many arguments");
	return n;
}

static struct capture run(const char *binary, const struct run_case *c)
{
	int out_pipe[2], err_pipe[2];
	char *argv[MAX_ARGS + 1];
	size_t n = argument_count(c);
	struct buffer out = { 0 }, err = { 0 };
	struct capture result;
	pid_t pid;
	int status;

	argv[0] = (char *)PROGRAM;
	for (size_t i = 0; i < n; i++)
		argv[i + 1] = (char *)c->args[i];
	argv[n + 1] = NULL;

	if (pipe(out_pipe) || pipe(err_pipe))
		fail_errno("pipe");
	fflush(stdout);
	pid = fork();
	if (pid < 0)
		fail_errno("fork");
	if (pid == 0) {
		if (dup2(out_pipe[1], STDOUT_FILENO) < 0 || dup2(err_pipe[1], STDERR_FILENO) < 0)
			_exit(127);
		close(out_pipe[0]);
		close(out_pipe[1]);
		close(err_pipe[0]);
		close(err_pipe[1]);
		execv(binary, argv);
		_exit(127);
	}
	close(out_pipe[1]);
	close(err_pipe[1]);

	struct pollfd fds[2] = {
		{ .fd = out_pipe[0], .events = POLLIN },
		{ .fd = err_pipe[0], .events = POLLIN },
	};
	struct buffer *targets[2] = { &out, &err };
	int open_fds = 2;
	while (open_fds) {
		if (poll(fds, 2, -1) < 0) {
			if (errno == EINTR)
				continue;
			fail_errno("poll");
		}
		for (int i = 0; i < 2; i++) {
			char chunk[4096];
			ssize_t got;

			if (fds[i].fd < 0 || !(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
				continue;
			got = read(fds[i].fd, chunk, sizeof(chunk));
			if (got < 0) {
				if (errno == EINTR)
					continue;
				fail_errno("read");
			}
			if (got == 0) {
				close(fds[i].fd);
				fds[i].fd = -1;
				open_fds--;
				continue;
			}
			if (memchr(chunk, '\0', (size_t)got))
				fail("the program wrote a NUL byte");
			append(targets[i], chunk, (size_t)got);
		}
	}

	if (waitpid(pid, &status, 0) < 0)
		fail_errno("waitpid");
	if (!WIFEXITED(status))
		fail("the program did not exit normally");
	if (WEXITSTATUS(status) == 127)
		fail("the program could not be started");

	result.status = WEXITSTATUS(status);
	result.out = finish(&out);
	result.err = finish(&err);
	return result;
}

static void print_json_string(const char *s)
{
	putchar('"');
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
		switch (*p) {
		case '"':
			fputs("\\\"", stdout);
			break;
		case '\\':
			fputs("\\\\", stdout);
			break;
		case '\n':
			fputs("\\n", stdout);
			break;
		case '\t':
			fputs("\\t", stdout);
			break;
		default:
			if (*p >= 0x80)
				fail("the output is not ASCII");
			if (*p < 0x20)
				printf("\\u%04x", *p);
			else
				putchar(*p);
		}
	}
	putchar('"');
}

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	struct buffer b = { 0 };
	char chunk[4096];
	size_t got;

	if (!f)
		fail_errno(path);
	while ((got = fread(chunk, 1, sizeof(chunk), f)) > 0)
		append(&b, chunk, got);
	if (ferror(f))
		fail_errno(path);
	fclose(f);
	return finish(&b);
}

/* The shell script's echo of a command: "> osmo-auc-gen" and the arguments. */
static void append_command(struct buffer *b, const struct run_case *c)
{
	append_str(b, "> " PROGRAM);
	for (size_t i = 0; c->args[i]; i++) {
		append_str(b, " ");
		append_str(b, c->args[i]);
	}
	append_str(b, "\n");
}

int main(int argc, char **argv)
{
	size_t count = sizeof(cases) / sizeof(cases[0]);
	struct buffer ok = { 0 }, err = { 0 };

	if (argc != 4)
		fail("usage: osmo_auc_gen_vector BINARY OK ERR");

	printf("{\"schema\":\"osmo-auc-gen-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	for (size_t i = 0; i < count; i++) {
		const struct run_case *c = &cases[i];
		struct capture first = run(argv[1], c);
		struct capture second = run(argv[1], c);

		for (size_t j = 0; j < i; j++)
			if (!strcmp(cases[j].name, c->name))
				fail("two cases have the same name");
		if (first.status != second.status || strcmp(first.out, second.out) ||
		    strcmp(first.err, second.err))
			fail("a case gave different results on two runs");

		if (!strcmp(c->source, "upstream")) {
			bool error = first.status != 0;

			if (i > 0 && strcmp(cases[i - 1].source, "upstream"))
				fail("the upstream cases must come first");
			append_str(&ok, "\n\n");
			if (error) {
				append_str(&ok, "expecting error:\n");
				append_str(&err, "expecting error:\n");
				append_command(&err, c);
			}
			append_command(&ok, c);
			append_str(&ok, first.out);
			append_str(&err, first.err);
		}

		printf("{\"kind\":\"run\",\"source\":\"%s\",\"name\":\"%s\",\"args\":[", c->source,
		       c->name);
		for (size_t j = 0; c->args[j]; j++) {
			if (j)
				putchar(',');
			print_json_string(c->args[j]);
		}
		printf("],\"status\":%d,\"stdout\":", first.status);
		print_json_string(first.out);
		fputs(",\"stderr\":", stdout);
		print_json_string(first.err);
		fputs("}\n", stdout);

		free(first.out);
		free(first.err);
		free(second.out);
		free(second.err);
	}

	if (strcmp(finish(&ok), read_file(argv[2])))
		fail("the upstream records don't rebuild osmo-auc-gen_test.ok");
	if (strcmp(finish(&err), read_file(argv[3])))
		fail("the upstream records don't rebuild osmo-auc-gen_test.err");
	return 0;
}
