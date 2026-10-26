/* Hierarchy evidence. Callback snapshots never dereference removed instances. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <osmocom/core/fsm.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/tdef.h>
static struct osmo_fsm fsm;
static const char *ids[]={"p","a","b","g"};
static unsigned scenario,step,mode;
static struct osmo_fsm_inst *get(unsigned i){return osmo_fsm_inst_find_by_id(&fsm,ids[i]);}
static int number(struct osmo_fsm_inst *fi){if(!fi)return -1;for(int i=0;i<4;i++)if(get(i)==fi)return i;return -2;}
static void record(const char *op,int target,int64_t arg,int rc){
    printf("%u\t%u\t%s\t%d\t%lld\t%d",scenario,step++,op,target,(long long)arg,rc);
    for(unsigned i=0;i<4;i++){
        struct osmo_fsm_inst *fi=get(i),*child;unsigned children=0;
        if(fi)llist_for_each_entry(child,&fi->proc.children,proc.child){int c=number(child);if(c>=0)children|=1u<<c;}
        printf("\t%d\t%d\t%d\t%d\t%d\t%u\t%d\t%ld\t%ld",fi?(int)fi->state:-1,fi?fi->T:0,fi?fi->proc.terminating:0,!!fi,fi?number(fi->proc.parent):-1,children,fi?osmo_timer_pending(&fi->timer):0,fi?(long)fi->timer.timeout.tv_sec:0,fi?(long)fi->timer.timeout.tv_usec:0);
    }
    struct osmo_fsm_inst *fi;unsigned order=0;
    llist_for_each_entry(fi,&fsm.instances,list){int n=number(fi);if(n>=0)order=order*5+n+1;}
    printf("\t%u\n",order);
}
static void action(struct osmo_fsm_inst *fi,uint32_t event,void *data){record("action",number(fi),event,(intptr_t)data);}
static void enter(struct osmo_fsm_inst *fi,uint32_t prev){record("enter",number(fi),prev,0);}
static void leave(struct osmo_fsm_inst *fi,uint32_t next){record("leave",number(fi),next,0);}
static void preterm(struct osmo_fsm_inst *fi,enum osmo_fsm_term_cause cause){record("preterm",number(fi),cause,0);}
static void cleanup(struct osmo_fsm_inst *fi,enum osmo_fsm_term_cause cause){record("cleanup",number(fi),cause,0);}
static int timer_cb(struct osmo_fsm_inst *fi){
    int n=number(fi),rc=mode==0?1:0;record("timeout",n,fi->T,rc);
    struct osmo_fsm_inst *peer=get(n==1?0:1);
    if(mode==1&&peer){osmo_timer_del(&peer->timer);record("cancel-peer",n,0,0);}
    if(mode==2&&peer){osmo_fsm_inst_term(peer,OSMO_FSM_TERM_REQUEST,NULL);record("term-peer",n,0,0);}
    if(mode==3&&!get(3)){struct osmo_fsm_inst *g=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"g");g->T=44;osmo_timer_schedule(&g->timer,0,0);record("create-peer",n,0,0);}
    if(mode==4){osmo_timer_schedule(&fi->timer,0,0);record("rearm",n,0,0);}
    if(mode==5&&peer){int result=osmo_fsm_inst_dispatch(peer,2,(void*)55);record("dispatch-peer",n,0,result);}
    return rc;
}
static void app_cb(void *data){
    record("app",-1,0,0);
    if(mode==6&&get(1)){
        struct osmo_tdef defs[]={{.T=23,.val=1,.unit=OSMO_TDEF_MS},{}};
        struct osmo_tdef_state_timeout map[32]={[1]={.T=23}};
        int rc=osmo_tdef_set(defs,23,50,OSMO_TDEF_MS);record("app-tset",-1,50,rc);
        rc=osmo_tdef_fsm_inst_state_chg(get(1),1,map,defs,0);record("app-tdef",-1,0,rc);
    }
}
static const struct log_info info={};
int main(int argc,char **argv){
    log_init(&info,NULL);osmo_fsm_log_addr(false);osmo_gettimeofday_override=true;
    if(argc!=3)return 2;
    printf("meta\t1\texact-source\tunknown\t%s\t%s\t64\t64\t32\n",argv[1],argv[2]);
    struct osmo_fsm_state states[2]={{.name="alive",.in_event_mask=4,.out_state_mask=3,.action=action,.onenter=enter,.onleave=leave},{.name="wait",.in_event_mask=4,.out_state_mask=3,.action=action,.onenter=enter,.onleave=leave}};
    fsm=(struct osmo_fsm){.name="Tree",.states=states,.num_states=2,.pre_term=preterm,.cleanup=cleanup,.timer_cb=timer_cb};osmo_fsm_register(&fsm);
    for(mode=0;mode<7;mode++)for(unsigned variant=0;variant<3;variant++){
        ++scenario;step=0;osmo_gettimeofday_override_time=(struct timeval){100,0};
        for(unsigned i=0;i<3;i++){struct osmo_fsm_inst *fi=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,ids[i]);fi->T=10+i;int sec=i==0?(variant==1?0:1):i==1?(variant==2?2:1):2;osmo_timer_schedule(&fi->timer,sec,0);}
        struct osmo_timer_list app={};osmo_timer_setup(&app,app_cb,NULL);osmo_timer_schedule(&app,1,0);record("init",variant,mode,0);
        for(unsigned update=0;update<3;update++){osmo_gettimeofday_override_time.tv_sec=update==2?102:101;int rc=osmo_timers_update();record("update",update,osmo_gettimeofday_override_time.tv_sec,rc);}
        osmo_timer_del(&app);
        for(unsigned i=0;i<4;i++){struct osmo_fsm_inst *fi=get(i);if(fi){osmo_fsm_inst_term(fi,OSMO_FSM_TERM_REQUEST,NULL);record("drain",i,0,0);}}
        record("removed",0,0,0);
    }osmo_fsm_unregister(&fsm);return 0;
}
