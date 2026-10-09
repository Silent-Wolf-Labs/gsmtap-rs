#pragma once
/* Direct C observations only. Input construction never supplies outcomes. */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <setjmp.h>
#include <stdint.h>
#include <stddef.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/panic.h>

static jmp_buf panic_env;
static unsigned int case_id;
static max_align_t sentinel;
static void caught_panic(const char *fmt, va_list args)
{
    (void)fmt; (void)args;
    longjmp(panic_env, 1);
}
static void hex(const unsigned char *data, unsigned int size)
{
    putchar('"');
    for (unsigned int i = 0; i < size; i++) printf("%02x", data[i]);
    putchar('"');
}
static void snapshot(const struct msgb *m, const char *prefix)
{
    printf(",\"%s_hex\":", prefix); hex(m->head, m->data_len);
    printf(",\"%s_data\":%td,\"%s_tail\":%td,\"%s_len\":%u", prefix,
           m->data-m->head, prefix, m->tail-m->head, prefix, m->len);
    printf(",\"%s_layers\":\"", prefix);
    unsigned char *layers[] = {m->l1h,m->l2h,m->l3h,m->l4h};
    for (unsigned int i = 0; i < 4; i++) printf("%s%td", i ? "/" : "", layers[i] ? layers[i]-m->head : -1);
    printf("\",\"%s_cb\":\"%lu/%lu/%lu/%lu/%lu\"", prefix,
           m->cb[0],m->cb[1],m->cb[2],m->cb[3],m->cb[4]);
    printf(",\"%s_dst\":%d,\"%s_lchan\":%d",prefix,m->dst != NULL,prefix,m->lchan != NULL);
    printf(",\"%s_headlen\":%u,\"%s_headroom\":%d,\"%s_tailroom\":%d,\"%s_invariant\":%d",
           prefix,msgb_headlen(m),prefix,msgb_headroom(m),prefix,msgb_tailroom(m),prefix,msgb_test_invariant(m));
    printf(",\"%s_length\":%u,\"%s_data_return\":%td",prefix,msgb_length(m),prefix,msgb_data(m)-m->head);
}
static void start_record(const struct msgb *m, const char *api, int64_t arg, unsigned int scenario, unsigned int step)
{
    printf("%s{\"case\":%u,\"api\":\"%s\",\"arg\":%lld,\"scenario\":%u,\"step\":%u,\"capacity\":%u",
           case_id ? ",\n" : "",case_id,api,(long long)arg,scenario,step,m->data_len);
    case_id++;
    printf(",\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\"");
    printf(",\"source_sha256\":\"eaf5f2fe4f3cfa3cf44de33d5c47804990e120e5ab8f8d17e797bd08af437ee1\"");
    printf(",\"header_sha256\":\"26ab5bf78406a29d60d8079ae5516b553234b433aec100c958cdc50bb80f5f4a\"");
    printf(",\"test_sha256\":\"5eac93541a8d80107b5512100bbc9743fbc263c4145da73a36deb91d23cb4bf4\"");
    printf(",\"panic_source_sha256\":\"8686ec3d0283bba6a0d8e168662f24f7a290f88e703691619d1f7d285827eb18\"");
    printf(",\"panic_header_sha256\":\"f2f6466f274bd045200626ade442479706ece3c9c4ba75164628ae84b7cb37fd\"");
    printf(",\"c_uint_bits\":%zu,\"c_ulong_bits\":%zu,\"c_pointer_bits\":%zu",sizeof(unsigned int)*CHAR_BIT,sizeof(unsigned long)*CHAR_BIT,sizeof(void*)*CHAR_BIT);
}
static struct msgb *prepared(unsigned int capacity, unsigned int data, unsigned int len, unsigned int metadata)
{
    struct msgb *m = msgb_alloc_c(NULL,capacity,"vectors");
    if (!m) abort();
    msgb_reserve(m,data); msgb_put(m,len);
    for (unsigned int i=0;i<capacity;i++) m->head[i]=0xa5 ^ (i & 255);
    if (metadata) {
        m->l1h=m->head+capacity/3; m->l2h=m->head+capacity/2;
        m->l3h=m->head+capacity*2/3; m->l4h=m->head+capacity;
        if (metadata==2 || metadata>=19) { m->l1h=m->head+capacity; m->l4h=m->head; }
        if (metadata>=3) {
            unsigned int mask=(metadata-3)%16;
            if (!(mask & 1)) m->l1h=NULL;
            if (!(mask & 2)) m->l2h=NULL;
            if (!(mask & 4)) m->l3h=NULL;
            if (!(mask & 8)) m->l4h=NULL;
        }
        for (unsigned int i=0;i<5;i++) m->cb[i]=i+1;
        m->dst=&sentinel; m->lchan=(struct gsm_lchan *)&sentinel;
    }
    return m;
}
