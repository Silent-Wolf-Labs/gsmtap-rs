/*
 * Reference harness for libosmocore's soft-bit conversions.
 *
 * Calls osmo_ubit2sbit() on every byte value and osmo_sbit2ubit() on every soft bit, each
 * as one buffer, and writes NDJSON: a metadata line, then one record per input.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <stdio.h>

#include <osmocom/core/bits.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"

int main(void)
{
	ubit_t unpacked[256], from_soft[256];
	sbit_t soft[256], from_unpacked[256];

	for (int i = 0; i < 256; i++) {
		unpacked[i] = (ubit_t)i;
		soft[i] = (sbit_t)(i - 128);
	}
	osmo_ubit2sbit(from_unpacked, unpacked, 256);
	osmo_sbit2ubit(from_soft, soft, 256);

	printf("{\"schema\":\"soft_bits-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	for (int i = 0; i < 256; i++)
		printf("{\"kind\":\"ubit2sbit\",\"input\":%d,\"output\":%d}\n", unpacked[i], from_unpacked[i]);
	for (int i = 0; i < 256; i++)
		printf("{\"kind\":\"sbit2ubit\",\"input\":%d,\"output\":%d}\n", soft[i], from_soft[i]);
	return 0;
}
