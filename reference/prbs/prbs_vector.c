/*
 * Reference harness for libosmocore's PRBS generator.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one
 * record per call with its return value, the complete destination and the
 * generator state afterwards.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <osmocom/core/prbs.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define MAX_OUT 32767u
#define GUARD 4u
#define FILL 0xa5

static ubit_t backing[MAX_OUT + GUARD];

struct run {
	const char *name;
	const struct osmo_prbs *prbs;
	struct osmo_prbs_state state;
	unsigned int step;
};

static void emit(struct run *run, const char *op, unsigned int out_len, int ret, size_t dst_len)
{
	size_t i;

	printf("{\"case\":\"%s\",\"step\":%u,\"op\":\"%s\",\"name\":\"%s\",\"len\":%u,"
	       "\"coeff\":%" PRIu64 ",\"out_len\":%u,\"ret\":%d,\"dst_hex\":\"",
	       run->name, run->step++, op, run->prbs->name, run->prbs->len, run->prbs->coeff,
	       out_len, ret);
	for (i = 0; i < dst_len; i++)
		printf("%02x", backing[i]);
	printf("\",\"state\":%" PRIu64 "}\n", run->state.state);
}

static void start(struct run *run, const char *name, const struct osmo_prbs *prbs)
{
	run->name = name;
	run->prbs = prbs;
	run->step = 0;
	osmo_prbs_state_init(&run->state, prbs);
	emit(run, "init", 0, 0, 0);
}

static void ubit(struct run *run, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		ubit_t bit = osmo_prbs_get_ubit(&run->state);
		emit(run, "ubit", 0, bit, 0);
	}
}

static void ubits(struct run *run, unsigned int out_len)
{
	int ret;

	memset(backing, FILL, out_len + GUARD);
	ret = osmo_prbs_get_ubits(backing, out_len, &run->state);
	emit(run, "ubits", out_len, ret, out_len + GUARD);
}

static void two_periods(const char *name, const struct osmo_prbs *prbs)
{
	struct run run;
	unsigned int period = (1u << prbs->len) - 1;

	start(&run, name, prbs);
	ubits(&run, period);
	ubits(&run, period);
}

static void bit_steps(const char *name, const struct osmo_prbs *prbs)
{
	struct run run;

	start(&run, name, prbs);
	ubit(&run, 40);
}

int main(void)
{
	/* Registers and coefficients the header doesn't provide. The generator
	 * ignores len, and any 64-bit coefficient is a valid public descriptor. */
	static const struct osmo_prbs zero = { "zero", 7, 0 };
	static const struct osmo_prbs fixed = { "fixed", 1, 1 };
	static const struct osmo_prbs wide = { "wide", 64, UINT64_C(0xc000000000000000) };
	static const struct osmo_prbs no_len = { "no-len", 0, (1 << 6) | (1 << 5) };
	struct run run;

	printf("{\"schema\":\"prbs-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");

	two_periods("prbs7_two_periods", &osmo_prbs7);
	two_periods("prbs9_two_periods", &osmo_prbs9);
	two_periods("prbs11_two_periods", &osmo_prbs11);
	two_periods("prbs15_two_periods", &osmo_prbs15);

	bit_steps("prbs7_bit_steps", &osmo_prbs7);
	bit_steps("prbs9_bit_steps", &osmo_prbs9);
	bit_steps("prbs11_bit_steps", &osmo_prbs11);
	bit_steps("prbs15_bit_steps", &osmo_prbs15);

	start(&run, "prbs7_mixed_calls", &osmo_prbs7);
	ubit(&run, 1);
	ubits(&run, 3);
	ubit(&run, 2);
	ubits(&run, 200);
	ubit(&run, 1);

	start(&run, "prbs9_zero_length", &osmo_prbs9);
	ubits(&run, 0);
	ubit(&run, 1);
	ubits(&run, 0);

	start(&run, "custom_zero_coefficient", &zero);
	ubits(&run, 16);

	start(&run, "custom_fixed_point", &fixed);
	ubits(&run, 16);

	start(&run, "custom_64_bit_register", &wide);
	ubits(&run, 300);
	ubit(&run, 4);

	start(&run, "custom_length_ignored", &no_len);
	ubits(&run, 200);

	return 0;
}
