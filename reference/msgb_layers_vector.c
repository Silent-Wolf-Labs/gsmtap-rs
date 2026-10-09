/* Direct-call typed/layer observations. Never synthesize expected bytes. */
#include "msgb_vector_common.h"

static void observe_layers(struct msgb *m, const char *api, int64_t arg, int seed, unsigned int scenario, unsigned int step)
{
    start_record(m,api,arg,scenario,step);
    printf(",\"seed\":%d",seed);
    printf(",\"endian16_sha256\":\"927d65fa34c9cc59cf777fadabea4a7d370f934eebf85f24c0c25a540a8df895\"");
    printf(",\"endian32_sha256\":\"dac4631b3a38d3fba61af5c739cf1823d2a5cd2f460a511aeef04412601a4cf0\"");
    printf(",\"endian_template_sha256\":\"35109704fc81fd83d2a75072f1efe1df8fafbc1775c1a5114d2fb892441f02a1\"");
    printf(",\"endian_rule_sha256\":\"3474af2d135857e1da1814fbae33aff8b24292d7d740041d4454cdf72c49e83e\"");
    printf(",\"bits_header_sha256\":\"922f568b46a84f36a2c6208308a71ceb505088b217e7105f254725f029c297f7\"");
    printf(",\"utils_header_sha256\":\"3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d\"");
    snapshot(m,"before");
    volatile long result = -1;
    volatile int panic = 0;
    if (setjmp(panic_env)) panic = 1;
    else {
        if (!strcmp(api,"put_u8")) msgb_put_u8(m,(uint8_t)arg);
        else if (!strcmp(api,"put_u16")) msgb_put_u16(m,(uint16_t)arg);
        else if (!strcmp(api,"put_u32")) msgb_put_u32(m,(uint32_t)arg);
        else if (!strcmp(api,"push_u8")) msgb_push_u8(m,(uint8_t)arg);
        else if (!strcmp(api,"push_u16")) msgb_push_u16(m,(uint16_t)arg);
        else if (!strcmp(api,"push_u32")) msgb_push_u32(m,(uint32_t)arg);
        else if (!strcmp(api,"get_u8")) result = msgb_get_u8(m);
        else if (!strcmp(api,"get_u16")) result = msgb_get_u16(m);
        else if (!strcmp(api,"get_u32")) result = msgb_get_u32(m);
        else if (!strcmp(api,"pull_u8")) result = msgb_pull_u8(m);
        else if (!strcmp(api,"pull_u16")) result = msgb_pull_u16(m);
        else if (!strcmp(api,"pull_u32")) result = msgb_pull_u32(m);
        else if (!strcmp(api,"l1")) { unsigned char *p=msgb_l1(m); result=p ? p-m->head : -1; }
        else if (!strcmp(api,"l1len")) result=msgb_l1len(m);
        else if (!strcmp(api,"l2")) { unsigned char *p=msgb_l2(m); result=p ? p-m->head : -1; }
        else if (!strcmp(api,"l2len")) result=msgb_l2len(m);
        else if (!strcmp(api,"l3")) { unsigned char *p=msgb_l3(m); result=p ? p-m->head : -1; }
        else if (!strcmp(api,"l3len")) result=msgb_l3len(m);
        else if (!strcmp(api,"l4")) { unsigned char *p=msgb_l4(m); result=p ? p-m->head : -1; }
        else if (!strcmp(api,"l4len")) result=msgb_l4len(m);
        else if (!strcmp(api,"sms")) { unsigned char *p=msgb_sms(m); result=p ? p-m->head : -1; }
        else if (!strcmp(api,"pull_to_l2")) result=msgb_pull_to_l2(m)-m->head;
        else if (!strcmp(api,"pull_to_l3")) result=msgb_pull_to_l3(m)-m->head;
        else if (!strcmp(api,"l3trim")) result=msgb_l3trim(m,(int)arg);
        else if (!strcmp(api,"push_tl")) result=msgb_push_tl(m,(uint8_t)arg)-m->head;
        else if (!strcmp(api,"reset")) msgb_reset(m);
        else abort();
    }
    printf(",\"panic\":%d,\"return_code\":%ld",panic,result);
    snapshot(m,"after"); putchar('}');
}
static unsigned int scenario;
static void emit(const char *api, int64_t arg, unsigned int cap, unsigned int data, unsigned int len, unsigned int layer, int offset, int seed)
{
    struct msgb *m=prepared(cap,data,len,1);
    if ((uintptr_t)m->head % _Alignof(uint32_t)) abort();
    if (seed>=0) memset(m->head,seed,cap);
    if (layer) {
        unsigned char *p=offset < 0 ? NULL : m->head+offset;
        switch (layer) {case 1:m->l1h=p;break;case 2:m->l2h=p;break;case 3:m->l3h=p;break;case 4:m->l4h=p;break;default:abort();}
    }
    observe_layers(m,api,arg,seed,scenario++,0); msgb_free(m);
}
int main(void)
{
    if (CHAR_BIT != 8 || sizeof(unsigned int)*CHAR_BIT != 32 || sizeof(unsigned long)*CHAR_BIT != 64 || sizeof(void*)*CHAR_BIT != 64) return 1;
    osmo_set_panic_handler(caught_panic); puts("[");
    const unsigned int caps[]={0,1,2,3,4,5,8};
    const uint32_t values[]={0,1,0x7f,0x80,0xff,0x100,0x7fff,0x8000,0xffff,0x10000,0x12345678,0x80000000,0xffffffff};
    const char *typed[]={"put_u8","put_u16","put_u32","push_u8","push_u16","push_u32","get_u8","get_u16","get_u32","pull_u8","pull_u16","pull_u32"};
    for (unsigned int ci=0;ci<7;ci++) {
        unsigned int cap=caps[ci];
        for (unsigned int data=0;data<=cap;data++)
        for (unsigned int len=0;len<=cap-data;len++) {
            if (len!=0 && len!=(cap-data)/2 && len!=cap-data) continue;
            for (unsigned int ai=0;ai<12;ai++) {
                unsigned int width=1u << (ai%3);
                /* C push_u16/u32 cast to typed pointers. Successful C calls
                 * require aligned destinations even though byte stores follow. */
                if (ai>=3 && ai<6 && data>=width && data%width) continue;
                if (ai>=6) emit(typed[ai],0,cap,data,len,0,0,-1);
                else for (unsigned int vi=0;vi<13;vi++) {
                    uint32_t value=values[vi];
                    if (width==1 && value>UINT8_MAX) continue;
                    if (width==2 && value>UINT16_MAX) continue;
                    emit(typed[ai],value,cap,data,len,0,0,-1);
                }
            }
        }
    }
    for (int byte=0;byte<=255;byte++) {
        emit("put_u8",byte,8,4,0,0,0,-1);
        emit("push_u8",byte,8,4,0,0,0,-1);
        emit("get_u8",0,1,0,1,0,0,byte);
        emit("pull_u8",0,1,0,1,0,0,byte);
    }
    const char *access[]={"l1","l2","l3","l4","sms","l1len","l2len","l3len","l4len","pull_to_l2","pull_to_l3"};
    for (unsigned int data=0;data<=8;data+=2) {
        for (unsigned int len=0;len<=8-data;len++) {
            if (len!=0 && len!=(8-data)/2 && len!=8-data) continue;
            for (unsigned int ai=0;ai<11;ai++) {
                unsigned int layer=ai<4 ? ai+1 : ai==4 ? 4 : ai<9 ? ai-4 : ai==9 ? 2 : 3;
                for (int offset=-1;offset<=8;offset++) {
                    /* NULL pointer subtraction in pull-to-layer is undefined. */
                    if (ai>=9 && offset<0) continue;
                    emit(access[ai],0,8,data,len,layer,offset,-1);
                }
            }
        }
    }
    const int lengths[]={-10,-3,-1,0,1,2,4,6,8,9,10,INT_MIN,INT_MAX};
    for (unsigned int data=0;data<=8;data+=2)
    for (int offset=0;offset<=8;offset++)
    for (unsigned int ai=0;ai<13;ai++) {
        int64_t total=(int64_t)offset-data+lengths[ai];
        if (total<INT_MIN || total>INT_MAX) continue;
        if (total>8-(int64_t)data && total<=8) continue;
        emit("l3trim",lengths[ai],8,data,(8-data)/2,3,offset,-1);
    }
    const unsigned int tl_caps[]={1,2,4,260,512};
    const unsigned int tl_lens[]={0,1,254,255,256,257};
    const unsigned int tags[]={0,1,0x7f,0x80,0xff};
    for (unsigned int ci=0;ci<5;ci++) {
        unsigned int cap=tl_caps[ci];
        for (unsigned int data=0;data<=4 && data<=cap;data++) {
            for (unsigned int li=0;li<7;li++) {
                unsigned int len=li==6 ? cap-data : tl_lens[li];
                if (len>cap-data) continue;
                for (unsigned int ti=0;ti<5;ti++) emit("push_tl",tags[ti],cap,data,len,0,0,-1);
            }
        }
    }
    for (int tag=0;tag<=255;tag++) emit("push_tl",tag,260,2,256,0,0,-1);
    emit("push_tl",0xff,65535,2,65533,0,0,-1);
    struct msgb *m=prepared(16,8,0,1);
    const char *seq[]={"put_u32","put_u16","push_u32","get_u16","pull_u32","push_tl","pull_to_l3","get_u16","l3trim","reset","put_u8","get_u8","l3","l3len"};
    const int64_t args[]={0x01020304,0xff80,0x89abcdef,0,0,7,0,0,0,0,0xa5,0,0,0};
    for (unsigned int i=0;i<14;i++) observe_layers(m,seq[i],args[i],-1,scenario,i);
    msgb_free(m); puts("\n]"); osmo_set_panic_handler(NULL); return 0;
}
