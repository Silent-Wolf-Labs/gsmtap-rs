/* Direct observations from the required local libosmocore; no Rust oracle. */
#include <limits.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/prim.h>

static const uint32_t saps[] = {0, 1, _SAP_GSM_BASE, _SAP_TETRA_BASE, _SAP_SS7_BASE, UINT32_MAX};
static const uint32_t primitives[] = {0, 1, 255, 256, 0xffffff, 0x1000000, 0x80000000, UINT32_MAX};
static const uint32_t operations[] = {0, 1, 2, 3, 4, 255, 256, 257, 0x7fffffff, 0x80000000, UINT32_MAX};

static void snapshot(const struct msgb *msg, char *out)
{
    char *p = out;
    for (unsigned i = 0; i < 16; ++i)
        p += sprintf(p, "%02x", msg->head[i]);
    p += sprintf(p, "/%td/%td/%u/%td/%td/%td/%td", msg->data - msg->head,
                 msg->tail - msg->head, msg->len, msg->l1h - msg->head,
                 msg->l2h - msg->head, msg->l3h - msg->head, msg->l4h - msg->head);
    for (unsigned i = 0; i < 5; ++i)
        p += sprintf(p, "/%lu", msg->cb[i]);
}

static int callback(struct osmo_prim_hdr *header, void *ctx)
{
    unsigned *count = ctx;
    ++*count;
    header->sap = 42;
    header->msg->data[0] ^= 0xff;
    return -23;
}

