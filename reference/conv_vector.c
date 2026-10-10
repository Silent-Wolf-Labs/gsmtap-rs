#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osmocom/core/conv.h>

/* Including the unchanged source exposes only the backend-selection hook. */
#ifdef CONV_ISOLATED
#include "conv_acc.c"
#else
int osmo_conv_decode_acc(const struct osmo_conv_code *, const sbit_t *, ubit_t *);
#endif
#include "conv_upstream.h"

#define CAP 8192
#define MAX_AE 0x00ffffff
static const char *backend;
static const char *case_id;
static const char *code_id;
static int sequence;
static uint8_t outputs[512][2], states[512][2], term_outputs[512], term_states[512];
static int punctures[CAP];
static uint8_t history_valid[CAP * 512];

static int common_evidence(void)
{
#ifdef CONV_AUDIT
	return 1;
#else
	return !strcmp(backend, "linked-dispatch");
#endif
}

static void hex(const void *data, int n)
{
	const uint8_t *p = data;
	putchar('"');
	for (int i = 0; i < n; i++) printf("%02x", p[i]);
	putchar('"');
}

static void start(const char *api)
{
	printf("{\"type\":\"op\",\"case\":\"%s\",\"code\":\"%s\",\"seq\":%d,\"api\":\"%s\",\"backend\":\"%s\"",
	       case_id, code_id, sequence++, api, backend);
}

static void buffers(const void *in, const void *after, int ni, const uint8_t *before,
		    const uint8_t *out, int no, int ret)
{
	printf(",\"input_before\":"); hex(in, ni);
	printf(",\"input_after\":"); hex(after, ni);
	printf(",\"output_before\":"); hex(before, no);
	printf(",\"output_after\":"); hex(out, no);
	printf(",\"ret\":%d", ret);
}

static void descriptor(const char *id, const struct osmo_conv_code *c)
{
	int ns = 1 << (c->K - 1);
	code_id = id;
	printf("{\"type\":\"code\",\"id\":\"%s\",\"backend\":\"%s\",\"n\":%d,\"k\":%d,\"len\":%d,\"term\":%d,\"next_output\":",
	       id, backend, c->N, c->K, c->len, (int)c->term);
	hex(c->next_output, ns * 2);
	printf(",\"next_state\":"); hex(c->next_state, ns * 2);
	printf(",\"next_term_output\":"); hex(c->next_term_output, c->next_term_output ? ns : 0);
	printf(",\"next_term_state\":"); hex(c->next_term_state, c->next_term_state ? ns : 0);
	printf(",\"puncture_present\":%d,\"puncture\":\"", c->puncture != NULL);
	if (c->puncture) for (int i = 0; c->puncture[i] >= 0; i++)
		printf("%s%d", i ? ";" : "", c->puncture[i]);
	puts("\"}");
}

static void encoder_state(const struct osmo_conv_encoder *e)
{
	printf(",\"state\":%u,\"i_idx\":%d,\"p_idx\":%d}\n", e->state, e->i_idx, e->p_idx);
}

static void decoder_state(const struct osmo_conv_decoder *d, int next_valid)
{
	printf(",\"len\":%d,\"n_states\":%d,\"o_idx\":%d,\"p_idx\":%d,\"ae\":\"",
	       d->len, d->n_states, d->o_idx, d->p_idx);
	for (int i = 0; i < d->n_states; i++) printf("%s%u", i ? ";" : "", d->ae[i]);
	printf("\",\"ae_next_valid\":%d,\"ae_next\":\"", next_valid);
	if (next_valid) for (int i = 0; i < d->n_states; i++)
		printf("%s%u", i ? ";" : "", d->ae_next[i]);
	printf("\",\"history\":\"");
	for (int i = 0; i < d->o_idx * d->n_states; i++)
		printf("%s%d", i ? ";" : "", history_valid[i] ? d->state_history[i] : -1);
	puts("\"}");
}

/* Validity only: no metrics or expected decoder outputs are computed here.
 * In this bounded domain every reachable candidate remains below MAX_AE. */
static void mark_history(struct osmo_conv_decoder *d, int count, int flush)
{
	uint8_t reachable[512], next[512];
	for (int s = 0; s < d->n_states; s++) reachable[s] = d->ae[s] < MAX_AE;
	for (int i = 0; i < count; i++) {
		memset(next, 0, sizeof(next));
		for (int s = 0; s < d->n_states; s++) if (reachable[s]) {
			if (flush) next[d->code->next_term_state ? d->code->next_term_state[s] : d->code->next_state[s][0]] = 1;
			else for (int b = 0; b < 2; b++) next[d->code->next_state[s][b]] = 1;
		}
		memcpy(&history_valid[(d->o_idx + i) * d->n_states], next, d->n_states);
		memcpy(reachable, next, sizeof(next));
	}
}

