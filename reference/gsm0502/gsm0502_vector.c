/*
 * Reference harness for libosmogsm's GSM 05.02 helpers.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one
 * record per observation. The first argument is upstream's gsm0502_test.ok; its
 * frame numbers are replayed and the run fails if a result differs from the file.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/logging.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm0502.h>
#include <osmocom/gsm/gsm48.h>
#include <osmocom/gsm/gsm_utils.h>
#include <osmocom/gsm/rsl.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"
#define HYPERFRAME ((uint32_t)GSM_TDMA_HYPERFRAME)

static const char *const channel_names[FN_REMAP_MAX] = {
	"TCH/F", "TCH/H0", "TCH/H1", "FACCH/F", "FACCH/H0", "FACCH/H1",
};

static void constant(const char *name, long value)
{
	printf("{\"kind\":\"constant\",\"name\":\"%s\",\"value\":%ld}\n", name, value);
}

#define CONSTANT(name) constant(#name, (long)(name))

static void constants(void)
{
	CONSTANT(GSM_TDMA_FN_DURATION_nS);
	CONSTANT(GSM_TDMA_FN_DURATION_uS);
	CONSTANT(GSM_TDMA_SUPERFRAME);
	CONSTANT(GSM_TDMA_HYPERFRAME);
	CONSTANT(GSM_NBITS_NB_GMSK_TAIL);
	CONSTANT(GSM_NBITS_NB_GMSK_PAYLOAD);
	CONSTANT(GSM_NBITS_NB_GMSK_TRAIN_SEQ);
	CONSTANT(GSM_NBITS_NB_GMSK_BURST);
	CONSTANT(GSM_NBITS_NB_8PSK_TAIL);
	CONSTANT(GSM_NBITS_NB_8PSK_PAYLOAD);
	CONSTANT(GSM_NBITS_NB_8PSK_TRAIN_SEQ);
	CONSTANT(GSM_NBITS_NB_8PSK_BURST);
	CONSTANT(GSM_NBITS_SB_GMSK_TAIL);
	CONSTANT(GSM_NBITS_SB_GMSK_PAYLOAD);
	CONSTANT(GSM_NBITS_SB_GMSK_ETRAIN_SEQ);
	CONSTANT(GSM_NBITS_SB_GMSK_BURST);
	CONSTANT(GSM_NBITS_DB_GMSK_TAIL);
	CONSTANT(GSM_NBITS_DB_GMSK_MIXED);
	CONSTANT(GSM_NBITS_DB_GMSK_BURST);
	CONSTANT(GSM_NBITS_AB_GMSK_ETAIL);
	CONSTANT(GSM_NBITS_AB_GMSK_SYNCH_SEQ);
	CONSTANT(GSM_NBITS_AB_GMSK_PAYLOAD);
	CONSTANT(GSM_NBITS_AB_GMSK_TAIL);
	CONSTANT(GSM_NBITS_AB_GMSK_BURST);
}

static void frame_pair(uint32_t a, uint32_t b)
{
	uint32_t inc = a, dec = a;

	GSM_TDMA_FN_INC(inc);
	GSM_TDMA_FN_DEC(dec);
	printf("{\"kind\":\"frame\",\"a\":%" PRIu32 ",\"b\":%" PRIu32 ",\"sum\":%" PRIu32
	       ",\"sub\":%" PRIu32 ",\"diff\":%" PRIu32 ",\"cmp\":%d,\"inc\":%" PRIu32
	       ",\"dec\":%" PRIu32 "}\n",
	       a, b, (uint32_t)GSM_TDMA_FN_SUM(a, b), (uint32_t)GSM_TDMA_FN_SUB(a, b),
	       (uint32_t)GSM_TDMA_FN_DIFF(a, b), gsm0502_fncmp(a, b), inc, dec);
}

static uint32_t next_random(uint32_t *state)
{
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

static void frames(void)
{
	/* Includes every value upstream's gsm0502_fncmp test uses. */
	static const uint32_t edges[] = {
		0, 1, 42, 1337, 1357820, 1357823, 1357824, 1357825, 2715642, 2715646,
		2715647, 2715648, 2715649, 5431296, 0x7fffffff, 0xfffffffe, 0xffffffff,
	};
	uint32_t random_state = 0x05020502;
	size_t i, j;

	for (i = 0; i < ARRAY_SIZE(edges); i++)
		for (j = 0; j < ARRAY_SIZE(edges); j++)
			frame_pair(edges[i], edges[j]);
	for (i = 0; i < 200; i++) {
		uint32_t a = next_random(&random_state) % HYPERFRAME;

		frame_pair(a, next_random(&random_state) % HYPERFRAME);
	}
}

