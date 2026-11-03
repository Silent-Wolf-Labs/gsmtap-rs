/*
 * Reference harness for the numeric helpers in libosmogsm's gsm_utils: bands,
 * power, ARFCNs, GSM time, reduced frame numbers, TLLIs, classmarks and RACH.
 *
 * Calls the public API directly and writes NDJSON: a metadata line, then one
 * record per observation. The first argument is upstream's gsm0408_test.ok. Its
 * RACH lines and the checks upstream's gsm0408_test.c makes on these functions are
 * replayed, and the run fails if a result differs.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm0502.h>
#include <osmocom/gsm/gsm_utils.h>

#define COMMIT "950430e829a3dc1d162aa241bc0505745c5a7311"

/* The eight bands, then codes that name no band. */
static const int band_codes[] = {
	GSM_BAND_850, GSM_BAND_900, GSM_BAND_1800, GSM_BAND_1900,
	GSM_BAND_450, GSM_BAND_480, GSM_BAND_750, GSM_BAND_810,
	0, 3, 0x100, -22,
};

static void fail(const char *what)
{
	fprintf(stderr, "upstream expectation failed: %s\n", what);
	exit(EXIT_FAILURE);
}

static uint32_t next_random(uint32_t *state)
{
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

static void constant(const char *name, long long value)
{
	printf("{\"kind\":\"constant\",\"name\":\"%s\",\"value\":%lld}\n", name, value);
}

#define CONSTANT(name) constant(#name, (long long)(name))

static void constants(void)
{
	CONSTANT(GSM_MAX_FN);
	CONSTANT(GSM_FN_UNSET);
	CONSTANT(GSM_RFN_MODULUS);
	CONSTANT(ARFCN_PCS);
	CONSTANT(ARFCN_UPLINK);
	CONSTANT(ARFCN_FLAG_MASK);
	CONSTANT(GSM_BAND_850);
	CONSTANT(GSM_BAND_900);
	CONSTANT(GSM_BAND_1800);
	CONSTANT(GSM_BAND_1900);
	CONSTANT(GSM_BAND_450);
	CONSTANT(GSM_BAND_480);
	CONSTANT(GSM_BAND_750);
	CONSTANT(GSM_BAND_810);
	CONSTANT(TLLI_LOCAL);
	CONSTANT(TLLI_FOREIGN);
	CONSTANT(TLLI_RANDOM);
	CONSTANT(TLLI_AUXILIARY);
	CONSTANT(TLLI_RESERVED);
	CONSTANT(TLLI_G_RNTI);
	CONSTANT(TLLI_RAND_G_RNTI);
}

static void band_names(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(band_codes); i++)
		printf("{\"kind\":\"band_name\",\"band\":%d,\"name\":\"%s\"}\n", band_codes[i],
		       gsm_band_name((enum gsm_band)band_codes[i]));
}

static void band_parse(void)
{
	/* No commas, colons or quotes: the fixture reader splits on them. */
	static const char *const inputs[] = {
		"450", "480", "750", "810", "850", "900", "1800", "1900",
		"GSM450", "GSM480", "GSM750", "GSM810", "GSM850", "GSM900", "DCS1800", "PCS1900",
		"", "GSM", "abc", " ", "0", "9", "90", "9000", "1", "4500", "2100",
		"0900", "00000000000000000000000900", "-900", "+900", " 900", "E-GSM900",
		"900MHz", "900 1800", "1800.5", "GSM 1900", "45 0",
		"99999999999999999999999999", "18446744073709551616", "4294968196",
	};

	for (size_t i = 0; i < ARRAY_SIZE(inputs); i++)
		printf("{\"kind\":\"band_parse\",\"input\":\"%s\",\"band\":%d}\n", inputs[i],
		       (int)gsm_band_parse(inputs[i]));
}