static int valid_trace(const struct osmo_conv_decoder *d, int end)
{
	if (end < 0 || end >= d->n_states || d->o_idx < 1) return 0;
	for (int i = d->o_idx - 1; i >= 0; i--) {
		int p = i * d->n_states + end;
		if (!history_valid[p]) return 0;
		end = d->state_history[p];
	}
	return 1;
}

static void generic(const struct osmo_conv_code *c, sbit_t *soft, int count)
{
	struct osmo_conv_decoder d;
	uint8_t out[CAP], before[CAP];
	sbit_t copy[CAP];
	int split = c->len / 2, used = 0, next_valid = 0;
	memcpy(copy, soft, count);
	osmo_conv_decode_init(&d, c, 0, 0);
	memset(history_valid, 0, sizeof(history_valid));
	start("decode_init"); printf(",\"length_arg\":0,\"start_state\":0"); decoder_state(&d, 0);
	/* Reset to all states for safe short/tie/tail-biting evidence. */
	osmo_conv_decode_reset(&d, -1);
	start("decode_reset"); printf(",\"start_state\":-1"); decoder_state(&d, 0);
	/* A nonpositive override and a nonzero fixed start are separately observed. */
	osmo_conv_decode_deinit(&d);
	start("decode_deinit"); printf(",\"len\":%d,\"n_states\":%d,\"o_idx\":%d,\"p_idx\":%d,\"storage_released\":1}\n",
				       d.len, d.n_states, d.o_idx, d.p_idx);
	osmo_conv_decode_init(&d, c, c->len + 1, 0);
	start("decode_init"); printf(",\"length_arg\":%d,\"start_state\":0", c->len + 1); decoder_state(&d, 0);
	/* An out-of-range reset selector gives all MAX_AE. Non-tail auto output
	 * returns -1 before touching history: this is not unreachable traceback. */
	if (c->term != CONV_TERM_FLUSH && c->term != CONV_TERM_TAIL_BITING) {
		osmo_conv_decode_reset(&d, d.n_states);
		start("decode_reset"); printf(",\"start_state\":%d", d.n_states); decoder_state(&d, 0);
		int end = osmo_conv_decode_get_best_end_state(&d);
		start("decode_get_best_end_state"); printf(",\"ret\":%d", end); decoder_state(&d, 0);
		memset(out, 0xa5, c->len + 4); memcpy(before, out, c->len + 4);
		int ret = osmo_conv_decode_get_output(&d, out, 0, -1);
		start("decode_get_output"); printf(",\"has_flush\":0,\"end_state\":-1");
		buffers(NULL, NULL, 0, before, out, c->len + 4, ret); decoder_state(&d, 0);
	}
	osmo_conv_decode_reset(&d, (1 << (c->K - 1)) - 1);
	start("decode_reset"); printf(",\"start_state\":%d", d.n_states - 1); decoder_state(&d, 0);
	osmo_conv_decode_reset(&d, -1);
	start("decode_reset"); printf(",\"start_state\":-1"); decoder_state(&d, 0);
	for (int pass = 0; pass < 2; pass++) {
		if (pass) {
			osmo_conv_decode_rewind(&d);
			start("decode_rewind"); decoder_state(&d, next_valid);
			used = 0;
		}
		for (int part = 0; part < 2; part++) {
			int n = part ? c->len - split : split;
			mark_history(&d, n, 0);
			int ret = osmo_conv_decode_scan(&d, soft + used, n);
			start("decode_scan"); printf(",\"n\":%d", n);
			buffers(copy + used, soft + used, ret, NULL, NULL, 0, ret);
			used += ret; next_valid |= n > 0; decoder_state(&d, next_valid);
		}
		if (c->term == CONV_TERM_FLUSH) {
			mark_history(&d, c->K - 1, 1);
			int ret = osmo_conv_decode_flush(&d, soft + used);
			start("decode_flush"); buffers(copy + used, soft + used, ret, NULL, NULL, 0, ret);
			used += ret; next_valid = 1; decoder_state(&d, 1);
		}
		assert(used == count);
		/* Tail closure inspects every final state, so require valid paths. */
		if (c->term == CONV_TERM_TAIL_BITING)
			for (int s = 0; s < d.n_states; s++) assert(valid_trace(&d, s));
		int end = osmo_conv_decode_get_best_end_state(&d);
		start("decode_get_best_end_state"); printf(",\"ret\":%d", end); decoder_state(&d, next_valid);
		for (int forced = 0; forced < 3; forced++) {
			int selected = forced == 2 ? d.n_states - 1 : forced ? 0 : end;
			if (!valid_trace(&d, selected)) continue;
			memset(out, 0xa5, c->len + 4); memcpy(before, out, c->len + 4);
			int ret = osmo_conv_decode_get_output(&d, out, c->term == CONV_TERM_FLUSH, forced ? selected : -1);
			start("decode_get_output");
			printf(",\"has_flush\":%d,\"end_state\":%d", c->term == CONV_TERM_FLUSH, forced ? selected : -1);
			buffers(NULL, NULL, 0, before, out, c->len + 4, ret); decoder_state(&d, next_valid);
		}
		/* A flushed frame cannot be rewound and scanned with data-only layout. */
		if (c->term == CONV_TERM_FLUSH) {
			osmo_conv_decode_reset(&d, 0);
			memset(history_valid, 0, sizeof(history_valid));
			start("decode_reset"); printf(",\"start_state\":0"); decoder_state(&d, next_valid);
		}
	}
	osmo_conv_decode_deinit(&d);
	start("decode_deinit"); printf(",\"len\":%d,\"n_states\":%d,\"o_idx\":%d,\"p_idx\":%d,\"storage_released\":1}\n",
				       d.len, d.n_states, d.o_idx, d.p_idx);
}

