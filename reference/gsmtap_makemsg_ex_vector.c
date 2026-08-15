/*
 * Reference harness for GSMTAP message constructors.
 *
 * This program intentionally delegates packet construction to libosmocore.
 * Its stdout is the conformance oracle; no Rust code participates in
 * generating it. Each invocation emits one JSON object suitable for
 * committing as a golden vector.
 *
 * Case names are stable fixture-contract identifiers. Rename or materially
 * change one only through an explicit fixture-contract review.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <osmocom/core/gsmtap.h>
#include <osmocom/core/gsmtap_util.h>
#include <osmocom/core/msgb.h>

enum constructor_api {
	CONSTRUCTOR_MAKEMSG_EX,
	CONSTRUCTOR_MAKEMSG,
};

struct vector_case {
	const char *name;
	enum constructor_api api;
	uint8_t type;
	uint16_t arfcn;
	uint8_t timeslot;
	uint8_t channel_type;
	uint8_t sub_slot;
	uint32_t frame_number;
	int8_t signal_dbm;
	int8_t snr_db;
	const uint8_t *payload;
	unsigned int payload_len;
};

static const uint8_t basic_payload[] = { 0xde, 0xad, 0xbe, 0xef };
static const uint8_t uplink_boundary_payload[] = { 0x00 };
static const uint8_t sim_atr_payload[] = { 0x3b, 0x00 };
static const uint8_t wrapper_payload[] = { 0x2b };

static const struct vector_case vector_cases[] = {
	{
		.name = "gsmtap_v2_basic_header",
		.api = CONSTRUCTOR_MAKEMSG_EX,
		.type = GSMTAP_TYPE_UM,
		.arfcn = 0x1234,
		.timeslot = 3,
		.channel_type = GSMTAP_CHANNEL_SDCCH8,
		.sub_slot = 1,
		.frame_number = 0x01020304,
		.signal_dbm = -73,
		.snr_db = 19,
		.payload = basic_payload,
		.payload_len = sizeof(basic_payload),
	},
	{
		.name = "gsmtap_um_uplink_pcs_boundary",
		.api = CONSTRUCTOR_MAKEMSG_EX,
		.type = GSMTAP_TYPE_UM,
		.arfcn = GSMTAP_ARFCN_F_UPLINK | GSMTAP_ARFCN_F_PCS | GSMTAP_ARFCN_MASK,
		.timeslot = 7,
		.channel_type = GSMTAP_CHANNEL_VOICE_H | GSMTAP_CHANNEL_ACCH,
		.sub_slot = 7,
		.frame_number = UINT32_MAX,
		.signal_dbm = INT8_MIN,
		.snr_db = INT8_MAX,
		.payload = uplink_boundary_payload,
		.payload_len = sizeof(uplink_boundary_payload),
	},
	{
		.name = "gsmtap_sim_atr",
		.api = CONSTRUCTOR_MAKEMSG_EX,
		.type = GSMTAP_TYPE_SIM,
		.arfcn = 0,
		.timeslot = 0,
		.channel_type = GSMTAP_SIM_ATR,
		.sub_slot = 0,
		.frame_number = 0,
		.signal_dbm = 0,
		.snr_db = 0,
		.payload = sim_atr_payload,
		.payload_len = sizeof(sim_atr_payload),
	},
	{
		.name = "gsmtap_makemsg_um_wrapper",
		.api = CONSTRUCTOR_MAKEMSG,
		.arfcn = 0x0042,
		.timeslot = 0,
		.channel_type = GSMTAP_CHANNEL_RACH,
		.sub_slot = 0,
		.frame_number = 0,
		.signal_dbm = -1,
		.snr_db = 0,
		.payload = wrapper_payload,
		.payload_len = sizeof(wrapper_payload),
	},
};

static const char *api_name(enum constructor_api api)
{
	return api == CONSTRUCTOR_MAKEMSG_EX ? "gsmtap_makemsg_ex" : "gsmtap_makemsg";
}

static const struct vector_case *find_case(const char *name)
{
	unsigned int index;

	for (index = 0; index < sizeof(vector_cases) / sizeof(vector_cases[0]); index++) {
		if (strcmp(vector_cases[index].name, name) == 0)
			return &vector_cases[index];
	}

	return NULL;
}

static void print_hex(const uint8_t *data, unsigned int length)
{
	unsigned int index;

	for (index = 0; index < length; index++)
		printf("%02x", data[index]);
}

static struct msgb *make_message(const struct vector_case *vector)
{
	if (vector->api == CONSTRUCTOR_MAKEMSG_EX) {
		return gsmtap_makemsg_ex(vector->type, vector->arfcn, vector->timeslot,
			vector->channel_type, vector->sub_slot, vector->frame_number,
			vector->signal_dbm, vector->snr_db, vector->payload, vector->payload_len);
	}

	return gsmtap_makemsg(vector->arfcn, vector->timeslot, vector->channel_type,
		vector->sub_slot, vector->frame_number, vector->signal_dbm, vector->snr_db,
		vector->payload, vector->payload_len);
}

static void print_vector(const struct vector_case *vector, const struct msgb *message)
{
	printf("{\"case\":\"%s\",", vector->name);
	printf("\"api\":\"%s\",", api_name(vector->api));
	printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
	puts("\"input\":{");
	if (vector->api == CONSTRUCTOR_MAKEMSG_EX)
		printf("\"type\":%u,", vector->type);
	printf("\"arfcn\":%u,\"timeslot\":%u,", vector->arfcn, vector->timeslot);
	printf("\"channel_type\":%u,\"sub_slot\":%u,", vector->channel_type, vector->sub_slot);
	printf("\"frame_number\":%u,\"signal_dbm\":%d,", vector->frame_number, vector->signal_dbm);
	printf("\"snr_db\":%d,\"payload_hex\":\"", vector->snr_db);
	print_hex(vector->payload, vector->payload_len);
	puts("\"},");
	printf("\"return_code\":0,\"message_created\":true,");
	printf("\"length\":%u,\"encoded_hex\":\"", message->len);
	print_hex(message->data, message->len);
	puts("\"}");
}

static void list_cases(void)
{
	unsigned int index;

	for (index = 0; index < sizeof(vector_cases) / sizeof(vector_cases[0]); index++)
		puts(vector_cases[index].name);
}

int main(int argc, char **argv)
{
	const struct vector_case *vector;
	struct msgb *message;

	if (argc == 2 && strcmp(argv[1], "--list") == 0) {
		list_cases();
		return 0;
	}
	if (argc != 3 || strcmp(argv[1], "--case") != 0) {
		fprintf(stderr, "usage: %s --list | --case CASE\n", argv[0]);
		return 2;
	}

	vector = find_case(argv[2]);
	if (vector == NULL) {
		fprintf(stderr, "unknown vector case: %s\n", argv[2]);
		return 2;
	}

	message = make_message(vector);
	if (message == NULL) {
		fprintf(stderr, "message construction failed for case: %s\n", vector->name);
		return 1;
	}

	print_vector(vector, message);
	msgb_free(message);
	return 0;
}