int main(int argc, char **argv)
{
    if (argc != 7 || sizeof(unsigned int) != 4 || sizeof(enum osmo_prim_operation) != 4
        || sizeof(unsigned long) != 8 || sizeof(void *) != 8
        || (enum osmo_prim_operation)-1 <= 0)
        return 1;
    printf("{\"kind\":\"metadata\",\"revision\":\"950430e829a3dc1d162aa241bc0505745c5a7311\"");
    const char *keys[] = {"prim_source", "prim_header", "utils_source", "utils_header", "msgb_source", "msgb_header"};
    for (unsigned i = 0; i < 6; ++i)
        printf(",\"%s\":\"%s\"", keys[i], argv[i + 1]);
    printf(",\"uint_bits\":%zu,\"enum_bits\":%zu,\"ulong_bits\":%zu,\"pointer_bits\":%zu,\"enum_unsigned\":1,\"sap_shift\":%u,\"gsm_base\":%u,\"tetra_base\":%u,\"ss7_base\":%u,\"no_event\":%u}\n",
           sizeof(unsigned int)*CHAR_BIT, sizeof(enum osmo_prim_operation)*CHAR_BIT,
           sizeof(unsigned long)*CHAR_BIT, sizeof(void *)*CHAR_BIT,
           _SAP_GSM_SHIFT, _SAP_GSM_BASE, _SAP_TETRA_BASE, _SAP_SS7_BASE, OSMO_NO_EVENT);
    struct msgb *msg = msgb_alloc(16, "prim-observation");
    if (!msg)
        return 1;
    for (unsigned i = 0; i < 16; ++i)
        msg->head[i] = 0xa5 ^ i;
    msgb_reserve(msg, 4);
    msgb_put(msg, 4);
    msg->l1h = msg->head + 4;
    msg->l2h = msg->head + 5;
    msg->l3h = msg->head + 6;
    msg->l4h = msg->head + 8;
    for (unsigned i = 0; i < 5; ++i)
        msg->cb[i] = i + 1;
    unsigned id = 0;
    for (unsigned s = 0; s < ARRAY_SIZE(saps); ++s)
    for (unsigned p = 0; p < ARRAY_SIZE(primitives); ++p)
    for (unsigned o = 0; o < ARRAY_SIZE(operations); ++o)
    for (unsigned attached = 0; attached < 2; ++attached) {
        struct osmo_prim_hdr header;
        memset(&header, 0x7e, sizeof(header));
        for (unsigned phase = 0; phase < 2; ++phase) {
            uint32_t sap = saps[s] ^ (phase ? UINT32_MAX : 0);
            uint32_t primitive = primitives[p] ^ (phase ? UINT32_MAX : 0);
            uint32_t operation = operations[o] ^ (phase ? UINT32_MAX : 0);
            struct msgb *associated = (attached ^ phase) ? msg : NULL;
            char before[256], after[256];
            snapshot(msg, before);
            osmo_prim_init(&header, sap, primitive, (enum osmo_prim_operation)operation, associated);
            snapshot(msg, after);
            printf("{\"kind\":\"init\",\"case\":%u,\"sap\":%"PRIu32",\"primitive\":%"PRIu32",\"operation\":%"PRIu32",\"attached\":%u,\"phase\":%u,\"sap_out\":%u,\"primitive_out\":%u,\"operation_out\":%u,\"association\":%u,\"packed\":%u,\"header_packed\":%u,\"name\":\"%s\",\"before\":\"%s\",\"after\":\"%s\"}\n",
                   id++, sap, primitive, operation, attached ^ phase, phase,
                   header.sap, header.primitive, (unsigned)header.operation,
                   header.msg == associated, OSMO_PRIM(primitive, operation), OSMO_PRIM_HDR(&header),
                   osmo_prim_operation_name(header.operation), before, after);
        }
    }
    for (unsigned s = 0; s < ARRAY_SIZE(saps); ++s)
    for (unsigned p = 0; p < ARRAY_SIZE(primitives); ++p)
    for (unsigned o = 0; o < ARRAY_SIZE(operations); ++o)
    for (unsigned scenario = 0; scenario < 9; ++scenario) {
        struct osmo_prim_hdr header;
        osmo_prim_init(&header, saps[s], primitives[p], (enum osmo_prim_operation)operations[o], msg);
        struct osmo_prim_event_map exact = {header.sap, header.primitive, header.operation, 17};
        struct osmo_prim_event_map maps[4] = {{0, 0, 0, OSMO_NO_EVENT}};
        maps[0] = exact;
        maps[1] = (struct osmo_prim_event_map){0, 0, 0, OSMO_NO_EVENT};
        unsigned count = 2;
        switch (scenario) {
        case 0: maps[0].event = OSMO_NO_EVENT; count = 1; break;
        case 1: break;
        case 2: maps[0].sap ^= 1; break;
        case 3: maps[0].primitive ^= 1; break;
        case 4: maps[0].operation = (enum osmo_prim_operation)(operations[o] ^ 256); break;
        case 5: maps[1] = exact; maps[1].event = 23;
                maps[2].event = OSMO_NO_EVENT; count = 3; break;
        case 6: maps[0].event = OSMO_NO_EVENT; maps[1] = exact;
                maps[2].event = OSMO_NO_EVENT; count = 3; break;
        case 7: maps[0].sap ^= 1; maps[1] = exact; maps[1].event = 0;
                maps[2].event = OSMO_NO_EVENT; count = 3; break;
        case 8: maps[0].event = UINT32_MAX - 1; break;
        }
        char before[256], after[256];
        snapshot(msg, before);
        uint32_t event = osmo_event_for_prim(&header, maps);
        snapshot(msg, after);
        printf("{\"kind\":\"event\",\"case\":%u,\"sap\":%u,\"primitive\":%u,\"operation\":%u,\"scenario\":%u,\"event\":%"PRIu32",\"map\":\"",
               id++, header.sap, header.primitive, (unsigned)header.operation, scenario, event);
        for (unsigned i = 0; i < count; ++i)
            printf("%s%u/%u/%u/%"PRIu32, i ? ";" : "", maps[i].sap, maps[i].primitive, (unsigned)maps[i].operation, maps[i].event);
        printf("\",\"before\":\"%s\",\"after\":\"%s\"}\n", before, after);
    }
    struct osmo_prim_hdr header;
    osmo_prim_init(&header, 17, 23, PRIM_OP_RESPONSE, msg);
    unsigned context = 7;
    char before[256], after[256];
    snapshot(msg, before);
    osmo_prim_cb cb = callback;
    int result = cb(&header, &context);
    snapshot(msg, after);
    printf("{\"kind\":\"callback\",\"case\":%u,\"return_code\":%d,\"context\":%u,\"sap_out\":%u,\"primitive_out\":%u,\"operation_out\":%u,\"association\":%u,\"before\":\"%s\",\"after\":\"%s\"}\n",
           id++, result, context, header.sap, header.primitive, (unsigned)header.operation, header.msg == msg, before, after);
    msgb_free(msg);
    return 0;
}
