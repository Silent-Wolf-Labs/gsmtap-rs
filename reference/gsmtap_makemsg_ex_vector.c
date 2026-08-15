/*
 * Reference harness for gsmtap_makemsg_ex().
 *
 * This program intentionally delegates packet construction to libosmocore.
 * Its stdout is the conformance oracle; no Rust code participates in
 * generating it. The output is a single JSON object suitable for committing
 * as a golden vector.
 *
 * Reference source revision: 950430e829a3dc1d162aa241bc0505745c5a7311
 */

#include <stdint.h>
#include <stdio.h>

#include <osmocom/core/gsmtap.h>
#include <osmocom/core/gsmtap_util.h>
#include <osmocom/core/msgb.h>

int main(void)
{
	const uint8_t payload[] = { 0xde, 0xad, 0xbe, 0xef };
	struct msgb *message;
	unsigned int index;

	message = gsmtap_makemsg_ex(
		GSMTAP_TYPE_UM,
		0x1234,
		3,
		GSMTAP_CHANNEL_SDCCH8,
		1,
		0x01020304,
		-73,
		19,
		payload,
		sizeof(payload));
	if (message == NULL) {
		fputs("gsmtap_makemsg_ex failed\n", stderr);
		return 1;
	}

	printf("{\"case\":\"gsmtap_v2_basic_header\",");
	printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",");
	printf("\"return_code\":0,");
	printf("\"length\":%u,\"encoded_hex\":\"", message->len);
	for (index = 0; index < message->len; index++)
		printf("%02x", message->data[index]);
	puts("\"}");

	msgb_free(message);
	return 0;
}
