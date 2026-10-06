/* Direct C observations, verified against libosmocore revision
 * 950430e829a3dc1d162aa241bc0505745c5a7311. Input construction only;
 * all return values and final destination bytes come from the C APIs. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osmocom/core/bits.h>

static unsigned int observations;

static void hex(const uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; i++)
		printf("%02x", bytes[i]);
}

static void observe(const char *name, int packing, unsigned int bits,
		    const uint8_t *src, size_t src_len, size_t dst_len)
{
	/* Allocate exactly the recorded source and destination lengths; zero-bit
	 * calls also have real, nonempty backing. Neither API accepts capacity. */
	uint8_t *input = malloc(src_len);
	uint8_t *dst = malloc(dst_len);
	uint8_t *before = malloc(dst_len);
	if (!input || !dst || !before) {
		fprintf(stderr, "allocation failed\n");
		exit(1);
	}
	memcpy(input, src, src_len);
	for (size_t i = 0; i < dst_len; i++)
		before[i] = dst[i] = (uint8_t)(0xa5 ^ i);
	int rc = packing ? osmo_ubit2pbit(dst, input, bits) : osmo_pbit2ubit(dst, input, bits);
	printf("%s{\"case\":\"%s\",\"api\":\"%s\",", observations++ ? ",\n" : "", name,
	       packing ? "osmo_ubit2pbit" : "osmo_pbit2ubit");
	printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",\"num_bits\":%u,\"src_hex\":\"", bits);
	hex(src, src_len);
	printf("\",\"dst_len\":%zu,\"dst_before_hex\":\"", dst_len);
	hex(before, dst_len);
	printf("\",\"return_code\":%d,\"dst_after_hex\":\"", rc);
	hex(dst, dst_len);
	printf("\"}");
	free(before);
	free(dst);
	free(input);
}

static size_t packed_size(unsigned int bits)
{
	return bits / 8 + (bits % 8 != 0);
}

int main(void)
{
	char name[128];
	const unsigned int lengths[] = {7, 8, 9, 15, 16, 17, 31, 32, 33};
	const char *patterns[] = {"zero", "one", "alternating"};
	printf("[\n");
	for (unsigned int byte = 0; byte < 256; byte++) {
		uint8_t packed = byte, unpacked[8];
		for (unsigned int i = 0; i < 8; i++)
			unpacked[i] = (byte >> (7 - i)) & 1;
		for (unsigned int bits = 1; bits <= 8; bits++) {
			for (int packing = 0; packing <= 1; packing++) {
				for (unsigned int extra = 0; extra <= 1; extra++) {
					snprintf(name, sizeof(name), "%s_byte_%02x_bits_%u_%s",
						 packing ? "pack" : "unpack", byte, bits, extra ? "oversized" : "exact");
					observe(name, packing, bits, packing ? unpacked : &packed,
						packing ? 8 : 1, (packing ? packed_size(bits) : bits) + extra * 3);
				}
			}
		}
	}
	for (size_t l = 0; l < sizeof(lengths) / sizeof(lengths[0]); l++) {
		unsigned int bits = lengths[l];
		for (unsigned int pattern = 0; pattern < 3 + bits; pattern++) {
			uint8_t packed[5] = {0}, unpacked[33] = {0};
			char pattern_name[32];
			if (pattern < 3)
				snprintf(pattern_name, sizeof(pattern_name), "%s", patterns[pattern]);
			else
				snprintf(pattern_name, sizeof(pattern_name), "single_%02u", pattern - 3);
			for (unsigned int i = 0; i < bits; i++) {
				unpacked[i] = pattern == 1 || (pattern == 2 && i % 2 == 0) ||
					(pattern >= 3 && i == pattern - 3);
				packed[i / 8] |= unpacked[i] << (7 - i % 8);
			}
			for (int packing = 0; packing <= 1; packing++) {
				for (unsigned int extra = 0; extra <= 1; extra++) {
					snprintf(name, sizeof(name), "%s_boundary_%u_%s_%s", packing ? "pack" : "unpack",
						 bits, pattern_name, extra ? "oversized" : "exact");
					observe(name, packing, bits, packing ? unpacked : packed,
						packing ? bits : packed_size(bits), (packing ? packed_size(bits) : bits) + extra * 3);
				}
			}
		}
		/* Identical requested input with contrasting unrelated source suffixes. */
		for (int packing = 0; packing <= 1; packing++) {
			for (unsigned int tail = 0; tail <= 1; tail++) {
				uint8_t src[36] = {0};
				size_t used = packing ? bits : packed_size(bits);
				memset(src, packing ? 1 : 0xff, used);
				memset(src + used, packing ? tail : tail * 0xff, 3);
				snprintf(name, sizeof(name), "%s_extra_bits_%u_tail_%u", packing ? "pack" : "unpack", bits, tail);
				observe(name, packing, bits, src, used + 3, (packing ? packed_size(bits) : bits) + 3);
			}
		}
	}
	for (int packing = 0; packing <= 1; packing++) {
		for (unsigned int high = 0; high <= 1; high++) {
			uint8_t src = packing ? high : high * 0x80;
			for (unsigned int extra = 0; extra <= 1; extra++) {
				snprintf(name, sizeof(name), "%s_zero_bit_%u_%s", packing ? "pack" : "unpack", high,
					 extra ? "oversized" : "backed");
				observe(name, packing, 0, &src, 1, 1 + extra * 3);
			}
		}
	}
	printf("\n]\n");
	return ferror(stdout) ? 1 : 0;
}
