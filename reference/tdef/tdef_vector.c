/* Direct calls to the required unmodified local TDef implementation. */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osmocom/core/tdef.h>
#include <osmocom/core/panic.h>

static void conversion(void)
{
    /* Every factor and both rounding/saturation boundaries, without computing outputs. */
    const unsigned long values[] = {0,1,2,59,60,61,999,1000,1001,59999,60000,60001,
        999999,1000000,1000001,59999999,60000000,60000001,
        ULONG_MAX/60-1,ULONG_MAX/60,ULONG_MAX/60+1,
        ULONG_MAX/1000-1,ULONG_MAX/1000,ULONG_MAX/1000+1,
        ULONG_MAX/60000-1,ULONG_MAX/60000,ULONG_MAX/60000+1,
        ULONG_MAX/1000000-1,ULONG_MAX/1000000,ULONG_MAX/1000000+1,
        ULONG_MAX/60000000-1,ULONG_MAX/60000000,ULONG_MAX/60000000+1,
        LONG_MAX,ULONG_MAX-1,ULONG_MAX};
    for (int from=0; from<=4; from++) for (int to=0; to<=4; to++)
    for (unsigned i=0; i<sizeof(values)/sizeof(values[0]); i++) {
        struct osmo_tdef get[] = {{.T=7,.unit=from,.val=values[i]}, {0}};
        struct osmo_tdef set[] = {{.T=7,.unit=to,.val=73}, {0}};
        unsigned long result = osmo_tdef_get(get,7,to,-1);
        int ret = osmo_tdef_set(set,7,values[i],from);
        printf("{\"type\":\"conversion\",\"from\":%d,\"to\":%d,\"value\":%lu,\"get\":%lu,\"ret\":%d,\"before\":73,\"after\":%lu}\n",
            from,to,values[i],result,ret,set[0].val);
    }
    /* Unknown same-unit, custom, and zero conversions are defined in C. */
    const int invalid[][2] = {{-1,-1},{5,5},{INT_MIN,INT_MIN},{INT_MAX,INT_MAX},
        {-1,3},{3,-1},{5,3},{3,5},{INT_MIN,3},{3,INT_MAX}};
    for (unsigned i=0; i<sizeof(invalid)/sizeof(invalid[0]); i++) for (int j=0; j<3; j++) {
        unsigned long val = j==0 ? 0 : j==1 ? 1 : ULONG_MAX;
        struct osmo_tdef get[] = {{.T=7,.unit=invalid[i][0],.val=val}, {0}};
        struct osmo_tdef set[] = {{.T=7,.unit=invalid[i][1],.val=73}, {0}};
        unsigned long result=osmo_tdef_get(get,7,invalid[i][1],-1);
        int ret=osmo_tdef_set(set,7,val,invalid[i][0]);
        printf("{\"type\":\"invalid_safe\",\"from\":%d,\"to\":%d,\"value\":%lu,\"get\":%lu,\"ret\":%d,\"before\":73,\"after\":%lu}\n",
            invalid[i][0],invalid[i][1],val,result,ret,set[0].val);
    }
    const int units[]={-1,0,1,2,3,4,5,INT_MIN,INT_MAX};
    for(unsigned a=0;a<9;a++) for(unsigned b=0;b<9;b++) {
        struct osmo_tdef get[]={{.T=7,.unit=units[a],.val=0},{0}};
        struct osmo_tdef set[]={{.T=7,.unit=units[b],.val=73},{0}};
        unsigned long result=osmo_tdef_get(get,7,units[b],-1);
        int ret=osmo_tdef_set(set,7,0,units[a]);
        printf("{\"type\":\"zero_units\",\"from\":%d,\"to\":%d,\"value\":0,\"get\":%lu,\"ret\":%d,\"before\":73,\"after\":%lu}\n",
            units[a],units[b],result,ret,set[0].val);
    }
}
static void tables(void)
{
    struct osmo_tdef table[]={
        {.T=0,.desc="",.default_val=5,.val=91},
        {.T=-7,.default_val=6,.val=92}, {.T=7,.default_val=7,.val=93},
        {.T=7,.default_val=8,.val=94}, {.T=INT_MIN,.default_val=9,.val=95},
        {.T=INT_MAX,.default_val=10,.val=96},
        {.unit=OSMO_TDEF_US,.val=97,.min_val=99,.max_val=100},
        {.T=123,.default_val=11,.val=98}, {0}};
    const int ids[]={0,-7,7,INT_MIN,INT_MAX,123,999};
    for(unsigned i=0;i<7;i++) {
        struct osmo_tdef *entry=osmo_tdef_get_entry(table,ids[i]);
        unsigned long val=osmo_tdef_get(table,ids[i],OSMO_TDEF_US,LONG_MAX);
        int ret=osmo_tdef_set(table,ids[i],42,OSMO_TDEF_S);
        printf("{\"type\":\"lookup\",\"id\":%d,\"index\":%ld,\"fallback\":%ld,\"get\":%lu,\"ret\":%d,\"values\":\"%lu %lu %lu %lu %lu %lu %lu %lu\"}\n",
            ids[i],entry ? (long)(entry-table) : -1L,LONG_MAX,val,ret,
            table[0].val,table[1].val,table[2].val,table[3].val,table[4].val,table[5].val,table[6].val,table[7].val);
    }
    osmo_tdefs_reset(table);
    printf("{\"type\":\"reset\",\"values\":\"%lu %lu %lu %lu %lu %lu %lu %lu\"}\n",
        table[0].val,table[1].val,table[2].val,table[3].val,table[4].val,table[5].val,table[6].val,table[7].val);
    for(int unit=-1;unit<=5;unit++) for(int fallback=0;fallback<=1;fallback++)
        printf("{\"type\":\"missing\",\"unit\":%d,\"fallback\":%d,\"get\":%lu,\"entry\":%d,\"ret\":%d}\n",unit,fallback,
            osmo_tdef_get(NULL,0,unit,fallback),osmo_tdef_get_entry(NULL,0)!=NULL,osmo_tdef_set(NULL,0,1,unit));
    osmo_tdefs_reset(NULL);
    /* Each sentinel field independently prevents termination; val/unit/bounds do not. */
    struct osmo_tdef traversal[]={ {.T=1}, {.default_val=1}, {.desc=""},
        {.unit=OSMO_TDEF_US,.val=99,.min_val=1,.max_val=2}, {.T=2}, {0} };
    struct osmo_tdef *t; int n=0;
    osmo_tdef_for_each(t,traversal) n++;
    printf("{\"type\":\"traversal\",\"entries\":%d,\"hidden\":%d}\n",n,osmo_tdef_get_entry(traversal,2)!=NULL);
    struct osmo_tdef_group groups[]={ {.tdefs=traversal}, {.name="",.tdefs=&traversal[3]},
        {.name="stop",.desc="nonempty"}, {.tdefs=traversal}, {0} };
    struct osmo_tdef_group *g; n=0;
    osmo_tdef_groups_for_each(g,groups) n++;
    printf("{\"type\":\"groups\",\"entries\":%d}\n",n);
    n=0;osmo_tdef_for_each(t,NULL) n++;
    printf("{\"type\":\"traversal_null\",\"entries\":%d}\n",n);
    n=0;osmo_tdef_groups_for_each(g,NULL) n++;
    printf("{\"type\":\"groups_null\",\"entries\":%d}\n",n);
}
static void ranges(void)
{
    const unsigned long bounds[][2]={{0,0},{0,1},{1,1},{5,10},{5,0},{10,5},{ULONG_MAX,0},{ULONG_MAX,ULONG_MAX}};
    for(unsigned i=0;i<8;i++) {
        unsigned long min=bounds[i][0], max=bounds[i][1];
        const unsigned long values[]={0,1,min?min-1:0,min,min==ULONG_MAX?min:min+1,
            max?max-1:0,max,max==ULONG_MAX?max:max+1,ULONG_MAX};
        for(unsigned j=0;j<9;j++) for(int source=0;source<5;source++) {
            struct osmo_tdef t[]={ {.T=1,.unit=OSMO_TDEF_S,.val=73,.min_val=min,.max_val=max}, {0} };
            int in=osmo_tdef_val_in_range(t,values[j]);
            int ret=osmo_tdef_set(t,1,values[j],source);
            printf("{\"type\":\"range\",\"min\":%lu,\"max\":%lu,\"value\":%lu,\"from\":%d,\"in\":%d,\"ret\":%d,\"before\":73,\"after\":%lu}\n",min,max,values[j],source,in,ret,t[0].val);
        }
        for(unsigned j=0;j<9;j++) {
            struct osmo_tdef t[]={ {.T=1,.default_val=values[j],.val=73,.min_val=min,.max_val=max}, {0} };
            if(osmo_tdef_val_in_range(t,values[j])) {
                osmo_tdefs_reset(t);
                printf("{\"type\":\"reset_range\",\"min\":%lu,\"max\":%lu,\"default\":%lu,\"before\":73,\"after\":%lu}\n",min,max,values[j],t[0].val);
            }
        }
        for(unsigned cap=0;cap<=50;cap++) {
            unsigned char buf[64]; memset(buf,0xa5,sizeof(buf));
            struct osmo_tdef t={.min_val=min,.max_val=max};
            int ret=osmo_tdef_range_str_buf((char*)buf,cap,&t);
            printf("{\"type\":\"format\",\"min\":%lu,\"max\":%lu,\"capacity\":%u,\"ret\":%d,\"initial\":\"",min,max,cap,ret);
            for(unsigned j=0;j<64;j++) printf("a5");
            printf("\",\"final\":\"");for(unsigned j=0;j<64;j++) printf("%02x",buf[j]);printf("\"}\n");
        }
    }
}
static void names_and_states(void)
{
    const int units[]={0,1,2,3,4,-1,5,INT_MIN,INT_MAX};
    for(unsigned i=0;i<9;i++) printf("{\"type\":\"name\",\"unit\":%d,\"name\":\"%s\"}\n",units[i],osmo_tdef_unit_name(units[i]));
    for(unsigned i=0;i<6;i++) printf("{\"type\":\"name_table\",\"index\":%u,\"unit\":%u,\"present\":%d,\"name\":\"%s\"}\n",
        i,osmo_tdef_unit_names[i].value,osmo_tdef_unit_names[i].str!=NULL,osmo_tdef_unit_names[i].str?osmo_tdef_unit_names[i].str:"");
    const int ids[]={0,-7,7,INT_MIN,INT_MAX};
    for(unsigned state=0;state<32;state++) for(unsigned i=0;i<5;i++) for(int keep=0;keep<2;keep++) {
        struct osmo_tdef_state_timeout table[32]={{0}};
        table[state].T=ids[i];table[state].keep_timer=keep;
        const struct osmo_tdef_state_timeout *t=osmo_tdef_get_state_timeout(state,table);
        printf("{\"type\":\"state\",\"state\":%u,\"id\":%d,\"keep\":%d,\"present\":%d,\"index\":%ld,\"out_id\":%d,\"out_keep\":%d}\n",
            state,ids[i],keep,t!=NULL,t?(long)(t-table):-1L,t?t->T:0,t?t->keep_timer:0);
    }
}
static struct osmo_tdef bad_reset[]={ {.T=1,.default_val=5,.val=91},
    {.T=2,.default_val=11,.val=92,.min_val=5,.max_val=10}, {.T=3,.default_val=7,.val=93}, {0} };