static void run_case(const struct osmo_conv_code *c, const char *id, uint8_t *in, int soft_mode, int detailed)
{
	uint8_t encoded[CAP], out[CAP], before[CAP], copy[CAP];
	sbit_t soft[CAP], soft_copy[CAP];
	case_id = id; sequence = 0;
	memcpy(copy, in, CAP);
	memset(encoded, 0xa5, sizeof(encoded));
	int count = osmo_conv_encode(c, in, encoded);
	assert(count >= 0 && count + 4 < CAP);
	if (common_evidence()) {
		for (int arg = -1; arg <= 1; arg++) {
			start("get_input_length"); printf(",\"length_arg\":%d,\"ret\":%d}\n", arg, osmo_conv_get_input_length(c, arg));
			start("get_output_length"); printf(",\"length_arg\":%d,\"ret\":%d}\n", arg, osmo_conv_get_output_length(c, arg));
		}
		memset(before, 0xa5, count + 4);
		start("encode"); buffers(copy, in, c->len, before, encoded, count + 4, count); puts("}");
		if (detailed) {
			struct osmo_conv_encoder e;
			osmo_conv_encode_init(&e, c); start("encode_init"); encoder_state(&e);
			osmo_conv_encode_load_state(&e, in); start("encode_load_state");
			buffers(copy, in, c->K - 1, NULL, NULL, 0, 0); encoder_state(&e);
			osmo_conv_encode_init(&e, c); start("encode_init"); encoder_state(&e);
			if (c->term == CONV_TERM_TAIL_BITING) {
				osmo_conv_encode_load_state(&e, in + c->len - c->K + 1);
				start("encode_load_state"); buffers(copy + c->len - c->K + 1, in + c->len - c->K + 1, c->K - 1, NULL, NULL, 0, 0); encoder_state(&e);
			}
			int split = c->len / 2;
			for (int part = 0; part < 2; part++) {
				int offset = part ? split : 0, n = part ? c->len - split : split;
				memset(out, 0xa5, n * c->N + 4); memcpy(before, out, n * c->N + 4);
				int ret = osmo_conv_encode_raw(&e, in + offset, out, n);
				start("encode_raw"); printf(",\"n\":%d", n);
				buffers(copy + offset, in + offset, n, before, out, n * c->N + 4, ret); encoder_state(&e);
			}
			/* Explicit low-level flush even when one-shot does not flush; repeat. */
			for (int i = 0; i < 2; i++) {
				int size = (c->K - 1) * c->N + 4;
				memset(out, 0xa5, size); memcpy(before, out, size);
				int ret = osmo_conv_encode_flush(&e, out);
				start("encode_flush"); buffers(NULL, NULL, 0, before, out, size, ret); encoder_state(&e);
			}
		}
	}
	for (int i = 0; i < count; i++) {
		int value = encoded[i] ? -127 : 127;
		if (soft_mode == 1) value = 0;
		if (soft_mode == 2) value = i % 5 == 0 ? -value : value / 4;
		if (soft_mode == 3) value = encoded[i] ? -128 : 127;
		if (soft_mode == 4) value = (i % 3) - 1;
		if (soft_mode == 5) value = -128;
		soft[i] = value;
	}
	memcpy(soft_copy, soft, count);
	if (detailed && common_evidence() && c->K <= 9) generic(c, soft, count);
	for (int acc = 0; acc < 2; acc++) {
		/* Very short generic tail-biting traceback can read unreachable history. */
		if (!acc && !(c->N <= 4 && (c->K == 5 || c->K == 7)) &&
		    ((c->term == CONV_TERM_TAIL_BITING && c->len < c->K) ||
		     (c->len == 0 && c->term != CONV_TERM_FLUSH))) continue;
		if (!acc && c->K > 9 && c->term == CONV_TERM_TAIL_BITING) continue;
		memset(out, 0xa5, c->len + 4); memcpy(before, out, c->len + 4);
		int ret = acc ? osmo_conv_decode_acc(c, soft, out) : osmo_conv_decode(c, soft, out);
		start(acc ? "decode_acc" : "decode"); printf(",\"soft_mode\":%d", soft_mode);
		buffers(soft_copy, soft, count, before, out, c->len + 4, ret); puts("}");
	}
}

