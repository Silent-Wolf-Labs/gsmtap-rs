/* Direct-call copy/area-resize observations, without unsafe C pointer cases. */
#include "msgb_vector_common.h"
#include <osmocom/core/logging.h>

static void absent_copy(void)
{
    printf(",\"copy_hex\":\"\",\"copy_data\":-1,\"copy_tail\":-1,\"copy_len\":-1,\"copy_layers\":\"\",\"copy_cb\":\"\",\"copy_dst\":-1,\"copy_lchan\":-1,\"copy_headlen\":-1,\"copy_headroom\":-1,\"copy_tailroom\":-1,\"copy_invariant\":-1,\"copy_length\":-1,\"copy_data_return\":-1");
}
static void observe_resize(struct msgb *m, const char *api, int arg, int area, int old_size, int new_size, unsigned int scenario, unsigned int step)
{
    start_record(m,api,arg,scenario,step);
    printf(",\"area\":%d,\"old_size\":%d,\"new_size\":%d",area,old_size,new_size);
    printf(",\"logging_source_sha256\":\"13a0adeda85a762695f4b8c338f5b272b81774a15780c218ac4ccd13b21b2d0d\",\"logging_header_sha256\":\"7454572bcd4655f9cd87d8fe64e87f10bc17c322a14669982dfb62bcff002c55\",\"utils_header_sha256\":\"3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d\",\"c_int_bits\":%zu",sizeof(int)*CHAR_BIT);
    snapshot(m,"before");
    volatile int panic=0;
    volatile int result=-1;
    struct msgb *volatile copy=NULL;
    if (setjmp(panic_env)) panic=1;
    else {
        if (!strcmp(api,"copy")) copy=msgb_copy(m,"copy");
        else if (!strcmp(api,"copy_c")) copy=msgb_copy_c(NULL,m,"copy");
        else if (!strcmp(api,"copy_resize")) copy=msgb_copy_resize(m,arg,"copy");
        else if (!strcmp(api,"copy_resize_c")) copy=msgb_copy_resize_c(NULL,m,arg,"copy");
        else if (!strcmp(api,"resize_area")) result=msgb_resize_area(m,m->head+area,old_size,new_size);
        else abort();
    }
    printf(",\"panic\":%d,\"return_code\":%d",panic,result);
    snapshot(m,"after");
    printf(",\"copy_present\":%d,\"copy_capacity\":%d,\"distinct\":%d",copy!=NULL,copy ? copy->data_len : -1,copy ? copy!=m && copy->head!=m->head : 0);
    if (copy) {snapshot(copy,"copy");msgb_free(copy);} else absent_copy();
    putchar('}');
}
static void resize_layers(struct msgb *m, int area, int old_size, int layout)
{
    unsigned char *post=m->head+area+old_size;
    if (layout==0) {m->l1h=m->data;m->l2h=m->head+area;m->l3h=post;m->l4h=m->tail;}
    else if (layout==1) m->l1h=m->l2h=m->l3h=m->l4h=post;
    else if (layout==2) {m->l1h=m->head;m->l2h=m->head+m->data_len/3;m->l3h=m->head+m->data_len/2;m->l4h=m->head+m->data_len;}
    else {m->l1h=m->tail;m->l2h=post;m->l3h=m->head+area;m->l4h=m->head;}
}
static int safe_resize(unsigned int cap, unsigned int data, unsigned int len, int area, int old_size, int new_size, int layout)
{
    /* Even panic cases must form valid pointers before the C checks execute. */
    int post=area+old_size;
    if (area<0 || area>(int)cap || post<0 || post>(int)cap) return 0;
    if (old_size<0 || new_size<0 || area<(int)data || post>(int)(data+len)) return 1;
    int delta=new_size-old_size;
    if (delta==0 || (int)len+delta>(int)cap) return 1;
    if ((int)len+delta>(int)(cap-data)) return 0;
    int offsets[4];
    if (layout==0) {offsets[0]=data;offsets[1]=area;offsets[2]=post;offsets[3]=data+len;}
    else if (layout==1) for(int i=0;i<4;i++) offsets[i]=post;
    else if (layout==2) {offsets[0]=0;offsets[1]=cap/3;offsets[2]=cap/2;offsets[3]=cap;}
    else {offsets[0]=data+len;offsets[1]=post;offsets[2]=area;offsets[3]=0;}
    for(int i=0;i<4;i++) if(offsets[i]>=post && (offsets[i]+delta<0 || offsets[i]+delta>(int)cap)) return 0;
    return 1;
}
int main(void)
{
    if (CHAR_BIT!=8 || sizeof(int)*CHAR_BIT!=32 || sizeof(unsigned int)*CHAR_BIT!=32 || sizeof(unsigned long)*CHAR_BIT!=64 || sizeof(void*)*CHAR_BIT!=64) return 1;
    /* Copy failures log through the actual local core library; no targets means no diagnostics. */
    if (log_init(NULL,NULL)) return 1;
    osmo_set_panic_handler(caught_panic);puts("[");
    unsigned int scenario=0;
    const unsigned int caps[]={0,1,2,8,16};
    const char *copies[]={"copy","copy_c","copy_resize","copy_resize_c"};
    for (unsigned int ci=0;ci<5;ci++) {
        unsigned int cap=caps[ci];
        for (unsigned int data=0;data<=cap;data++) {
            if (data!=0 && data!=cap/2 && data!=cap) continue;
            for (unsigned int len=0;len<=cap-data;len++) {
                if (len!=0 && len!=(cap-data)/2 && len!=cap-data) continue;
                for (int mask=0;mask<=15;mask+=15)
                for (unsigned int ai=0;ai<4;ai++)
                for (unsigned int size=0;size<=cap+2;size++) {
                    if (ai<2 && size!=cap) continue;
                    struct msgb *m=prepared(cap,data,len,3+mask);
                    unsigned char *layers[]={m->l1h,m->l2h,m->l3h,m->l4h};
                    int safe=1;
                    if (size>=data+len) for(int i=0;i<4;i++) if(layers[i] && layers[i]-m->head>(int)size) safe=0;
                    if (safe) observe_resize(m,copies[ai],size,0,0,0,scenario++,0);
                    msgb_free(m);
                }
            }
        }
    }
    /* All optional-layer masks, ordered and reversed. */
    for(int meta=3;meta<35;meta++) for(unsigned int ai=0;ai<4;ai++) {
        struct msgb *m=prepared(8,2,3,meta);
        observe_resize(m,copies[ai],8,0,0,0,scenario++,0);msgb_free(m);
    }
    const unsigned int resize_caps[]={0,1,2,4,8};
    for(unsigned int ci=0;ci<5;ci++) {
        unsigned int cap=resize_caps[ci];
        for(unsigned int data=0;data<=cap;data++) {
            if(data!=0 && data!=cap/2 && data!=cap) continue;
            for(unsigned int len=0;len<=cap-data;len++) {
                if(len!=0 && len!=(cap-data)/2 && len!=cap-data) continue;
                for(int area=0;area<=(int)cap;area++)
                for(int old_size=0;old_size<=(int)cap-area;old_size++)
                for(int new_size=0;new_size<=(int)cap+1;new_size++) {
                    if(cap==8 && new_size!=0 && new_size!=old_size && new_size!=old_size+1 && new_size!=(int)cap+1) continue;
                    for(int layout=0;layout<4;layout++) {
                        if(!safe_resize(cap,data,len,area,old_size,new_size,layout)) continue;
                        struct msgb *m=prepared(cap,data,len,1);resize_layers(m,area,old_size,layout);
                        observe_resize(m,"resize_area",0,area,old_size,new_size,scenario++,0);msgb_free(m);
                    }
                }
            }
        }
    }
    for(int area=1;area<=8;area++) for(int which=0;which<2;which++) {
        int old_size=which ? 0 : -1;int new_size=which ? -1 : 0;
        struct msgb *m=prepared(8,2,4,1);resize_layers(m,area,old_size,0);
        observe_resize(m,"resize_area",0,area,old_size,new_size,scenario++,0);msgb_free(m);
    }
    struct msgb *m=prepared(16,2,8,1);resize_layers(m,4,2,0);
    const char *seq[]={"resize_area","resize_area","copy_resize","resize_area","resize_area","resize_area","copy","resize_area","copy_resize_c"};
    const int areas[]={4,8,0,2,6,9,0,2,0};
    const int olds[]={2,2,0,0,4,0,0,7,0};
    const int news[]={4,1,0,2,0,0,0,0,0};
    for(unsigned int i=0;i<9;i++) observe_resize(m,seq[i],!strcmp(seq[i],"resize_area") ? 0 : 16,areas[i],olds[i],news[i],scenario,i);
    msgb_free(m);scenario++;
    m=prepared(65535,1,65534,1);resize_layers(m,1,1,0);
    observe_resize(m,"resize_area",0,1,1,0,scenario,0);
    observe_resize(m,"resize_area",0,1,0,1,scenario,1);
    observe_resize(m,"copy_resize",65535,0,0,0,scenario,2);
    msgb_free(m);scenario++;
    m=prepared(65535,1,4,3);observe_resize(m,"copy_resize_c",8,0,0,0,scenario,0);msgb_free(m);
    puts("\n]");osmo_set_panic_handler(NULL);log_fini();return 0;
}