static struct osmo_tdef low_reset[]={ {.T=1,.default_val=5,.val=91},
    {.T=2,.default_val=7,.val=92}, {.T=3,.default_val=2,.val=93,.min_val=3,.max_val=10}, {0} };
static struct osmo_tdef inverted_reset[]={ {.T=1,.default_val=5,.val=91,.min_val=10,.max_val=2},
    {.T=2,.default_val=7,.val=92}, {.T=3,.default_val=7,.val=93}, {0} };
static struct osmo_tdef *reset_snapshot=bad_reset;
static void panic_report(const char *fmt, va_list args)
{
    vfprintf(stderr,fmt,args);
    printf("{\"type\":\"panic_snapshot\",\"values\":\"%lu %lu %lu\"}\n",reset_snapshot[0].val,reset_snapshot[1].val,reset_snapshot[2].val);
    fflush(stdout); _Exit(77);
}
int main(int argc,char **argv)
{
    if(sizeof(unsigned long)!=8 || sizeof(long)!=8 || sizeof(int)!=4 || CHAR_BIT!=8) return 65;
    if(argc==2) {
        if(!strcmp(argv[1],"reset-invalid")) {osmo_set_panic_handler(panic_report);osmo_tdefs_reset(bad_reset);return 66;}
        if(!strcmp(argv[1],"reset-low") || !strcmp(argv[1],"reset-inverted")) {
            reset_snapshot=!strcmp(argv[1],"reset-low")?low_reset:inverted_reset;
            osmo_set_panic_handler(panic_report);osmo_tdefs_reset(reset_snapshot);return 66;
        }
        if(!strcmp(argv[1],"missing-negative")) {osmo_tdef_get(NULL,123,OSMO_TDEF_S,-1);return 66;}
        if(!strcmp(argv[1],"state-32") || !strcmp(argv[1],"state-max")) {
            struct osmo_tdef_state_timeout t[32]={{0}};
            osmo_tdef_get_state_timeout(!strcmp(argv[1],"state-32")?32:UINT32_MAX,t);return 66;
        }
        if(!strcmp(argv[1],"format-null")) {struct osmo_tdef t={0};osmo_tdef_range_str_buf(NULL,0,&t);return 66;}
        int from,to,set;
        if(sscanf(argv[1],"divide-%d-%d-%d",&from,&to,&set)==3) {
            struct osmo_tdef t[]={{.T=1,.unit=from,.val=1},{0}};
            if(set) {struct osmo_tdef dest[]={{.T=1,.unit=to},{0}};osmo_tdef_set(dest,1,1,from);}
            else osmo_tdef_get(t,1,to,-1);
            return 66;
        }
        return 64;
    }
    if(argc!=3) return 64;
    printf("{\"type\":\"meta\",\"schema\":1,\"evidence\":\"exact-source\",\"revision\":\"unknown\",\"reference\":\"%s\",\"generator\":\"%s\",\"ulong_bits\":64,\"long_bits\":64,\"int_bits\":32}\n",argv[1],argv[2]);
    conversion();tables();ranges();names_and_states();return 0;
}
