/* Direct calls only: the required local time_cc, timer, clock, TDef and counter sources. */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <osmocom/core/time_cc.h>
#include <osmocom/core/tdef.h>
#include <osmocom/core/rate_ctr.h>
#include <osmocom/core/timer_compat.h>

/* Fault injection at libc boundary, never substitute libosmocore functions. */
static int fail_clock;
int __wrap_clock_gettime(clockid_t id, struct timespec *tp)
{
    (void)id; (void)tp;
    if (!fail_clock) abort(); /* golden reads must use the public override */
    errno = EIO;
    return -1;
}

static struct osmo_time_cc tc;
static struct rate_ctr ctr;
static struct osmo_tdef defs_us[] = {
    {.T=-23,.unit=OSMO_TDEF_US}, {.T=-24,.unit=OSMO_TDEF_US},
    {.T=-25,.unit=OSMO_TDEF_US}, {0}
};
static struct osmo_tdef defs_ms[] = {
    {.T=-23,.unit=OSMO_TDEF_MS}, {.T=-24,.unit=OSMO_TDEF_MS},
    {.T=-25,.unit=OSMO_TDEF_MS}, {0}
};
static struct osmo_tdef defs_bad[] = {{.T=-23,.unit=-1,.val=1},{0}};
static struct osmo_tdef *defs=defs_us;
static unsigned scenario, step;
static int work;
static void clocks(int64_t mono_sec, int64_t mono_nsec, int64_t wall_sec, int64_t wall_usec, int fail)
{
    fail_clock=fail;
    osmo_clock_override_enable(CLOCK_MONOTONIC, !fail);
    *osmo_clock_override_gettimespec(CLOCK_MONOTONIC)=(struct timespec){mono_sec,mono_nsec};
    osmo_gettimeofday_override=true;
    osmo_gettimeofday_override_time=(struct timeval){wall_sec,wall_usec};
}
static void emit(const char *op, int arg, int64_t ms, int64_t mn, int64_t ws, int64_t wu,
                 uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e)
{
    printf("{\"type\":\"trace\",\"scenario\":%u,\"step\":%u,\"op\":\"%s\",\"arg\":%d,"
           "\"mono_s\":%"PRId64",\"mono_ns\":%"PRId64",\"wall_s\":%"PRId64",\"wall_us\":%"PRId64",\"fail\":%d,"
           "\"a\":%"PRIu64",\"b\":%"PRIu64",\"c\":%"PRIu64",\"d\":%"PRIu64",\"e\":%"PRIu64","
           "\"gran\":%"PRIu64",\"threshold\":%"PRIu64",\"forget\":%"PRIu64",\"sink\":%d,\"bindings\":%d,"
           "\"flag\":%d,\"total\":%"PRIu64",\"start\":%"PRIu64",\"last\":%"PRIu64","
           "\"sum\":%"PRIu64",\"reported\":%"PRIu64",\"counter\":%"PRIu64",\"previous\":%"PRIu64","
           "\"active\":%d,\"deadline_s\":%ld,\"deadline_us\":%ld,\"timers\":%d,\"work\":%d}\n",
           scenario,step++,op,arg,ms,mn,ws,wu,fail_clock,a,b,c,d,e,
           tc.cfg.gran_usec,tc.cfg.round_threshold_usec,tc.cfg.forget_sum_usec,tc.cfg.rate_ctr!=NULL,tc.cfg.T_defs!=NULL,
           tc.flag_state,tc.total_sum,tc.start_time,tc.last_counted_time,tc.sum,tc.reported_sum,ctr.current,ctr.previous,
           osmo_timer_pending(&tc.timer),(long)tc.timer.timeout.tv_sec,(long)tc.timer.timeout.tv_usec,osmo_timers_check(),work);
    /* Only current may be changed by the real rate_ctr_add(). */
    if (ctr.previous!=19 || ctr.intv[0].last!=29 || ctr.intv[3].rate!=39) abort();
}
static void event(const char *op,int arg,int64_t ms,int64_t mn,int64_t ws,int64_t wu,
                  int fail,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e)
{
    clocks(ms,mn,ws,wu,fail); work=-1;
    if (!strcmp(op,"init")) {
        osmo_time_cc_init(&tc);
        tc.cfg=(struct osmo_time_cc_cfg){.gran_usec=a,.round_threshold_usec=b,.forget_sum_usec=c,.rate_ctr=arg?&ctr:NULL};
    } else if (!strcmp(op,"flag")) osmo_time_cc_set_flag(&tc,arg);
    else if (!strcmp(op,"update")) work=osmo_timers_update();
    else if (!strcmp(op,"cleanup")) osmo_time_cc_cleanup(&tc);
    else if (!strcmp(op,"config")) {
        tc.cfg.gran_usec=a;tc.cfg.round_threshold_usec=b;tc.cfg.forget_sum_usec=c;tc.cfg.rate_ctr=arg?&ctr:NULL;
    } else if (!strcmp(op,"bindings")) {
        defs=arg==OSMO_TDEF_MS?defs_ms:defs_us;
        defs[0].val=a;defs[1].val=b;defs[2].val=c;
        tc.cfg.T_defs=defs;tc.cfg.T_gran=-23;tc.cfg.T_round_threshold=-24;tc.cfg.T_forget_sum=-25;
    } else if (!strcmp(op,"tset")) {
        work=osmo_tdef_set(defs,-23,a,arg);
        if(osmo_tdef_set(defs,-24,b,arg)||osmo_tdef_set(defs,-25,c,arg)) abort();
    } else if (!strcmp(op,"seed")) {
        /* Public struct storage for isolated representable edge states; no model. */
        tc.start_time=a;tc.last_counted_time=b;tc.sum=c;tc.reported_sum=d;tc.total_sum=e;tc.flag_state=arg;
    } else if (!strcmp(op,"counter")) ctr.current=a;
    else abort();
    emit(op,arg,ms,mn,ws,wu,a,b,c,d,e);
}
#define EV(op,arg,mono,wall) event(op,arg,(mono)/1000000,((mono)%1000000)*1000,(wall)/1000000,(wall)%1000000,0,0,0,0,0,0)
static void begin(uint64_t gran,uint64_t threshold,uint64_t forget,int sink)
{
    if(scenario) osmo_time_cc_cleanup(&tc);
    scenario++;step=0;ctr=(struct rate_ctr){.previous=19,.intv={{.last=29},{0},{0},{.rate=39}}};
    event("init",sink,1,0,100,0,0,gran,threshold,forget,0,0);
}
static void end(void) { EV("cleanup",0,9000000,900000000); }
static void matrix(void)
{
    const uint64_t grans[]={0,1,2,3,10,1000000,UINT64_C(2147483648000000),UINT64_MAX};
    const uint64_t thresholds[]={0,1,2,5,500000,1000000,UINT64_MAX};
    const uint64_t forgets[]={0,1,1000000,UINT64_MAX};
    for(unsigned g=0;g<8;g++)for(unsigned r=0;r<7;r++)for(unsigned f=0;f<4;f++)for(int sink=0;sink<2;sink++) {
        begin(grans[g],thresholds[r],forgets[f],sink);
        EV("flag",0,1000000,100000000);
        EV("flag",1,1000000,100000000);
        EV("flag",1,1000001,100000001); /* unchanged flag does not count */
        EV("update",0,1499999,100499999);
        EV("update",0,1500000,100500000);
        EV("update",0,3750001,102750001); /* delayed wakeup */
        EV("flag",0,4000000,103000000);
        EV("update",0,4000001,103000001);
        EV("update",0,5000000,104000000);
        EV("flag",1,5000001,104000001);
        EV("update",0,6000000,105000000);
        end();
    }
}
static void edge_traces(void)
{
    begin(1000000,0,0,1); /* independent clocks */
    EV("flag",1,1000000,100000000);
    EV("update",0,3000000,100000000); /* monotonic advanced, wall frozen */
    EV("update",0,3000000,100500000);
    EV("update",0,3000000,50000000); /* wall backwards */
    EV("update",0,3000000,105000000); /* wall forwards, monotonic frozen */
    EV("flag",0,3500000,105000000);
    EV("update",0,4500000,106000000); /* prior active timer gets one last callback */
    end();

    begin(10,1,100,1); /* cleanup/reuse preserves cfg and sink, resets timer storage */
    EV("flag",1,1000000,100000000);EV("update",0,1000020,100000020);
    EV("cleanup",0,1000020,100000020);EV("cleanup",0,1000020,100000020);
    EV("flag",1,2000000,200000000);EV("flag",0,2000005,200000005);end();

    begin(0,0,0,1); /* timestamp zero stays the unstarted sentinel */
    EV("flag",1,0,0);EV("flag",1,0,0);EV("flag",0,1,1);
    EV("flag",1,2,2);EV("update",0,500002,500002);end();

    begin(1000000,0,0,1); /* real libc fault via --wrap, osmo shim unchanged */
    event("flag",1,0,0,100,0,1,0,0,0,0,0);
    event("flag",1,0,0,100,1,1,0,0,0,0,0);
    EV("flag",1,1000000,100000002);
    event("update",0,0,0,101,0,1,0,0,0,0,0);end();

    begin(10,0,0,1); /* direct config changes do not forget old sums */
    EV("flag",1,1000000,100000000);EV("flag",0,1000004,100000004);
    event("config",0,1,4000,100,4,0,3,UINT64_MAX,9,0,0);
    EV("flag",1,1000005,100000005);EV("update",0,1000020,100000020);
    event("config",1,1,20000,100,20,0,0,0,0,0,0);
    EV("flag",0,1000021,100000021);end();

    begin(100,0,0,1); /* TDef gran/forget changes reset sums at callback observation */
    event("bindings",OSMO_TDEF_MS,1,0,100,0,0,1,0,10,0,0);
    EV("flag",1,1000000,100000000);EV("update",0,1000600,100000600);
    event("tset",OSMO_TDEF_US,1,600000,100,600,0,2000,1900,100,0,0);
    EV("flag",1,1000700,100000700); /* unchanged flag does not refresh TDefs */
    EV("update",0,1001600,100001600);EV("update",0,1003500,100003500);
    event("tset",OSMO_TDEF_US,1,3500000,100,3500,0,2000,1,0,0,0); /* threshold only/disable forget */
    EV("update",0,1005500,100005500);EV("flag",0,1006000,100006000);
    event("tset",OSMO_TDEF_US,1,6000000,100,6000,0,2000,1,1,0,0); /* enabling forget resets */
    EV("update",0,1008000,100008000);end();

    begin(10,0,0,1); /* no sink still advances reported_sum; later attach no catchup */
    event("config",0,1,0,100,0,0,10,0,0,0,0);
    EV("flag",1,1000000,100000000);EV("update",0,1000015,100000015);
    event("config",1,1,15000,100,15,0,10,0,0,0,0);
    EV("update",0,1000025,100000025);end();

    /* Wrapping sums/time deltas/counter increments and representable i32 narrowing. */
    const uint64_t seeds[][5]={
        {1,1000000,UINT64_MAX-2,UINT64_MAX-3,UINT64_MAX-1},
        {1,2000000,0,0,0},
        {1,1,0,0,0},
        {1,1,UINT64_MAX,0,UINT64_MAX},
    };
    for(unsigned i=0;i<4;i++) {
        begin(i==2?1:10,1,0,1);
        event("seed",1,1,0,100,0,0,seeds[i][0],seeds[i][1],seeds[i][2],seeds[i][3],seeds[i][4]);
        event("counter",0,1,0,100,0,0,UINT64_MAX-1,0,0,0,0);
        EV("flag",0,1000005,100000005);end();
    }
    begin(UINT64_C(2147483648000000),UINT64_MAX,0,1); /* seconds narrow to INT_MIN */
    EV("flag",1,1000000,100000000);EV("update",0,1000000,100000000);end();
    begin(UINT64_C(4294967296000001),UINT64_MAX,0,1); /* seconds narrow to 0, usec=1 */
    EV("flag",1,1000000,100000000);EV("update",0,1000001,100000001);end();

    begin(UINT64_MAX-1000000,UINT64_MAX,0,1); /* next_inc == UINT64_MAX: no timer */
    EV("flag",1,1000000,100000000);EV("update",0,2000000,101000000);end();
    begin(10,5,0,1); /* UINT64_MAX keeps previously active timer */
    EV("flag",1,1000000,100000000);
    event("config",1,1,0,100,0,0,UINT64_MAX-1000000,UINT64_MAX,0,0,0);
    EV("flag",0,1000000,100000000);EV("flag",1,1000000,100000000);end();
    begin(10,1,UINT64_MAX-1000000,1); /* next_forget == UINT64_MAX */
    event("seed",1,1,0,100,0,0,1,1000000,1,1,1);
    EV("flag",0,1000000,100000000);end();
    begin(10,1,UINT64_MAX,1); /* forget deadline wraps, immediate callback */
    event("seed",1,1,0,100,0,0,1,1000000,1,1,1);
    EV("flag",0,1000000,100000000);EV("update",0,1000000,100000000);end();

    begin(1,0,0,1); /* n++ wraps from UINT64_MAX to zero when threshold defaults to zero */
    event("seed",1,1,0,100,0,0,1,1000000,UINT64_MAX,0,UINT64_MAX);
    EV("flag",0,1000000,100000000);end();
    begin(UINT64_MAX-1,1,0,1); /* rounded reported granularity multiplication wraps */
    event("seed",1,1,0,100,0,0,1,1000000,UINT64_MAX,0,UINT64_MAX);
    EV("flag",0,1000000,100000000);end();

    begin(10,1,0,1); /* signed raw timespec conversion is defined unsigned arithmetic */
    event("flag",1,-1,-1000,100,0,0,0,0,0,0,0);
    event("flag",0,-1,1000,100,1,0,0,0,0,0,0);end();
    begin(10,1,0,1); /* UINT64_MAX timestamps and multiplication wrapping */
    event("flag",1,18446744073709LL,551615000,100,0,0,0,0,0,0,0);
    event("flag",0,INT64_MAX,999999999,100,1,0,0,0,0,0,0);end();
}
int main(int argc,char **argv)
{
    if(sizeof(unsigned long)!=8||sizeof(long)!=8||sizeof(int)!=4||CHAR_BIT!=8) return 65;
    if(argc==2) {
        clocks(1,0,100,0,0);osmo_time_cc_init(&tc);
        if(!strcmp(argv[1],"null-init")){osmo_time_cc_init(NULL);return 66;}
        if(!strcmp(argv[1],"null-set")){osmo_time_cc_set_flag(NULL,true);return 66;}
        if(!strcmp(argv[1],"null-cleanup")){osmo_time_cc_cleanup(NULL);return 66;}
        tc.cfg.T_defs=defs;tc.cfg.T_gran=123;
        if(!strcmp(argv[1],"missing-tdef")){osmo_time_cc_set_flag(&tc,true);return 66;}
        tc.cfg.T_defs=defs_bad;tc.cfg.T_gran=-23;
        if(!strcmp(argv[1],"invalid-tdef-unit")){osmo_time_cc_set_flag(&tc,true);return 66;}
        return 64;
    }
    if(argc!=3) return 64;
    printf("{\"type\":\"meta\",\"schema\":1,\"evidence\":\"exact-source\",\"revision\":\"unknown\",\"reference\":\"%s\",\"generator\":\"%s\",\"ulong_bits\":64,\"long_bits\":64,\"int_bits\":32}\n",argv[1],argv[2]);
    matrix();edge_traces();osmo_time_cc_cleanup(&tc);return osmo_timers_check()?67:0;
}
