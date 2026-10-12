/* Standalone FSM execution/termination evidence; no hierarchy scenarios. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <osmocom/core/fsm.h>
#include <osmocom/core/talloc.h>
static struct osmo_fsm fsm;
static unsigned scenario, step, mode;
static int nested;
static unsigned count(void) {
    unsigned n=0; struct osmo_fsm_inst *fi;
    llist_for_each_entry(fi,&fsm.instances,list) ++n;
    return n;
}
static void record(const char *op, int64_t arg, int rc) {
    struct osmo_fsm_inst *fi=osmo_fsm_inst_find_by_id(&fsm,"node");
    struct osmo_fsm_inst *name=osmo_fsm_inst_find_by_name(&fsm,"Evidence(node)");
    printf("%u\t%u\t%s\t%lld\t%d\t%d\t%d\t%d\t%ld\t%ld\t%d\t%d\t%d\t%u\n",
        scenario,step++,op,(long long)arg,rc,fi?(int)fi->state:-1,fi?fi->T:0,
        fi?osmo_timer_pending(&fi->timer):0,fi?(long)fi->timer.timeout.tv_sec:0,
        fi?(long)fi->timer.timeout.tv_usec:0,fi?fi->proc.terminating:0,!!fi,!!name,count());
}
static void action(struct osmo_fsm_inst *fi,uint32_t event,void *data) {
    record("action",event,0);
    if(mode==1) record("nested-chg",1,osmo_fsm_inst_state_chg(fi,1,0,7));
    if(mode==4) { osmo_fsm_inst_term(fi,OSMO_FSM_TERM_REGULAR,data); record("after-action-term",0,0); }
}
static void allstate(struct osmo_fsm_inst *fi,uint32_t event,void *data) {
    record("allstate",event,0);
    if(mode==2 && !nested) {nested=1; int rc=osmo_fsm_inst_dispatch(fi,0,data);nested=0;record("nested-dispatch",0,rc);}
}
static void onleave(struct osmo_fsm_inst *fi,uint32_t next) {
    record("leave",next,0);
    if(mode==7 && !nested) {nested=1;int rc=osmo_fsm_inst_state_chg(fi,2,0,9);nested=0;record("leave-nested-chg",2,rc);}
}
static void onenter(struct osmo_fsm_inst *fi,uint32_t prev) {
    record("enter",prev,0);
    if(mode==3) {osmo_fsm_inst_term(fi,OSMO_FSM_TERM_REQUEST,NULL);record("after-enter-term",0,0);}
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
    struct osmo_fsm_state states[3];
    memset(states,0,sizeof(states));
    for(unsigned i=0;i<3;i++) {states[i].name="state";states[i].in_event_mask=3;states[i].out_state_mask=7;states[i].action=action;states[i].onenter=onenter;states[i].onleave=onleave;}
    fsm=(struct osmo_fsm){.name="Evidence",.states=states,.num_states=3,.allstate_event_mask=2,.allstate_action=allstate,.pre_term=preterm,.cleanup=cleanup};
    osmo_fsm_register(&fsm);
    if(argc==2) {
        struct osmo_fsm_inst *fi=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"node");
        if(!strcmp(argv[1],"event31"))osmo_fsm_inst_dispatch(fi,31,NULL);
        else if(!strcmp(argv[1],"event32"))osmo_fsm_inst_dispatch(fi,32,NULL);
        else if(!strcmp(argv[1],"state31"))osmo_fsm_inst_state_chg(fi,31,0,0);
        else if(!strcmp(argv[1],"state32"))osmo_fsm_inst_state_chg(fi,32,0,0);
        else if(!strcmp(argv[1],"bad-current")){fi->state=3;osmo_fsm_inst_dispatch(fi,0,NULL);}
        else if(!strcmp(argv[1],"null-chg"))osmo_fsm_inst_state_chg(NULL,1,0,0);
        else if(!strcmp(argv[1],"null-term"))osmo_fsm_inst_term(NULL,OSMO_FSM_TERM_REQUEST,NULL);
        return 0;
    }
    if(argc!=3)return 2;
    printf("meta\t1\texact-source\tunknown\t%s\t%s\t64\t64\t32\n",argv[1],argv[2]);
    for(mode=0;mode<8;mode++) for(unsigned callbacks=0;callbacks<4;callbacks++) for(unsigned permit=0;permit<2;permit++) {
        ++scenario;step=0;nested=0;
        fsm.allstate_action=(callbacks&2)?allstate:NULL;
        for(unsigned i=0;i<3;i++) {states[i].action=(callbacks&1)?action:NULL;states[i].out_state_mask=permit?7:5;}
        struct osmo_fsm_inst *fi=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"node");
        record("init",mode*8+callbacks*2+permit,0);
        record("null-dispatch",0,osmo_fsm_inst_dispatch(NULL,0,NULL));
        const unsigned events[]={0,1,2,30};
        for(unsigned i=0;i<4;i++) {
            fi=osmo_fsm_inst_find_by_id(&fsm,"node");if(!fi)break;
            int rc=osmo_fsm_inst_dispatch(fi,events[i],NULL);record("dispatch",events[i],rc);
        }
        const unsigned targets[]={1,2,2,0,30};
        for(unsigned i=0;i<5;i++) {
            fi=osmo_fsm_inst_find_by_id(&fsm,"node");if(!fi)break;
            int rc=osmo_fsm_inst_state_chg(fi,targets[i],0,13);record("chg",targets[i],rc);
        }
        fi=osmo_fsm_inst_find_by_id(&fsm,"node");
        if(fi){osmo_fsm_inst_term(fi,scenario%5,NULL);record("term",scenario%5,0);}
        record("removed",0,0);
    }
    osmo_fsm_unregister(&fsm);return 0;
}