static void upstream_power_classes(void)
{
	static const struct {
		enum gsm_band band;
		int class;
		int expected; /* 0 means upstream only checks for a negative result. */
	} cases[] = {
		{ GSM_BAND_850, 0, 0 }, { GSM_BAND_850, 1, 43 }, { GSM_BAND_900, 3, 37 },
		{ GSM_BAND_1800, 2, 24 }, { GSM_BAND_1800, 3, 36 }, { GSM_BAND_1900, 3, 33 },
		{ GSM_BAND_1900, 4, 0 }, { GSM_BAND_900, 5, 29 }, { GSM_BAND_900, 6, 0 },
	};

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		int dbm = ms_class_gmsk_dbm(cases[i].band, cases[i].class);

		if (cases[i].expected ? dbm != cases[i].expected : dbm >= 0)
			fail("ms_class_gmsk_dbm");
	}
}

static void power_classes(void)
{
	static const int classes[] = { INT_MIN, -1, 0, 1, 2, 3, 4, 5, 6, 7, 256, INT_MAX };

	for (size_t b = 0; b < ARRAY_SIZE(band_codes); b++)
		for (size_t c = 0; c < ARRAY_SIZE(classes); c++)
			printf("{\"kind\":\"class\",\"band\":%d,\"class\":%d,\"dbm\":%d}\n",
			       band_codes[b], classes[c],
			       ms_class_gmsk_dbm((enum gsm_band)band_codes[b], classes[c]));
}

static void power_levels(void)
{
	for (size_t b = 0; b < ARRAY_SIZE(band_codes); b++) {
		enum gsm_band band = (enum gsm_band)band_codes[b];

		for (unsigned int dbm = 0; dbm <= 45; dbm++)
			printf("{\"kind\":\"ctl_level\",\"band\":%d,\"dbm\":%u,\"level\":%d}\n",
			       band_codes[b], dbm, ms_pwr_ctl_lvl(band, dbm));
		printf("{\"kind\":\"ctl_level\",\"band\":%d,\"dbm\":%u,\"level\":%d}\n",
		       band_codes[b], UINT_MAX, ms_pwr_ctl_lvl(band, UINT_MAX));
	}
}

static void power_dbm(void)
{
	for (size_t b = 0; b < ARRAY_SIZE(band_codes); b++)
		for (unsigned int level = 0; level <= 255; level++)
			printf("{\"kind\":\"pwr_dbm\",\"band\":%d,\"level\":%u,\"dbm\":%d}\n",
			       band_codes[b], level,
			       ms_pwr_dbm((enum gsm_band)band_codes[b], (uint8_t)level));
}

static void rxlev(void)
{
	/* dbm2rxlev adds 110 to its input, so inputs near INT_MAX are undefined. */
	static const int extremes[] = { INT_MIN, INT_MIN + 1, -100000, 100000, INT_MAX - 110 };

	for (unsigned int value = 0; value <= 255; value++)
		printf("{\"kind\":\"rxlev\",\"rxlev\":%u,\"dbm\":%d}\n", value,
		       rxlev2dbm((uint8_t)value));
	for (int dbm = -130; dbm <= -30; dbm++)
		printf("{\"kind\":\"dbm\",\"dbm\":%d,\"rxlev\":%u}\n", dbm, dbm2rxlev(dbm));
	for (size_t i = 0; i < ARRAY_SIZE(extremes); i++)
		printf("{\"kind\":\"dbm\",\"dbm\":%d,\"rxlev\":%u}\n", extremes[i],
		       dbm2rxlev(extremes[i]));
}

static void arfcn_record(uint16_t arfcn)
{
	/* The sentinel shows whether the call wrote its output. */
	enum gsm_band band = (enum gsm_band)-1;
	int rc = gsm_arfcn2band_rc(arfcn, &band);

	printf("{\"kind\":\"arfcn\",\"arfcn\":%u,\"rc\":%d,\"band\":%d,\"ul\":%u,\"dl\":%u}\n", arfcn,
	       rc, (int)band, gsm_arfcn2freq10(arfcn, 1), gsm_arfcn2freq10(arfcn, 0));
}