static void remap_record(const char *source, enum gsm0502_fn_remap_channel channel, uint32_t fn,
			 uint32_t begin)
{
	printf("{\"kind\":\"remap\",\"source\":\"%s\",\"channel\":\"%s\",\"fn\":%" PRIu32
	       ",\"begin\":%" PRIu32 "}\n",
	       source, channel_names[channel], fn, begin);
}

static void remap_upstream(const char *path)
{
	char line[256];
	FILE *file = fopen(path, "r");
	int channel = -1;
	unsigned int replayed = 0;

	if (!file) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	while (fgets(line, sizeof(line), file)) {
		unsigned int fn_end, end_104, fn_begin, begin_104;
		int i;

		line[strcspn(line, "\n")] = '\0';
		for (i = 0; i < FN_REMAP_MAX; i++)
			if (!strcmp(line, channel_names[i]))
				channel = i;
		if (sscanf(line, "fn_end=%u, fn_end%%104=%u, fn_begin=%u, fn_begin%%104=%u", &fn_end,
			   &end_104, &fn_begin, &begin_104) != 4)
			continue;
		if (channel < 0 || gsm0502_fn_remap(fn_end, channel) != fn_begin) {
			fprintf(stderr, "differs from upstream output: %s\n", line);
			exit(EXIT_FAILURE);
		}
		remap_record("upstream", channel, fn_end, fn_begin);
		replayed++;
	}
	fclose(file);
	if (!replayed) {
		fprintf(stderr, "no upstream observations in %s\n", path);
		exit(EXIT_FAILURE);
	}
}

static void remap_sweep(void)
{
	int channel;
	uint32_t fn;

	for (channel = 0; channel < FN_REMAP_MAX; channel++) {
		for (fn = 0; fn <= 104; fn++)
			remap_record("sweep", channel, fn, gsm0502_fn_remap(fn, channel));
		for (fn = HYPERFRAME - 26; fn <= HYPERFRAME + 26; fn++)
			remap_record("sweep", channel, fn, gsm0502_fn_remap(fn, channel));
		for (fn = 0xffffffe0; fn != 0; fn++)
			remap_record("sweep", channel, fn, gsm0502_fn_remap(fn, channel));
	}
}

static void ccch_blocks(void)
{
	static const uint32_t extra[] = { HYPERFRAME - 1, HYPERFRAME, 0xffffffff };
	uint32_t fn;
	size_t i;

	for (fn = 0; fn < 2 * 51; fn++)
		printf("{\"kind\":\"ccch\",\"fn\":%" PRIu32 ",\"block\":%d}\n", fn,
		       gsm0502_fn2ccch_block(fn));
	for (i = 0; i < ARRAY_SIZE(extra); i++)
		printf("{\"kind\":\"ccch\",\"fn\":%" PRIu32 ",\"block\":%d}\n", extra[i],
		       gsm0502_fn2ccch_block(extra[i]));
}

static void hop_row(const char *name, uint32_t start, uint32_t step, unsigned int count,
		    uint8_t hsn, uint8_t maio, size_t n, const uint16_t *ma)
{
	struct gsm_time time;
	unsigned int i;
	size_t j;

	printf("{\"kind\":\"hop\",\"case\":\"%s\",\"start\":%" PRIu32 ",\"step\":%" PRIu32
	       ",\"hsn\":%u,\"maio\":%u,\"n\":%zu,\"ma\":\"",
	       name, start, step, hsn, maio, n);
	for (j = 0; ma && j < n; j++)
		printf("%s%u", j ? " " : "", ma[j]);
	printf("\",\"values\":\"");
	for (i = 0; i < count; i++) {
		gsm_fn2gsmtime(&time, start + i * step);
		printf("%s%u", i ? " " : "", gsm0502_hop_seq_gen(&time, hsn, maio, n, ma));
	}
	printf("\"}\n");
}

