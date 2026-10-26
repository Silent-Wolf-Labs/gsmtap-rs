/* Standalone FSM execution/termination evidence; no hierarchy scenarios. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <osmocom/core/fsm.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/tdef.h>
#include <limits.h>
static struct osmo_fsm fsm;
static unsigned scenario, step, mode;
static unsigned kind;
static uint64_t input0,input2;
static int64_t input1,input3;
static unsigned count(void) {
    unsigned n=0; struct osmo_fsm_inst *fi;
    llist_for_each_entry(fi,&fsm.instances,list) ++n;
    return n;
}
static void record(const char *op, int64_t arg, int rc) {
    struct osmo_fsm_inst *fi=osmo_fsm_inst_find_by_id(&fsm,"node");
    struct osmo_fsm_inst *name=osmo_fsm_inst_find_by_name(&fsm,"Evidence(node)");
    printf("%u\t%u\t%s\t%lld\t%d\t%d\t%d\t%d\t%ld\t%ld\t%d\t%d\t%d\t%u\t%llu\t%lld\t%llu\t%lld\t%ld\t%ld\n",
        scenario,step++,op,(long long)arg,rc,fi?(int)fi->state:-1,fi?fi->T:0,
        fi?osmo_timer_pending(&fi->timer):0,fi?(long)fi->timer.timeout.tv_sec:0,
        fi?(long)fi->timer.timeout.tv_usec:0,fi?fi->proc.terminating:0,!!fi,!!name,count(),(unsigned long long)input0,(long long)input1,(unsigned long long)input2,(long long)input3,(long)osmo_gettimeofday_override_time.tv_sec,(long)osmo_gettimeofday_override_time.tv_usec);
}
static void onleave(struct osmo_fsm_inst *fi,uint32_t next) {
    record("leave",next,0);
    if(mode==1){fi->T=123;osmo_timer_schedule(&fi->timer,9,456);record("leave-rearm",0,0);}
    if(mode==2){osmo_timer_del(&fi->timer);record("leave-cancel",0,0);}
}
static void onenter(struct osmo_fsm_inst *fi,uint32_t prev){record("enter",prev,0);}
static int timer_cb(struct osmo_fsm_inst *fi) {
    int rc=kind==2?1:kind==3?-1:kind==4?2:0;
    record("timeout",fi->T,rc);
    if(kind==5){fi->T=77;osmo_timer_schedule(&fi->timer,1,0);record("timeout-rearm",0,0);}
    if(kind==6){osmo_fsm_inst_term(fi,OSMO_FSM_TERM_REGULAR,NULL);record("timeout-term",0,0);}
    return rc;
}
static void terminating_calls(struct osmo_fsm_inst *fi) {
    record("terminating-dispatch",0,osmo_fsm_inst_dispatch(fi,0,NULL));
    record("terminating-chg",1,osmo_fsm_inst_state_chg(fi,1,0,0));
    osmo_fsm_inst_term(fi,OSMO_FSM_TERM_ERROR,NULL);record("terminating-term",0,0);
}
static void preterm(struct osmo_fsm_inst *fi,enum osmo_fsm_term_cause cause) {
    record("preterm",cause,0); if(mode==5)terminating_calls(fi);
}
static void cleanup(struct osmo_fsm_inst *fi,enum osmo_fsm_term_cause cause) {
    record("cleanup",cause,0);if(mode==6)terminating_calls(fi);
}
static const struct log_info info={};
int main(int argc,char **argv) {
    log_init(&info,NULL);osmo_fsm_log_addr(false);
    osmo_gettimeofday_override=true;
    struct osmo_fsm_state states[3];memset(states,0,sizeof(states));
    for(unsigned i=0;i<3;i++){states[i].name="state";states[i].out_state_mask=7;states[i].onenter=onenter;states[i].onleave=onleave;}
    fsm=(struct osmo_fsm){.name="Evidence",.states=states,.num_states=3,.pre_term=preterm,.cleanup=cleanup};
    osmo_fsm_register(&fsm);
    if(argc==2){
        struct osmo_fsm_inst *fi=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"node");
        struct osmo_tdef_state_timeout map[32]={[1]={.T=23}};
        struct osmo_tdef defs[]={{.T=23,.val=1,.unit=99},{}};
        if(!strcmp(argv[1],"invalid-unit"))osmo_tdef_fsm_inst_state_chg(fi,1,map,defs,0);
        else if(!strcmp(argv[1],"missing-tdef"))osmo_tdef_fsm_inst_state_chg(fi,1,map,NULL,-1);
        else if(!strcmp(argv[1],"fallback-overflow"))osmo_tdef_fsm_inst_state_chg(fi,1,map,NULL,LONG_MAX);
        else if(!strcmp(argv[1],"null-map"))osmo_tdef_fsm_inst_state_chg(fi,1,NULL,defs,0);
        else if(!strcmp(argv[1],"map32"))osmo_tdef_fsm_inst_state_chg(fi,32,map,defs,0);
        return 0;
    }
    if(argc!=3)return 2;
    printf("meta\t1\texact-source\tunknown\t%s\t%s\t64\t64\t32\n",argv[1],argv[2]);
    const uint64_t delays[]={0,1,999,1000,1001,INT_MAX,(uint64_t)INT_MAX+1,(uint64_t)INT_MAX*1000+999,UINT64_MAX/1000+1,UINT64_MAX};
    for(unsigned form=0;form<5;form++)for(unsigned active=0;active<2;active++)for(unsigned permit=0;permit<2;permit++)for(unsigned v=0;v<10;v++)for(mode=0;mode<3;mode++){
        ++scenario;step=0;kind=scenario%7;input0=delays[v];input1=-23;input2=form;input3=active*2+permit;
        osmo_gettimeofday_override_time=(struct timeval){100,789};
        states[0].out_state_mask=permit?7:5;fsm.timer_cb=kind?timer_cb:NULL;
        struct osmo_fsm_inst *fi=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"node");
        if(active){fi->T=99;osmo_timer_schedule(&fi->timer,3,123);}record("init",mode,0);
        int rc;
        switch(form){
        case 0:rc=osmo_fsm_inst_state_chg(fi,1,input0,input1);break;
        case 1:rc=osmo_fsm_inst_state_chg_ms(fi,1,input0,input1);break;
        case 2:rc=osmo_fsm_inst_state_chg_keep_timer(fi,1);break;
        case 3:rc=osmo_fsm_inst_state_chg_keep_or_start_timer(fi,1,input0,input1);break;
        default:rc=osmo_fsm_inst_state_chg_keep_or_start_timer_ms(fi,1,input0,input1);break;
        }record("transition",form,rc);
        osmo_gettimeofday_override_time=(struct timeval){120,789};rc=osmo_timers_update();record("update",0,rc);
        fi=osmo_fsm_inst_find_by_id(&fsm,"node");if(fi){osmo_fsm_inst_term(fi,OSMO_FSM_TERM_REQUEST,NULL);record("term",1,0);}record("removed",0,0);
    }
    const uint64_t values[]={0,1,999,1000,1001,UINT64_MAX};
    const int ids[]={0,23,-23,99};
    for(unsigned unit=0;unit<5;unit++)for(unsigned v=0;v<6;v++)for(unsigned mapping=0;mapping<4;mapping++)for(unsigned keep=0;keep<2;keep++)for(unsigned active=0;active<2;active++){
        ++scenario;step=0;mode=0;kind=0;input0=values[v];input1=unit;input2=mapping;input3=keep*2+active;
        osmo_gettimeofday_override_time=(struct timeval){100,789};states[0].out_state_mask=7;fsm.timer_cb=NULL;
        struct osmo_tdef_state_timeout map[32]={[1]={.T=ids[mapping],.keep_timer=keep}};
        struct osmo_tdef defs[]={{.T=23,.val=input0,.unit=unit},{.T=-23,.val=input0,.unit=unit},{}};
        struct osmo_fsm_inst *fi=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"node");
        if(active){fi->T=99;osmo_timer_schedule(&fi->timer,3,123);}record("init",100,0);
        int rc=osmo_tdef_fsm_inst_state_chg(fi,1,map,defs,999);record("tdef",0,rc);
        osmo_gettimeofday_override_time=(struct timeval){120,789};rc=osmo_timers_update();record("update",0,rc);
        fi=osmo_fsm_inst_find_by_id(&fsm,"node");if(fi){osmo_fsm_inst_term(fi,OSMO_FSM_TERM_REQUEST,NULL);record("term",1,0);}record("removed",0,0);
    }
    osmo_fsm_unregister(&fsm);return 0;
}