static void arfcns(void)
{
	static const uint16_t flags[] = { 0, 0x1000, 0x2000, ARFCN_UPLINK, ARFCN_PCS,
					  ARFCN_PCS | ARFCN_UPLINK, ARFCN_FLAG_MASK };
	static const uint16_t beyond[] = { 1024, 2047, 2048, 4095 };

	for (size_t f = 0; f < ARRAY_SIZE(flags); f++) {
		for (unsigned int arfcn = 0; arfcn <= 1023; arfcn++)
			arfcn_record((uint16_t)(flags[f] | arfcn));
		for (size_t i = 0; i < ARRAY_SIZE(beyond); i++)
			arfcn_record((uint16_t)(flags[f] | beyond[i]));
	}
}

static void freq_record(unsigned int freq10)
{
	/* Any nonzero argument means uplink, not just 1. */
	if (gsm_freq102arfcn((uint16_t)freq10, 1) != gsm_freq102arfcn((uint16_t)freq10, 2))
		fail("uplink flag");
	printf("{\"kind\":\"freq\",\"freq10\":%u,\"ul\":%u,\"dl\":%u}\n", freq10,
	       gsm_freq102arfcn((uint16_t)freq10, 1), gsm_freq102arfcn((uint16_t)freq10, 0));
}

static void freqs(void)
{
	static const uint16_t outside[] = { 0, 1, 100, 4000, 4449, 20001, 30000, 65534, 65535 };

	for (size_t i = 0; i < ARRAY_SIZE(outside) && outside[i] < 4450; i++)
		freq_record(outside[i]);
	for (unsigned int freq10 = 4450; freq10 <= 20000; freq10++)
		freq_record(freq10);
	for (size_t i = 0; i < ARRAY_SIZE(outside); i++)
		if (outside[i] > 20000)
			freq_record(outside[i]);
}

static void time_record(uint32_t fn)
{
	struct gsm_time time;
	char text[64];

	memset(&time, 0, sizeof(time));
	gsm_fn2gsmtime(&time, fn);
	snprintf(text, sizeof(text), "%s", gsm_fn_as_gsmtime_str(fn));
	if (strcmp(text, osmo_dump_gsmtime(&time)))
		fail("gsm_fn_as_gsmtime_str");
	printf("{\"kind\":\"time\",\"fn\":%" PRIu32 ",\"t1\":%u,\"t2\":%u,\"t3\":%u,\"tc\":%u"
	       ",\"rfn\":%u,\"back\":%" PRIu32 ",\"text\":\"%s\"}\n",
	       time.fn, time.t1, time.t2, time.t3, time.tc, gsm_fn2rfn(fn), gsm_gsmtime2fn(&time),
	       text);
}

static void upstream_time(void)
{
	for (uint32_t fn = 0; fn < 42432; fn++) {
		struct gsm_time time = {
			.t1 = (fn / 1326) % 32,
			.t2 = fn % 26,
			.t3 = fn % 51,
		};

		if (gsm_gsmtime2fn(&time) != fn)
			fail("gsm_gsmtime2fn");
	}
}

static void times(void)
{
	static const uint32_t edges[] = {
		42430, 42431, 42432, 42433, 86898, 86899, 86900,
		GSM_MAX_FN - 2, GSM_MAX_FN - 1, GSM_MAX_FN, GSM_MAX_FN + 1,
		2 * GSM_MAX_FN - 1, 2 * GSM_MAX_FN,
		65535u * 1326u, 65536u * 1326u - 1, 65536u * 1326u, 65536u * 1326u + 1,
		0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu,
	};
	uint32_t state = 0x6d2b79f5;

	for (uint32_t fn = 0; fn <= 1400; fn++)
		time_record(fn);
	for (size_t i = 0; i < ARRAY_SIZE(edges); i++)
		time_record(edges[i]);
	for (int i = 0; i < 300; i++)
		time_record(next_random(&state) % GSM_MAX_FN);
	for (int i = 0; i < 50; i++)
		time_record(next_random(&state));
}