static struct osmo_conv_code make_code(int n, int k, int len, int term, int punc, int recursive)
{
	int ns = 1 << (k - 1), mask = ns - 1;
	/* Fixture descriptors only; no encoder/decoder result is computed here. */
	for (int s = 0; s < ns; s++) {
		for (int b = 0; b < 2; b++) {
			int feedback = recursive == 1 ? b ^ (__builtin_parity((unsigned)s & 3)) : b;
			int reg = (s << 1) | feedback, symbol = 0;
			states[s][b] = reg & mask;
			for (int j = 0; j < n; j++) {
				unsigned poly = (1u << (k - 1)) | 1u | (j % 2 ? 2u : 4u);
				int bit = __builtin_parity((unsigned)reg & poly);
				if (recursive == 1 && j == 0) bit = b;
				symbol = (symbol << 1) | bit;
			}
			outputs[s][b] = symbol;
		}
		term_states[s] = (s << 1) & mask;
		int b = recursive == 1 ? __builtin_parity((unsigned)s & 3) : 0;
		term_outputs[s] = outputs[s][b];
	}
	int raw = n * (len + (term == CONV_TERM_FLUSH ? k - 1 : 0)), p = 0;
	if (punc == 2) for (int i = 0; i < raw; i += 3) punctures[p++] = i;
	if (punc == 3) { punctures[p++] = 0; punctures[p++] = raw; punctures[p++] = raw + 7; }
	if (punc == 4) for (int i = 0; i < raw; i++) punctures[p++] = i;
	punctures[p] = -1;
	return (struct osmo_conv_code) { .N = n, .K = k, .len = len, .term = term,
		.next_output = (const uint8_t (*)[2])outputs, .next_state = (const uint8_t (*)[2])states,
		.next_term_output = recursive ? term_outputs : NULL, .next_term_state = recursive ? term_states : NULL,
		.puncture = punc ? punctures : NULL };
}

static const char *runtime_backend(const char *requested, char **argv)
{
#ifdef CONV_ISOLATED
	osmo_conv_init();
	if (!strcmp(requested, "scalar")) { INIT_POINTERS(gen); }
	else if (!strcmp(requested, "sse") && __builtin_cpu_supports("ssse3") && __builtin_cpu_supports("sse4.1")) { INIT_POINTERS(sse); }
	else if (!strcmp(requested, "avx") && __builtin_cpu_supports("avx2") && __builtin_cpu_supports("ssse3") && __builtin_cpu_supports("sse4.1")) { INIT_POINTERS(sse_avx); }
	else { fprintf(stderr, "Required forced backend unavailable: %s\n", requested); exit(1); }
	(void)argv;
	return requested;
#else
	Dl_info info;
	struct osmo_conv_code invalid = { .N = 1 };
	osmo_conv_decode_acc(&invalid, NULL, NULL);
	assert(dladdr((void *)osmo_conv_decode_acc, &info));
	uintptr_t base = (uintptr_t)info.dli_fbase;
	/* Offsets come from the pinned ELF symbol table, not CPU inference. */
	void *actual = *(void **)(base + strtoull(argv[4], NULL, 16));
	for (int i = 0; i < 3; i++) if ((uintptr_t)actual == base + strtoull(argv[5 + i], NULL, 16))
		return (const char *[]) { "scalar", "sse", "avx" }[i];
	fprintf(stderr, "Unrecognized linked runtime metric implementation\n"); exit(1);
	(void)requested;
#endif
}

