#include "msgb_vector_common.h"

static void observe(struct msgb *m, const char *api, int arg, unsigned int scenario, unsigned int step)
{
    start_record(m,api,arg,scenario,step);
    snapshot(m,"before");
    /* Modified locals are volatile across longjmp; the allocated msgb remains valid. */
    volatile long result = -1;
    volatile int panic = 0;
    if (setjmp(panic_env)) panic = 1;
    else {
        if (!strcmp(api,"put")) result = msgb_put(m,arg)-m->head;
        else if (!strcmp(api,"get")) result = msgb_get(m,arg)-m->head;
        else if (!strcmp(api,"push")) result = msgb_push(m,arg)-m->head;
        else if (!strcmp(api,"pull")) result = msgb_pull(m,arg)-m->head;
        else if (!strcmp(api,"trim")) result = msgb_trim(m,arg);
        else if (!strcmp(api,"reserve")) msgb_reserve(m,arg);
        else if (!strcmp(api,"reset")) msgb_reset(m);
        else if (strcmp(api,"alloc") && strcmp(api,"invariant")) abort();
    }
    printf(",\"panic\":%d,\"return_code\":%ld",panic,result);
    snapshot(m,"after"); putchar('}');
}
int main(void)
{
    if (CHAR_BIT != 8 || sizeof(unsigned int)*CHAR_BIT != 32 || sizeof(unsigned long)*CHAR_BIT != 64 || sizeof(void*)*CHAR_BIT != 64) return 1;
    osmo_set_panic_handler(caught_panic);
    puts("[");
    const unsigned int capacities[]={0,1,2,8,16};
    const char *apis[]={"put","get","push","pull","trim","reserve","reset","invariant"};
    unsigned int scenario=0;
    for (unsigned int ci=0;ci<5;ci++) {
        unsigned int cap=capacities[ci];
        struct msgb *allocated=msgb_alloc_c(NULL,cap,"allocation");
        if (!allocated) abort();
        observe(allocated,"alloc",0,scenario++,0); msgb_free(allocated);
        for (unsigned int d=0;d<=cap;d++)
        for (unsigned int len=0;len<=cap-d;len++)
        for (unsigned int meta=0;meta<3;meta++)
        for (unsigned int ai=0;ai<8;ai++) {
            if (ci>=3 && d!=0 && d!=cap/2 && d!=cap) continue;
            if (ci>=3 && len!=0 && len!=(cap-d)/2 && len!=cap-d) continue;
            for (int arg=0;arg<=(int)cap+1;arg++) {
                const char *api=apis[ai];
                if (ai>=6 && arg!=0) continue;
                if (ai==5 && (len || arg>(int)(cap-d))) continue;
                /* trim does not account for headroom. Exclude its unsafe pointer arithmetic. */
                if (ai==4 && arg>(int)(cap-d) && arg<=(int)cap) continue;
                struct msgb *m=prepared(cap,d,len,meta);
                observe(m,api,arg,scenario++,0); msgb_free(m);
            }
            if (ai==4) { struct msgb *m=prepared(cap,d,len,meta); observe(m,"trim",-1,scenario++,0); msgb_free(m); }
        }
    }
    /* Every null/non-null layer combination, ordered and reversed, including
     * markers in headroom, active bytes, and unused tailroom. */
    for (unsigned int meta=3;meta<35;meta++) {
        struct msgb *m=prepared(16,4,4,meta);
        observe(m,"invariant",0,scenario++,0); msgb_free(m);
    }
    /* A sequence retains prior backing bytes and stale layer markers. */
    struct msgb *m=prepared(16,0,0,1);
    const char *seq[]={"reserve","put","push","pull","get","trim","reset","put"};
    int args[]={4,8,2,3,2,5,0,16};
    for (unsigned int i=0;i<8;i++) observe(m,seq[i],args[i],scenario,i);
    msgb_free(m); scenario++;
    /* Width boundary uses a few targeted calls rather than a quadratic sweep. */
    m=msgb_alloc_c(NULL,65535,"width"); if (!m) abort();
    observe(m,"alloc",0,scenario,0);
    observe(m,"put",65535,scenario,1);
    observe(m,"put",1,scenario,2);
    observe(m,"get",65535,scenario,3);
    observe(m,"reserve",65535,scenario,4);
    observe(m,"push",65535,scenario,5);
    observe(m,"pull",65535,scenario,6);
    observe(m,"reset",0,scenario,7);
    msgb_free(m);
    puts("\n]"); osmo_set_panic_handler(NULL); return 0;
}