static void time_fields(void)
{
	static const unsigned int t1s[] = { 0, 1, 31, 2047, 2048, 65535 };
	/* t2 26 and 52 with t3 0 reach a negative multiple of the modulus. */
	static const unsigned int t2s[] = { 0, 1, 25, 26, 52, 255 };
	static const unsigned int t3s[] = { 0, 1, 25, 50, 51, 255 };
	uint32_t state = 0x13579bdf;

	for (size_t a = 0; a < ARRAY_SIZE(t1s); a++)
		for (size_t b = 0; b < ARRAY_SIZE(t2s); b++)
			for (size_t c = 0; c < ARRAY_SIZE(t3s); c++) {
				/* The dump reads the fn field; the conversion ignores it. */
				struct gsm_time time = {
					.fn = next_random(&state),
					.t1 = (uint16_t)t1s[a],
					.t2 = (uint8_t)t2s[b],
					.t3 = (uint8_t)t3s[c],
					.tc = 7,
				};

				printf("{\"kind\":\"time_fields\",\"input_fn\":%" PRIu32
				       ",\"t1\":%u,\"t2\":%u,\"t3\":%u,\"fn\":%" PRIu32
				       ",\"text\":\"%s\"}\n",
				       time.fn, time.t1, time.t2, time.t3, gsm_gsmtime2fn(&time),
				       osmo_dump_gsmtime(&time));
			}
}

static void rfn_record(const char *source, uint16_t rfn, uint32_t curr)
{
	printf("{\"kind\":\"rfn\",\"source\":\"%s\",\"rfn\":%u,\"curr\":%" PRIu32 ",\"fn\":%" PRIu32
	       "}\n",
	       source, rfn, curr, gsm_rfn2fn(rfn, curr));
}

static void rfns(void)
{
	/* gsm_rfn2fn aborts for an rfn of GSM_RFN_MODULUS or more. */
	static const struct {
		uint32_t curr;
		uint16_t rfn;
		uint32_t expected;
	} upstream[] = {
		{ 0, 0, 0 },
		{ 0, 4, 4 },
		{ 2229729, 23322, 2229786 },
		{ 2229777, 23322, 2229786 },
		{ 1320458, 5070, 1320462 },
	};
	static const uint16_t rfn_values[] = { 0, 1, 4, 5070, 21215, 21216, 21217, 23322, 42430, 42431 };
	static const uint32_t currents[] = {
		0, 1, 4, 21215, 21216, 21217, 42431, 42432, 42433, 63648, 1320458, 2229729,
		GSM_MAX_FN - 42432, GSM_MAX_FN - 21216, GSM_MAX_FN - 1, GSM_MAX_FN, GSM_MAX_FN + 1,
		0xfffffffeu, 0xffffffffu,
	};
	uint32_t state = 0x2468ace1;

	for (size_t i = 0; i < ARRAY_SIZE(upstream); i++) {
		if (gsm_rfn2fn(upstream[i].rfn, upstream[i].curr) != upstream[i].expected)
			fail("gsm_rfn2fn");
		rfn_record("upstream", upstream[i].rfn, upstream[i].curr);
	}
	for (size_t r = 0; r < ARRAY_SIZE(rfn_values); r++)
		for (size_t c = 0; c < ARRAY_SIZE(currents); c++)
			rfn_record("sweep", rfn_values[r], currents[c]);
	for (int i = 0; i < 200; i++) {
		uint16_t rfn = (uint16_t)(next_random(&state) % GSM_RFN_MODULUS);

		rfn_record("sweep", rfn, next_random(&state) % GSM_MAX_FN);
	}
}

static void tlli_record(uint32_t tlli)
{
	printf("{\"kind\":\"tlli\",\"tlli\":%" PRIu32 ",\"type\":%d,\"tmsi\":%" PRIu32 "}\n", tlli,
	       gprs_tlli_type(tlli), gprs_tlli2tmsi(tlli));
}

static void tllis(void)
{
	uint32_t state = 0x0badcafe;

	/* The type is decided by at most the top five bits. */
	for (uint32_t prefix = 0; prefix < 32; prefix++) {
		tlli_record(prefix << 27);
		tlli_record(prefix << 27 | 1);
		tlli_record(prefix << 27 | 0x07ffffff);
	}
	for (int i = 0; i < 100; i++)
		tlli_record(next_random(&state));
}

