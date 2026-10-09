#include "msgb_vector_common.h"
#include <string.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>

static void extras(void)
{
    printf(",\"utils_source_sha256\":\"a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4\"");
    printf(",\"utils_header_sha256\":\"3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d\"");
    printf(",\"list_header_sha256\":\"9015f71c9e30755d48f8f8597c8a5362cb98f5acccf5149b9cf6a5a6b508e39f\"");
}
static void layout(struct msgb *m, unsigned int kind)
{
    if (kind == 0) m->l1h=m->l2h=m->l3h=m->l4h=NULL;
    if (kind == 1) { m->l1h=m->data; m->l2h=m->data+m->len/3; m->l3h=m->data+m->len*2/3; m->l4h=m->tail; }
    if (kind == 2) { m->l1h=m->head; m->l2h=m->tail; m->l3h=m->data; m->l4h=m->head+m->data_len; }
}
static void observe(const char *api, unsigned int cap, unsigned int data, unsigned int len, unsigned int kind, unsigned int arg, unsigned int mode)
{
    struct msgb *m=prepared(cap,data,len,1); layout(m,kind);
    unsigned char payload[4096], output[20000], dest[20000];
    unsigned int payload_len=0, output_len=0, dest_len=0;
    int rc=0;
    memset(dest,0x7e,sizeof(dest));
    start_record(m,api,arg,case_id,0); extras();
    printf(",\"layout\":%u,\"mode\":%u",kind,mode);
    snapshot(m,"before");
    if (!strcmp(api,"eq_data") || !strcmp(api,"eq_msg")) {
        unsigned char *p=arg ? ((unsigned char *[]){m->l1h,m->l2h,m->l3h,m->l4h})[arg-1] : m->data;
        payload_len=m->tail-p;
        memcpy(payload,p,payload_len);
        if (mode==1 && payload_len) payload[0]^=255;
        if (mode==2) payload[payload_len++]=0;
        if (!strcmp(api,"eq_data")) {
            rc=_msgb_eq(__FILE__,__LINE__,__func__,arg,mode==3 ? NULL : m,mode==4 ? NULL : payload,payload_len,false);
        } else {
            struct msgb *other=msgb_alloc(payload_len,"other");
            memcpy(msgb_put(other,payload_len),payload,payload_len);
            other->l1h=other->l2h=other->l3h=other->l4h=other->data;
            switch(arg) {
            case 0: rc=msgb_eq(m,other); break;
            case 1: rc=msgb_eq_l1(m,other); break;
            case 2: rc=msgb_eq_l2(m,other); break;
            case 3: rc=msgb_eq_l3(m,other); break;
            case 4: rc=msgb_eq_l4(m,other); break;
            }
            msgb_free(other);
        }
    } else if (!strcmp(api,"dump_buf")) {
        dest_len=arg;
        const char *result=msgb_hexdump_buf((char *)dest,arg,m);
        rc=result==(char *)dest;
        output_len=strlen(result); memcpy(output,result,output_len);
    } else if (!strcmp(api,"dump") || !strcmp(api,"dump_owned")) {
        const char *result=!strcmp(api,"dump") ? msgb_hexdump(m) : msgb_hexdump_c(NULL,m);
        output_len=strlen(result); memcpy(output,result,output_len);
        rc=strcmp(result,"ERROR")!=0;
        if (!strcmp(api,"dump_owned") && rc) talloc_free((void *)result);
    } else if (!strcmp(api,"dump_layer")) {
        const char *result=NULL;
        switch(arg) { case 1:result=msgb_hexdump_l1(m);break;case 2:result=msgb_hexdump_l2(m);break;case 3:result=msgb_hexdump_l3(m);break;case 4:result=msgb_hexdump_l4(m);break; }
        output_len=strlen(result); memcpy(output,result,output_len); rc=1;
    } else {
        const char *texts[]={"","abc","x=002a/-7/%","A\0B","caf\xc3\xa9"};
        const unsigned int sizes[]={0,3,11,3,5};
        payload_len=sizes[mode];memcpy(payload,texts[mode],payload_len);
        switch(mode) {
        case 0:rc=msgb_printf(m,"%s","");break;
        case 1:rc=msgb_printf(m,"%s","abc");break;
        case 2:rc=msgb_printf(m,"x=%04x/%d/%%",42,-7);break;
        case 3:rc=msgb_printf(m,"A%cB",0);break;
        case 4:rc=msgb_printf(m,"%s","caf\xc3\xa9");break;
        }
    }
    printf(",\"payload_hex\":");hex(payload,payload_len);
    printf(",\"output_hex\":");hex(output,output_len);
    printf(",\"dest_hex\":");hex(dest,dest_len);
    printf(",\"return_code\":%d,\"panic\":0",rc);
    snapshot(m,"after");printf("}");msgb_free(m);
}
static void queue_observations(unsigned int initial)
{
    LLIST_HEAD(queue);
    unsigned int count=initial;
    /* IDs are stored in cb[0]; the whole remaining message is also observed. */
    struct msgb *marker=prepared(8,2,3,1);
    const char *apis[]={"dequeue_count","enqueue_count","enqueue","enqueue_count","dequeue","dequeue_count","enqueue_count","queue_free","dequeue_count"};
    unsigned int scenario=case_id;
    for (unsigned int step=0;step<9;step++) {
        const char *api=apis[step];
        start_record(marker,api,step,scenario,step);extras();
        printf(",\"layout\":0,\"mode\":%u,\"count_before\":%u,\"queue_before\":\"",initial,count);
        struct msgb *item;
        llist_for_each_entry(item,&queue,list) printf("%lu/",item->cb[0]);
        printf("\"");snapshot(marker,"before");
        struct msgb *result=NULL;
        if (!strncmp(api,"enqueue",7)) {
            item=prepared(8,2,3,1);item->cb[0]=step;
            if (!strcmp(api,"enqueue_count")) msgb_enqueue_count(&queue,item,&count); else msgb_enqueue(&queue,item);
        } else if (!strcmp(api,"queue_free")) msgb_queue_free(&queue);
        else if (!strcmp(api,"dequeue_count")) result=msgb_dequeue_count(&queue,&count);
        else result=msgb_dequeue(&queue);
        printf(",\"count_after\":%u,\"queue_after\":\"",count);
        llist_for_each_entry(item,&queue,list) printf("%lu/",item->cb[0]);
        printf("\",\"return_code\":%ld,\"panic\":0",result ? (long)result->cb[0] : -1L);
        snapshot(result ? result : marker,"after");printf("}");
        if (result) msgb_free(result);
    }
    msgb_free(marker);
}
int main(void)
{
    osmo_set_panic_handler(caught_panic);
    printf("[\n");
    const unsigned int caps[]={0,1,8,16};
    for (unsigned int ci=0;ci<4;ci++) {
        unsigned int cap=caps[ci];
        for (unsigned int data=0;data<=cap;data++) {
            if (data!=0 && data!=cap/2 && data!=cap) continue;
            for (unsigned int len=0;len<=cap-data;len++) {
                if (len!=0 && len!=(cap-data)/2 && len!=cap-data) continue;
                for(unsigned int level=0;level<=4;level++) {
                    for(unsigned int mode=0;mode<5;mode++) observe("eq_data",cap,data,len,1,level,mode);
                    for(unsigned int mode=0;mode<3;mode++) observe("eq_msg",cap,data,len,1,level,mode);
                }
                for(unsigned int kind=0;kind<3;kind++) {
                    observe("dump",cap,data,len,kind,0,0);
                    observe("dump_owned",cap,data,len,kind,0,0);
                    for(unsigned int size=0;size<=80;size++) {
                        if(kind==2 && size<80) continue; /* C annotation branches do not check snprintf truncation. */
                        observe("dump_buf",cap,data,len,kind,size,0);
                    }
                }
                for(unsigned int level=1;level<=4;level++) {
                    observe("dump_layer",cap,data,len,0,level,0);
                    observe("dump_layer",cap,data,len,1,level,0);
                }
                for(unsigned int mode=0;mode<5;mode++) observe("printf",cap,data,len,1,0,mode);
            }
        }
    }
    const unsigned int big[]={1364,1365,1366,3000};
    for(unsigned int i=0;i<4;i++) {
        for(unsigned int kind=0;kind<2;kind++) {
            observe("dump",big[i],0,big[i],kind,0,0);
            observe("dump_owned",big[i],0,big[i],kind,0,0);
            observe("dump_buf",big[i],0,big[i],kind,4100,0);
            observe("dump_layer",big[i],0,big[i],kind,1,0);
        }
    }
    queue_observations(0);queue_observations(1);queue_observations(UINT_MAX);
    printf("\n]\n");return 0;
}
