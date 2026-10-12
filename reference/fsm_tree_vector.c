/* Hierarchy evidence. Callback snapshots never dereference removed instances. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <osmocom/core/fsm.h>
#include <osmocom/core/talloc.h>
static struct osmo_fsm fsm;
static const char *ids[]={"p","a","b","g"};
static unsigned scenario,step,mode,operation,safe;
static int timeout_data;
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
static void action(struct osmo_fsm_inst *fi,uint32_t event,void *data){
    int n=number(fi);
    if(event>=10){record("notify",n,event,timeout_data&&data?*(int32_t*)data:(intptr_t)data);if(mode==5){osmo_fsm_inst_term(fi,OSMO_FSM_TERM_REGULAR,NULL);record("notify-term",n,0,0);}return;}
    record("action",n,event,0);
    if(event==0){osmo_fsm_inst_term(fi,OSMO_FSM_TERM_REGULAR,data);record("action-term",n,0,0);}
}
static void preterm(struct osmo_fsm_inst *fi,enum osmo_fsm_term_cause cause){
    int n=number(fi);record("preterm",n,cause,0);
    if(mode==1&&fi->proc.parent){osmo_fsm_inst_unlink_parent(fi,NULL);record("pre-unlink",n,0,0);}
    if(mode==2&&n==1){osmo_fsm_inst_change_parent(fi,get(2),13);record("pre-reparent",n,2,0);}
    if(mode==7){osmo_fsm_inst_term(fi,OSMO_FSM_TERM_ERROR,NULL);record("pre-repeat",n,0,0);record("pre-dispatch",n,2,osmo_fsm_inst_dispatch(fi,2,NULL));}
}
static void cleanup(struct osmo_fsm_inst *fi,enum osmo_fsm_term_cause cause){
    int n=number(fi);record("cleanup",n,cause,0);
    if(mode==3&&n==1&&get(0)){osmo_fsm_inst_term(get(0),OSMO_FSM_TERM_REQUEST,NULL);record("cleanup-parent-term",n,0,0);record("cleanup-still-readable",n,fi->proc.terminating,0);}
}
static int timer_cb(struct osmo_fsm_inst *fi){record("timeout",number(fi),fi->T,1);fi->T=77;return 1;}
static const struct log_info info={};
static void scene(void){
    osmo_gettimeofday_override_time=(struct timeval){100,0};
    struct osmo_fsm_inst *p=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,ids[0]);
    struct osmo_fsm_inst *a=osmo_fsm_inst_alloc_child(&fsm,p,10);osmo_fsm_inst_update_id(a,ids[1]);
    struct osmo_fsm_inst *b=osmo_fsm_inst_alloc_child(&fsm,p,11);osmo_fsm_inst_update_id(b,ids[2]);
    struct osmo_fsm_inst *g=osmo_fsm_inst_alloc_child(&fsm,a,12);osmo_fsm_inst_update_id(g,ids[3]);
}
static void execute(void){
    struct osmo_fsm_inst *fi;int rc=0;
    switch(operation){
    case 0:osmo_fsm_inst_term(get(0),OSMO_FSM_TERM_REQUEST,(void*)55);break;
    case 1:osmo_fsm_inst_term(get(1),OSMO_FSM_TERM_REGULAR,(void*)55);break;
    case 2:osmo_fsm_inst_term(get(3),OSMO_FSM_TERM_ERROR,(void*)55);break;
    case 3:osmo_fsm_inst_term_children(get(0),OSMO_FSM_TERM_PARENT,(void*)55);break;
    case 4:osmo_fsm_inst_term_children(get(0),OSMO_FSM_TERM_REGULAR,(void*)55);break;
    case 5:osmo_fsm_inst_broadcast_children(get(0),0,(void*)55);break;
    case 6:osmo_fsm_inst_change_parent(get(3),get(2),13);break;
    case 7:osmo_fsm_inst_unlink_parent(get(3),NULL);break;
    case 8:rc=osmo_fsm_inst_dispatch(get(1),0,(void*)55);break;
    case 9:fi=get(1);fi->T=23;osmo_timer_schedule(&fi->timer,1,0);record("armed",1,0,0);timeout_data=1;osmo_gettimeofday_override_time.tv_sec=101;rc=osmo_timers_update();timeout_data=0;break;
    default:fi=get(3);osmo_fsm_inst_unlink_parent(fi,NULL);record("unlink-before-free",3,0,0);osmo_fsm_inst_free(fi);break;
    }
    record("operation",operation,mode,rc);
}
int main(int argc,char **argv){
    log_init(&info,NULL);osmo_fsm_log_addr(false);osmo_gettimeofday_override=true;
    struct osmo_fsm_state states[1]={{.name="alive",.in_event_mask=0x0000ffff,.out_state_mask=1,.action=action}};
    fsm=(struct osmo_fsm){.name="Tree",.states=states,.num_states=1,.pre_term=preterm,.cleanup=cleanup,.timer_cb=timer_cb};osmo_fsm_register(&fsm);
    if(argc==2){
        scene();
        if(!strcmp(argv[1],"legacy-cleanup-uaf")){mode=3;operation=1;execute();}
        else if(!strcmp(argv[1],"free-linked-uaf")){osmo_fsm_inst_free(get(3));record("after-free",3,0,0);}
        else if(!strcmp(argv[1],"children-parent-uaf")){mode=5;operation=4;execute();}
        else if(!strcmp(argv[1],"broadcast-parent-uaf")){mode=5;operation=5;execute();}
        else if(!strcmp(argv[1],"null-parent"))osmo_fsm_inst_alloc_child(&fsm,NULL,10);
        return 0;
    }
    if(argc!=3)return 2;
    printf("meta\t1\texact-source\tunknown\t%s\t%s\t64\t64\t32\n",argv[1],argv[2]);
    for(safe=0;safe<2;safe++)for(mode=0;mode<8;mode++)for(operation=0;operation<11;operation++){
        if(mode==2&&operation!=1&&operation!=8)continue;
        if(mode==3&&(!safe|| (operation!=1&&operation!=8)))continue;
        /* A broadcast's cached next child is unsafe if parent notifications
         * terminate the parent and remaining siblings. Keep that outside golden. */
        if(mode==5&&(operation==4||operation==5))continue;
        ++scenario;step=0;osmo_fsm_term_safely(safe);scene();record("init",operation,mode,safe);execute();
        /* Drain only live roots, then any unlinked nodes; no stale pointers. */
        for(unsigned i=0;i<4;i++){struct osmo_fsm_inst *fi=get(i);if(fi){osmo_fsm_inst_term(fi,OSMO_FSM_TERM_REQUEST,NULL);record("drain",i,0,0);}}
        record("removed",0,0,0);
    }
    osmo_fsm_unregister(&fsm);return 0;
}