int main(int argc, char **argv)
{
	if (argc != 11) return 2;
	if (!strcmp(argv[1], "diagnostic-shift")) {
		struct osmo_conv_code c = make_code(2, 3, 1, 1, 0, 0);
		struct osmo_conv_encoder e;
		uint8_t in[1] = {0}, out[64];
		c.N = 33;
		osmo_conv_encode_init(&e, &c);
		return osmo_conv_encode_raw(&e, in, out, 1);
	}
	if (!strcmp(argv[1], "diagnostic-overflow")) {
		struct osmo_conv_code c = { .N = INT32_MAX, .K = 3, .len = 2, .term = CONV_TERM_TRUNCATION };
		return osmo_conv_get_output_length(&c, 0);
	}
	backend = argv[1];
	const char *runtime = runtime_backend(backend, argv);
	printf("{\"type\":\"meta\",\"schema\":1,\"backend\":\"%s\",\"runtime\":\"%s\",\"upstream_git_revision\":\"unknown\",\"manifest_sha256\":\"%s\",\"library_sha256\":\"%s\",\"harness_sha256\":\"%s\",\"runner_sha256\":\"%s\",\"generator_manifest_sha256\":\"%s\",\"compiler\":\"%s\",\"neon_executed\":0}\n", backend, runtime, argv[2], argv[3], argv[8], argv[9], argv[10], __VERSION__);
	uint8_t input[CAP] = {0};
	char id[128], cid[96];
	int rates[] = {1, 2, 3, 4, 5, 8, 9}, constraints[] = {2, 3, 5, 7, 9, 10};
	for (int r = 0; r < 7; r++) for (int q = 0; q < 6; q++) {
		int n = rates[r], k = constraints[q], len = k + 3;
		for (int term = 0; term < 4; term++) {
			int punc = (r + q + term) % 5;
			struct osmo_conv_code c = make_code(n, k, len, term == 3 ? 42 : term, punc, 0);
			snprintf(cid, sizeof(cid), "n%d-k%d-t%d-p%d", n, k, (int)c.term, punc); descriptor(cid, &c);
			for (int i = 0; i < len; i++) input[i] = ((i * 73 + q * 17 + r) ^ (i >> 1)) & 1;
			for (int mode = 0; mode < 6; mode++) {
				snprintf(id, sizeof(id), "%s-s%d", cid, mode);
				run_case(&c, id, input, mode, mode == 0 || ((n == 2 || n == 9) && k == 3));
			}
		}
	}
	/* Exhaust all input patterns at lengths 0..4 (tail-biting only when safe). */
	for (int len = 0; len <= 4; len++) for (int term = 0; term < 3; term++) {
		if (term == CONV_TERM_TAIL_BITING && len < 2) continue;
		struct osmo_conv_code c = make_code(2, 3, len, term, 0, 0);
		snprintf(cid, sizeof(cid), "exhaust-l%d-t%d", len, term); descriptor(cid, &c);
		for (int pat = 0; pat < (1 << len); pat++) {
			for (int i = 0; i < len; i++) input[i] = (pat >> i) & 1;
			snprintf(id, sizeof(id), "%s-b%d", cid, pat); run_case(&c, id, input, 0, 1);
		}
	}
	/* Small recursive descriptors, both explicit systematic and EPROTO cases. */
	for (int recursive = 1; recursive <= 2; recursive++) for (int term = 0; term < 3; term++) {
		struct osmo_conv_code c = make_code(3, 5, 12, term, 1, recursive);
		snprintf(cid, sizeof(cid), "recursive-r%d-t%d", recursive, term); descriptor(cid, &c);
		for (int i = 0; i < c.len; i++) input[i] = (i % 3) == 1;
		for (int mode = 0; mode < 6; mode++) {
			snprintf(id, sizeof(id), "%s-s%d", cid, mode); run_case(&c, id, input, mode, 1);
		}
	}
	/* Load-state accepts raw bytes, unlike transition-row hard input indices. */
	if (common_evidence()) for (int k = 9; k <= 10; k++) {
		struct osmo_conv_code c = make_code(9, k, 0, CONV_TERM_TRUNCATION, 0, 0);
		snprintf(cid, sizeof(cid), "packing-k%d", k); descriptor(cid, &c);
		for (int pattern = 0; pattern < 4; pattern++) {
			struct osmo_conv_encoder e;
			uint8_t bytes[16], copy[16];
			for (int i = 0; i < k - 1; i++) bytes[i] = pattern == 0 ? 0 : pattern == 1 ? 1 : pattern == 2 ? 0xff : (i == 0 ? 0x80 : i & 1);
			memcpy(copy, bytes, k - 1);
			snprintf(id, sizeof(id), "%s-b%d", cid, pattern); case_id = id; sequence = 0;
			osmo_conv_encode_init(&e, &c); start("encode_init"); encoder_state(&e);
			osmo_conv_encode_load_state(&e, bytes); start("encode_load_state");
			buffers(copy, bytes, k - 1, NULL, NULL, 0, 0); encoder_state(&e);
		}
	}
	/* Correlation normalization: before/at/after and multiple intervals. */
	for (int n = 2; n <= 4; n++) for (int k = 5; k <= 7; k += 2) {
		int interval = INT16_MAX / (n * INT8_MAX) - k;
		for (int edge = -1; edge <= 2; edge++) {
			int len = (edge == 2 ? interval * 3 + 1 : interval + edge) - (k - 1);
			for (int recursive = 0; recursive <= 2; recursive++) {
				struct osmo_conv_code c = make_code(n, k, len, CONV_TERM_FLUSH, 2, recursive);
				snprintf(cid, sizeof(cid), "norm-n%d-k%d-l%d-r%d", n, k, len, recursive); descriptor(cid, &c);
				uint32_t seed = 0x12345678;
				for (int i = 0; i < len; i++) { seed = seed * 1664525u + 1013904223u; input[i] = seed >> 31; }
				for (int mode = 0; mode < 6; mode++) {
					snprintf(id, sizeof(id), "%s-s%d", cid, mode); run_case(&c, id, input, mode, 0);
				}
			}
		}
	}
	/* Identical descriptors differing only in NULL versus empty punctures,
	 * with zero/one/alternating/impulse/fixed-seed larger inputs. */
	for (int family = 0; family < 2; family++) for (int punc = 0; punc < 2; punc++) {
		struct osmo_conv_code c = make_code(family ? 5 : 2, family ? 3 : 5, 20, CONV_TERM_FLUSH, punc, 0);
		snprintf(cid, sizeof(cid), "patterns-f%d-p%d", family, punc); descriptor(cid, &c);
		for (int pattern = 0; pattern < 5; pattern++) {
			uint32_t seed = 0x12345678;
			for (int i = 0; i < c.len; i++) {
				seed = seed * 1664525u + 1013904223u;
				input[i] = pattern == 0 ? 0 : pattern == 1 ? 1 : pattern == 2 ? i & 1 : pattern == 3 ? i == 0 : seed >> 31;
			}
			for (int mode = 0; mode < 6; mode++) {
				snprintf(id, sizeof(id), "%s-b%d-s%d", cid, pattern, mode);
				run_case(&c, id, input, mode, pattern == 4);
			}
		}
	}
	/* Safely rejected accelerated length zero, including public dispatch. */
	struct osmo_conv_code zero = make_code(2, 5, 0, CONV_TERM_TRUNCATION, 1, 0);
	descriptor("zero-acc", &zero); run_case(&zero, "zero-acc", input, 0, 0);
	for (int v = 0; v < 2; v++) {
		const struct osmo_conv_code *c = v ? &conv_wimax_fch : &conv_gmr1_tch3_speech;
		const uint8_t *packed = v ? upstream_wimax_in : upstream_gmr_in;
		const uint8_t *expected = v ? upstream_wimax_out : upstream_gmr_out;
		uint8_t encoded[CAP];
		for (int i = 0; i < c->len; i++) input[i] = (packed[i / 8] >> (7 - i % 8)) & 1;
		int count = osmo_conv_encode(c, input, encoded);
		for (int i = 0; i < count; i++) assert(encoded[i] == ((expected[i / 8] >> (7 - i % 8)) & 1));
		descriptor(v ? "upstream-wimax" : "upstream-gmr", c);
		for (int mode = 0; mode < 6; mode++) {
			snprintf(id, sizeof(id), "%s-s%d", code_id, mode); run_case(c, id, input, mode, mode == 0);
		}
	}
	return 0;
}