static void hopping(void)
{
	static const size_t sizes[] = { 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64 };
	static const size_t wrap_sizes[] = { 1, 8, 64, 128, 1024, 65535 };
	static const uint8_t wrap_hsns[] = { 0, 1, 63 };
	static const uint8_t wrap_maios[] = { 0, 255 };
	static const size_t ma_sizes[] = { 1, 3, 8, 64 };
	static const uint8_t ma_hsns[] = { 0, 1, 10, 63 };
	uint16_t ma[64];
	unsigned int hsn, maio;
	size_t i, j, k;

	for (hsn = 0; hsn < 64; hsn++)
		for (i = 0; i < ARRAY_SIZE(sizes); i++)
			for (maio = 0; maio < 64; maio += 63)
				hop_row("coverage", 0, 1327, 32, hsn, maio, sizes[i], NULL);

	/* Size 200 keeps the whole sum of T2 and the table entry, so these rows reach
	 * and expose every RNTABLE entry. */
	for (i = 0; i < 64; i++)
		hop_row("table", i * GSM_TDMA_SUPERFRAME, 26, 51, 1, 0, 200, NULL);

	for (i = 0; i < ARRAY_SIZE(wrap_sizes); i++)
		for (j = 0; j < ARRAY_SIZE(wrap_hsns); j++)
			for (k = 0; k < ARRAY_SIZE(wrap_maios); k++) {
				hop_row("wrap", HYPERFRAME - 8, 1, 16, wrap_hsns[j], wrap_maios[k],
					wrap_sizes[i], NULL);
				hop_row("wrap", 0xfffffff8, 1, 8, wrap_hsns[j], wrap_maios[k],
					wrap_sizes[i], NULL);
			}

	for (i = 0; i < ARRAY_SIZE(ma); i++)
		ma[i] = 512 + 3 * i;
	for (i = 0; i < ARRAY_SIZE(ma_sizes); i++)
		for (j = 0; j < ARRAY_SIZE(ma_hsns); j++)
			hop_row("allocation", 0, 1327, 32, ma_hsns[j], 1, ma_sizes[i], ma);
}

static void paging(void)
{
	static const uint64_t imsis[] = {
		0, 999, 262011234567890ULL, UINT64_MAX,
	};
	unsigned int conf, ag, pa;
	size_t i;

	/* Every value the three-bit fields can hold, including reserved codes. */
	for (conf = 0; conf < 8; conf++)
		for (ag = 0; ag < 8; ag++)
			for (pa = 0; pa < 8; pa++)
				for (i = 0; i < ARRAY_SIZE(imsis); i++) {
					struct gsm48_control_channel_descr descr;

					memset(&descr, 0, sizeof(descr));
					descr.ccch_conf = conf;
					descr.bs_ag_blks_res = ag;
					descr.bs_pa_mfrms = pa;
					printf("{\"kind\":\"paging\",\"conf\":%u,\"ag\":%u,\"pa\":%u,"
					       "\"imsi\":%" PRIu64 ",\"blocks\":%u,\"subchannels\":%d,"
					       "\"channels\":%d,\"group\":%u}\n",
					       conf, ag, pa, imsis[i],
					       gsm0502_get_n_pag_blocks(&descr),
					       gsm48_number_of_paging_subchannels(&descr),
					       rsl_ccch_conf_to_bs_cc_chans(conf),
					       gsm0502_calc_paging_group(&descr, imsis[i]));
				}
}

static void paging_helpers(void)
{
	static const uint64_t imsis[] = {
		0, 1, 999, 1000, 262011234567890ULL, UINT64_MAX,
	};
	static const unsigned int channels[] = { 1, 2, 3, 4, 0x10000 };
	static const int blocks[] = { 1, 2, 3, 8, 9, 16, 81, 0x10001 };
	size_t i, j, k;

	for (i = 0; i < ARRAY_SIZE(imsis); i++)
		for (j = 0; j < ARRAY_SIZE(channels); j++)
			for (k = 0; k < ARRAY_SIZE(blocks); k++)
				printf("{\"kind\":\"group\",\"imsi\":%" PRIu64 ",\"channels\":%u,"
				       "\"blocks\":%d,\"ccch\":%u,\"paging\":%u}\n",
				       imsis[i], channels[j], blocks[k],
				       gsm0502_get_ccch_group(imsis[i], channels[j], blocks[k]),
				       gsm0502_get_paging_group(imsis[i], channels[j], blocks[k]));
}

int main(int argc, char **argv)
{
	/* An unmappable frame number logs, and logging aborts without this. */
	static const struct log_info info = { 0 };

	if (argc != 2) {
		fprintf(stderr, "usage: %s gsm0502_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}
	log_init(&info, NULL);

	printf("{\"schema\":\"gsm0502-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	constants();
	frames();
	remap_upstream(argv[1]);
	remap_sweep();
	ccch_blocks();
	hopping();
	paging();
	paging_helpers();
	return EXIT_SUCCESS;
}