static void tmsis(void)
{
	static const int types[] = { -1, TLLI_LOCAL, TLLI_FOREIGN, TLLI_RANDOM, TLLI_AUXILIARY,
				     TLLI_RESERVED, TLLI_G_RNTI, TLLI_RAND_G_RNTI, 7 };
	static const uint32_t values[] = { 0, 1, 0x3fffffff, 0x40000000, 0x7fffffff, 0x80000000,
					   0xbfffffff, 0xc0000000, 0xffffffff, 0x12345678 };

	for (size_t t = 0; t < ARRAY_SIZE(types); t++)
		for (size_t v = 0; v < ARRAY_SIZE(values); v++)
			printf("{\"kind\":\"tmsi\",\"tmsi\":%" PRIu32 ",\"type\":%d,\"tlli\":%" PRIu32
			       "}\n",
			       values[v], types[t],
			       gprs_tmsi2tlli(values[v], (enum gprs_tlli_type)types[t]));
}

static void classmarks(void)
{
	static const uint8_t cm2_first[] = { 0x00, 0x08, 0xf7, 0xff };
	static const uint8_t cm2_third[] = { 0x00, 0x01, 0x02, 0x03, 0xfc, 0xff };
	static const uint8_t cm3_values[] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x0f, 0xf0, 0xff };
	static const unsigned int indexes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 255, UINT_MAX };

	for (size_t a = 0; a < ARRAY_SIZE(cm2_first); a++)
		for (size_t c = 0; c < ARRAY_SIZE(cm2_third); c++)
			for (size_t n = 0; n < ARRAY_SIZE(indexes); n++) {
				uint8_t cm2[3] = { cm2_first[a], 0x55, cm2_third[c] };

				printf("{\"kind\":\"cm2\",\"first\":%u,\"third\":%u,\"n\":%u,"
				       "\"supported\":%d}\n",
				       cm2[0], cm2[2], indexes[n],
				       ms_cm2_a5n_support(cm2, indexes[n]));
			}
	for (size_t v = 0; v < ARRAY_SIZE(cm3_values); v++)
		for (size_t n = 0; n < ARRAY_SIZE(indexes); n++) {
			uint8_t cm3 = cm3_values[v];

			printf("{\"kind\":\"cm3\",\"first\":%u,\"n\":%u,\"supported\":%d}\n", cm3,
			       indexes[n], ms_cm3_a5n_support(&cm3, indexes[n]));
		}
}

static void rach(const char *ok_path)
{
	FILE *file = fopen(ok_path, "r");
	char line[256];
	unsigned int seen = 0;

	if (!file) {
		perror(ok_path);
		exit(EXIT_FAILURE);
	}
	while (fgets(line, sizeof(line), file)) {
		unsigned int raw, slots;

		if (sscanf(line, "rach_tx_integer_raw2val(0x%x): %u slots", &raw, &slots) != 2)
			continue;
		if (rach_tx_integer_raw2val((uint8_t)raw) != slots)
			fail("rach_tx_integer_raw2val");
		seen++;
	}
	fclose(file);
	if (seen != 16)
		fail("expected 16 RACH lines");

	for (int value = -2; value <= 16; value++)
		printf("{\"kind\":\"rach_max\",\"value\":%d,\"val2raw\":%d,\"raw2val\":%d}\n", value,
		       rach_max_trans_val2raw(value), rach_max_trans_raw2val(value));
	for (unsigned int raw = 0; raw <= 255; raw++)
		printf("{\"kind\":\"rach_tx\",\"raw\":%u,\"value\":%u}\n", raw,
		       rach_tx_integer_raw2val((uint8_t)raw));
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s gsm0408_test.ok\n", argv[0]);
		return EXIT_FAILURE;
	}

	upstream_power_classes();
	upstream_time();

	printf("{\"schema\":\"gsm-utils-numeric-v1\",\"libosmocore_commit\":\"" COMMIT "\"}\n");
	constants();
	band_names();
	band_parse();
	power_classes();
	power_levels();
	power_dbm();
	rxlev();
	arfcns();
	freqs();
	times();
	time_fields();
	rfns();
	tllis();
	tmsis();
	classmarks();
	rach(argv[1]);
	return EXIT_SUCCESS;
}
