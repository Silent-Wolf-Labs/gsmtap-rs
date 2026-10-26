#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <osmocom/core/fsm.h>
#include <osmocom/core/talloc.h>
extern struct llist_head osmo_g_fsms;
static struct osmo_fsm fsm,other;
static struct osmo_fsm_inst *nodes[3];
static unsigned scenario,step;
static void hex(const char *s){if(!s){printf("s:-");return;}printf("s:");for(const unsigned char *p=(const unsigned char*)s;*p;p++)printf("%02x",*p);}
static int index_of(const struct osmo_fsm_inst *fi){if(!fi)return -1;for(unsigned i=0;i<3;i++)if(nodes[i]==fi)return i;return -2;}
static int present(unsigned n){struct osmo_fsm_inst *fi;llist_for_each_entry(fi,&fsm.instances,list)if(fi==nodes[n])return 1;return 0;}
static void record(const char *op,int64_t arg,int rc,const char *qid,const char *qname,unsigned event,unsigned state,int cause){
    int safe_id=1;struct osmo_fsm_inst *fi;
    llist_for_each_entry(fi,&fsm.instances,list)if(!fi->id)safe_id=0;
    printf("%u\t%u\t%s\t%lld\t%d\t",scenario,step++,op,(long long)arg,rc);hex(qid);
    printf("\t%d\t",qid&&safe_id?index_of(osmo_fsm_inst_find_by_id(&fsm,qid)):-2);hex(qname);
    printf("\t%d\t%u\t",index_of(osmo_fsm_inst_find_by_name(&fsm,qname)),event);hex(osmo_fsm_event_name(&fsm,event));
    printf("\t%u\t",state);hex(osmo_fsm_state_name(&fsm,state));
    printf("\t%d\t",cause);hex(osmo_fsm_term_cause_name(cause));
    printf("\t%d\t",osmo_fsm_find_by_name(fsm.name)==&fsm);
    unsigned order=0;llist_for_each_entry(fi,&fsm.instances,list){int n=index_of(fi);if(n>=0)order=order*4+n+1;}printf("%u",order);
    for(unsigned i=0;i<3;i++){int live=present(i);printf("\t%d\t%d\t%d\t",live?(int)nodes[i]->state:-1,live?nodes[i]->T:0,live);hex(live?nodes[i]->id:NULL);printf("\t");hex(live?osmo_fsm_inst_name(nodes[i]):NULL);}
    putchar('\n');
}
static const struct log_info info={};
int main(int argc,char **argv){
    log_init(&info,NULL);osmo_fsm_log_addr(false);
    static struct osmo_fsm_state states[]={{.name="zero"},{.name="one"}};
    static const struct value_string names[]={{0,"E0"},{2,"E2"},{2,"duplicate"},{0,NULL}};
    fsm=(struct osmo_fsm){.name="Identity",.states=states,.num_states=2,.event_names=names};other=fsm;
    if(argc==2){osmo_fsm_register(&fsm);nodes[0]=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,NULL);if(!strcmp(argv[1],"null-stored-id"))osmo_fsm_inst_find_by_id(&fsm,"x");else if(!strcmp(argv[1],"null-query-id"))osmo_fsm_inst_find_by_id(&fsm,NULL);return 0;}
    if(argc!=3)return 2;
    printf("meta\t1\texact-source\tunknown\t%s\t%s\t64\t64\t32\n",argv[1],argv[2]);
    for(unsigned c=1;c<128;c++){
        ++scenario;step=0;memset(nodes,0,sizeof(nodes));states[1].name=c==1?NULL:"one";fsm.name="Identity";fsm.event_names=c%2?names:NULL;
        int rc=osmo_fsm_register(&fsm);record("init",c,rc,NULL,NULL,c,c,c%5);
        other.name="Identity";rc=osmo_fsm_register(&other);record("duplicate-register",0,rc,NULL,NULL,c,c,c%5);
        nodes[0]=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"base");nodes[1]=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"base");
        record("allocated",0,0,"base","Identity(base)",c,c,c%5);
        char id[]={ 'x',(char)c,'z',0};rc=osmo_fsm_inst_update_id(nodes[0],id);record("update-id",c,rc,id,"Identity(base)",c,c,c%5);
        rc=osmo_fsm_inst_update_id_f_sanitize(nodes[0],'-',"%s",id);record("sanitize-id",c,rc,"base","Identity(base)",c,c,c%5);
        osmo_fsm_inst_free(nodes[1]);nodes[1]=NULL;record("free-second",0,0,"base","Identity(base)",c,c,c%5);
        nodes[2]=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"base");record("reuse",0,0,"base","Identity(base)",c,c,c%5);
        osmo_fsm_unregister(&fsm);record("unregistered",0,0,"base","Identity(base)",c,c,c%5);
        /* The local implementation does not enforce its comment forbidding this. */
        nodes[1]=osmo_fsm_inst_alloc(&fsm,NULL,NULL,LOGL_DEBUG,"after");record("alloc-unregistered",0,!!nodes[1],"after","Identity(after)",c,c,c%5);
        rc=osmo_fsm_inst_update_id(nodes[0],NULL);record("clear-id",0,rc,NULL,"Identity",c,c,c%5);
        for(unsigned i=0;i<3;i++)if(present(i)){osmo_fsm_inst_free(nodes[i]);nodes[i]=NULL;}
        record("removed",0,0,NULL,"missing",c,c,c%5);
        other.name=id;rc=osmo_fsm_register(&other);record("identifier-register",c,rc,NULL,NULL,c,c,c%5);if(!rc)osmo_fsm_unregister(&other);
    }
    return 0;
}
