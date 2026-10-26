/* Actual unmodified static engine is included for controllable tick inputs.
 * No observed API or interval algorithm is reimplemented. */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/timerfd.h>
#include <osmocom/core/logging.h>
#include "src/core/rate_ctr.c"
_Static_assert(sizeof(unsigned int)==4 && sizeof(int)==4 && sizeof(uint64_t)==8,
               "recorded field widths");
_Static_assert(__builtin_types_compatible_p(__typeof__(((struct rate_ctr_group_desc*)0)->num_ctr), unsigned int), "num_ctr unsigned int");
_Static_assert(__builtin_types_compatible_p(__typeof__(((struct rate_ctr_group_desc*)0)->class_id), int), "class_id int");
_Static_assert(__builtin_types_compatible_p(__typeof__(((struct rate_ctr_group*)0)->idx), unsigned int), "idx unsigned int");
static int fail_create,fail_schedule,create_clock,create_flags,schedule_calls;
int __real_timerfd_create(int,int);
int __real_timerfd_settime(int,int,const struct itimerspec*,struct itimerspec*);
int __wrap_timerfd_create(int clockid,int flags) {
    create_clock=clockid;create_flags=flags;
    if(fail_create){errno=EMFILE;return -1;}return __real_timerfd_create(clockid,flags);
}
int __wrap_timerfd_settime(int fd,int flags,const struct itimerspec *in,struct itimerspec *out) {
    schedule_calls++;
    OSMO_ASSERT(in->it_interval.tv_sec==1 && !in->it_interval.tv_nsec);
    OSMO_ASSERT(in->it_value.tv_sec==1 && !in->it_value.tv_nsec);
    if(fail_schedule){errno=EINVAL;return -1;}return __real_timerfd_settime(fd,flags,in,out);
}
static const struct rate_ctr_desc ctrs[]={{"a","A"},{"a","duplicate"},{"b","B"}};
static const struct rate_ctr_desc dots[]={{"a.b","A"},{"plain","P"}};
static const struct rate_ctr_group_desc descs[]={
    {"g","Group",INT_MIN,3,ctrs},{"g.dot","Dot",INT_MAX,2,dots},
    {"bad name","Invalid",-1,3,ctrs},{"empty","Empty",0,0,ctrs},
    {"other","Other",42,3,ctrs},{"g:dot","Colon",1,2,dots}};
static struct rate_ctr_group *nodes[8];
static unsigned scenario,step;
static char trace[4096];
static void tr(int x) {char b[32];snprintf(b,sizeof(b),"%s%d",*trace?",":"",x);OSMO_ASSERT(strlen(trace)+strlen(b)<sizeof(trace));strcat(trace,b);}
static int id(const struct rate_ctr_group *g) {if(!g)return -1;for(int i=0;i<8;i++)if(nodes[i]==g)return i;return -2;}
static void hex(FILE *f,const char *s){if(!s){fputs("s:-",f);return;}fputs("s:",f);for(const unsigned char *p=(const unsigned char*)s;*p;p++)fprintf(f,"%02x",*p);}
static char *snapshot(void){
    char *out=NULL;size_t size=0;FILE *f=open_memstream(&out,&size);OSMO_ASSERT(f);
    struct rate_ctr_group *g;int count=0;
    llist_for_each_entry(g,&rate_ctr_groups,list){
        fprintf(f,"%s%d,%u,",count++?";":"",id(g),g->idx);hex(f,g->desc->group_name_prefix);fputc(',',f);hex(f,g->desc->group_description);
        fprintf(f,",%d,",g->desc->class_id);hex(f,g->name);fprintf(f,",%u",g->desc->num_ctr);
        for(unsigned i=0;i<g->desc->num_ctr;i++){
            fputc(',',f);hex(f,g->desc->ctr_desc[i].name);fputc(',',f);hex(f,g->desc->ctr_desc[i].description);
            struct rate_ctr *c=rate_ctr_group_get_ctr(g,i);fprintf(f,",%"PRIu64",%"PRIu64,c->current,c->previous);
            for(unsigned j=0;j<4;j++)fprintf(f,",%"PRIu64",%"PRIu64,c->intv[j].last,c->intv[j].rate);
        }
    }if(!count)fputs("-",f);fclose(f);return out;
}
static int visit_plain(struct rate_ctr_group *g,void *data){tr(100+id(g));return 0;}
struct visit {unsigned mode;int rc;};
static int group_visit(struct rate_ctr_group *g,void *data){
    struct visit *v=data;tr(id(g));
    if(v->mode==1){rate_ctr_group_upd_idx(g,g->idx+10);rate_ctr_group_set_name(g,"visited");}
    if(v->mode==2 && g->list.next!=&rate_ctr_groups){struct rate_ctr_group *n=llist_entry(g->list.next,struct rate_ctr_group,list);int k=id(n);rate_ctr_group_free(n);nodes[k]=NULL;}
    if(v->mode==3 && !nodes[7])nodes[7]=rate_ctr_group_alloc(NULL,&descs[4],77);
    if(v->mode==4){tr(id(rate_ctr_get_group_by_name_idx(g->desc->group_name_prefix,g->idx)));rate_ctr_for_each_group(visit_plain,NULL);}
    if(v->mode==5 && g->list.next!=&rate_ctr_groups){struct rate_ctr_group *n=llist_entry(g->list.next,struct rate_ctr_group,list);llist_del_init(&n->list);}
    if(v->mode==6){int k=id(g);rate_ctr_group_free(g);nodes[k]=NULL;}
    return v->rc;
}
static int ctr_plain(struct rate_ctr_group *g,struct rate_ctr *c,const struct rate_ctr_desc *d,void *data){tr(100+(int)(c-g->ctr));return 0;}
static int ctr_visit(struct rate_ctr_group *g,struct rate_ctr *c,const struct rate_ctr_desc *d,void *data){
    struct visit *v=data;unsigned i=c-g->ctr;tr((int)i);
    if(v->mode==1 && !i)rate_ctr_group_reset(g);
    if(v->mode==2){tr((int)(rate_ctr_get_by_name(g,d->name)-g->ctr));rate_ctr_for_each_counter(g,ctr_plain,NULL);}
    return v->rc;
}
static int expire(uint64_t count){int fds[2];OSMO_ASSERT(!pipe(fds));OSMO_ASSERT(write(fds[1],&count,8)==8);struct osmo_fd ofd={.fd=fds[0]};int rc=rate_ctr_timer_cb(&ofd,OSMO_FD_READ);close(fds[0]);close(fds[1]);return rc;}
static void record(const char *op,unsigned slot,uint64_t a,int64_t b,const char *text){
    char *before=snapshot();uint64_t ticks=timer_ticks;int64_t rc=0;trace[0]=0;
    struct rate_ctr_group *g=nodes[slot];
    if(!strcmp(op,"alloc")){nodes[slot]=rate_ctr_group_alloc(NULL,&descs[a],(unsigned)b);rc=!!nodes[slot];}
    else if(!strcmp(op,"free")){rate_ctr_group_free(g);nodes[slot]=NULL;}
    else if(!strcmp(op,"index"))rate_ctr_group_upd_idx(g,(unsigned)a);
    else if(!strcmp(op,"name"))rate_ctr_group_set_name(g,text);
    else if(!strcmp(op,"seed")){struct rate_ctr *c=rate_ctr_group_get_ctr(g,(unsigned)a);c->current=(uint64_t)b;c->previous=(uint64_t)b+13;for(unsigned j=0;j<4;j++){c->intv[j].last=(uint64_t)b+j+1;c->intv[j].rate=100+j;}}
    else if(!strcmp(op,"add"))rate_ctr_add2(g,(unsigned)a,(int)b);
    else if(!strcmp(op,"inc"))rate_ctr_inc2(g,(unsigned)a);
    else if(!strcmp(op,"difference"))rc=rate_ctr_difference(rate_ctr_group_get_ctr(g,(unsigned)a));
    else if(!strcmp(op,"reset"))rate_ctr_group_reset(g);
    else if(!strcmp(op,"counter-reset"))rate_ctr_reset(rate_ctr_group_get_ctr(g,(unsigned)a));
    else if(!strcmp(op,"lookup"))rc=id(rate_ctr_get_group_by_name_idx(text,(unsigned)a));
    else if(!strcmp(op,"counter-lookup")){const struct rate_ctr *c=rate_ctr_get_by_name(g,text);rc=c?c-g->ctr:-1;}
    else if(!strcmp(op,"groups")){struct visit v={(unsigned)a,(int)b};rc=rate_ctr_for_each_group(group_visit,&v);}
    else if(!strcmp(op,"counters")){struct visit v={(unsigned)a,(int)b};rc=rate_ctr_for_each_counter(g,ctr_visit,&v);}
    else if(!strcmp(op,"phase"))timer_ticks=a;
    else if(!strcmp(op,"expire"))rc=expire(a);
    else OSMO_ASSERT(false);
    char *after=snapshot();printf("%u\t%u\t%s\t%u\t%"PRIu64"\t%"PRId64"\t",scenario,step++,op,slot,a,b);hex(stdout,text);
    printf("\t%"PRId64"\t%"PRIu64"\t%"PRIu64"\t%s\t%s\t%s\n",rc,ticks,timer_ticks,before,after,*trace?trace:"-");free(before);free(after);
}
static void clear(void){for(unsigned i=0;i<8;i++)if(nodes[i]){rate_ctr_group_free(nodes[i]);nodes[i]=NULL;}timer_ticks=0;step=0;scenario++;}
static void alias_case(int mangled){
    char prefix[20],name[20]="plain",dot[20]="a.b",description[20]="original",group_description[20]="group-original",report_name[20]="report";
    strcpy(prefix,mangled?"alias.dot":"alias");struct rate_ctr_desc cd[]={{name,description},{dot,description}};
    struct rate_ctr_group_desc d={prefix,group_description,-7,mangled?2u:1u,cd};
    nodes[0]=rate_ctr_group_alloc(NULL,&d,3);OSMO_ASSERT(nodes[0]);struct rate_ctr_group *g=nodes[0];
    int flags=(g->desc==&d)|((g->desc->group_name_prefix==prefix)<<1)|((g->desc->ctr_desc==cd)<<2)|((g->desc->ctr_desc[0].name==name)<<3)|((g->desc->group_description==group_description)<<4)|((g->desc->ctr_desc[0].description==description)<<5);
    char *before=snapshot();uint64_t ticks=timer_ticks;
    strcpy(prefix,"changed");strcpy(name,"changed");strcpy(dot,"changed");strcpy(description,"edited");strcpy(group_description,"group-edited");d.class_id=19; /* num_ctr stays within allocation */
    char *after=snapshot();printf("%u\t%u\talias\t0\t%d\t0\ts:-\t%d\t%"PRIu64"\t%"PRIu64"\t%s\t%s\t-\n",scenario,step++,mangled,flags,ticks,ticks,before,after);free(before);free(after);
    rate_ctr_group_set_name(g,report_name);before=snapshot();strcpy(report_name,"edited");OSMO_ASSERT(!strcmp(g->name,"report"));after=snapshot();
    printf("%u\t%u\talias-report\t0\t0\t0\ts:7265706f7274\t%d\t%"PRIu64"\t%"PRIu64"\t%s\t%s\t-\n",scenario,step++,g->name!=report_name,ticks,ticks,before,after);free(before);free(after);
    /* Descriptors have stack lifetimes: free before leaving this scope. */
    rate_ctr_group_free(g);nodes[0]=NULL;
}
static int destructive(struct rate_ctr_group *g,void *data){
    if(!strcmp(data,"free-current"))rate_ctr_group_free(g);
    else if(!strcmp(data,"unlink-current-init"))llist_del_init(&g->list);
    else llist_del(&g->list);
    return 0;
}
static int diagnostics(const char *name){
    if(!strcmp(name,"zero-expirations"))expire(0);
    else if(!strcmp(name,"short-read")){int fds[2];OSMO_ASSERT(!pipe(fds));int x=1;OSMO_ASSERT(write(fds[1],&x,sizeof(x))==sizeof(x));struct osmo_fd ofd={.fd=fds[0]};rate_ctr_timer_cb(&ofd,OSMO_FD_READ);}
    else if(!strcmp(name,"null-descriptor"))rate_ctr_group_alloc(NULL,NULL,0);
    else if(!strcmp(name,"null-counter-name")){struct rate_ctr_desc c={NULL,"N"};struct rate_ctr_group_desc d={"g","G",0,1,&c};OSMO_ASSERT(!rate_ctr_group_alloc(NULL,&d,0));return 0;}
    else {nodes[0]=rate_ctr_group_alloc(NULL,&descs[0],0);if(!strcmp(name,"counter-bounds"))rate_ctr_add2(nodes[0],3,1);else rate_ctr_for_each_group(destructive,(void*)name);}
    return 0;
}
static void native_case(const char *mode){
    fail_create=!strcmp(mode,"init-create-fail");fail_schedule=!strcmp(mode,"init-schedule-fail");
    int first=rate_ctr_init(NULL),registered=osmo_fd_is_registered(&rate_ctr_timer);int calls=schedule_calls;
    int second=rate_ctr_init(NULL);printf("native\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",mode,first,second,registered,create_clock,create_flags,calls,schedule_calls);
    if(registered){rate_ctr_timer_cb(&rate_ctr_timer,0);if(!fail_schedule)OSMO_ASSERT(rate_ctr_timer_cb(&rate_ctr_timer,OSMO_FD_READ)==0);osmo_fd_unregister(&rate_ctr_timer);close(rate_ctr_timer.fd);}
}
int main(int argc,char **argv){
    static struct log_info info={};log_init(&info,NULL);
    if(argc==2){if(!strncmp(argv[1],"init-",5)){native_case(argv[1]);return 0;}if(!strcmp(argv[1],"native-smoke")){OSMO_ASSERT(rate_ctr_init(NULL)==0);OSMO_ASSERT(osmo_select_main(0)>=0);OSMO_ASSERT(timer_ticks>=1);osmo_fd_unregister(&rate_ctr_timer);close(rate_ctr_timer.fd);return 0;}return diagnostics(argv[1]);}
    if(argc!=3)return 2;
    printf("meta\t1\texact-source\tunknown\t%s\t%s\t64\t64\t32\n",argv[1],argv[2]);
    clear();record("groups",0,0,0,NULL);record("alloc",0,3,0,NULL);record("counters",0,0,4,NULL);record("free",0,0,0,NULL);
    for(unsigned d=0;d<6;d++){clear();record("alloc",0,d,2,NULL);if(nodes[0]){record("alloc",1,d,2,NULL);record("name",0,0,0,"report");record("name",0,0,0,"");record("name",0,0,0,NULL);record("lookup",0,2,0,descs[d].group_name_prefix);record("lookup",0,2,0,"g:dot");record("index",0,99,0,NULL);record("lookup",0,99,0,nodes[0]->desc->group_name_prefix);if(nodes[0]->desc->num_ctr){record("counter-lookup",0,0,0,"a");record("counter-lookup",0,0,0,"missing");}record("free",1,0,0,NULL);record("free",0,0,0,NULL);}record("alloc",2,2,0,NULL);}
    /* Every forbidden ASCII byte and high-bit byte, with valid storage. */
    for(unsigned c=1;c<256;c++) {clear();char prefix[]={'x',(char)c,'z',0};struct rate_ctr_group_desc d={prefix,"byte",0,0,ctrs};char *before=snapshot();nodes[0]=rate_ctr_group_alloc(NULL,&d,0);char *after=snapshot();printf("%u\t%u\tidentifier\t0\t%u\t0\t",scenario,step++,c);hex(stdout,prefix);printf("\t%d\t0\t0\t%s\t%s\t-\n",!!nodes[0],before,after);free(before);free(after);if(nodes[0]){rate_ctr_group_free(nodes[0]);nodes[0]=NULL;}}
    for(unsigned c=1;c<256;c++) {clear();char name[]={'x',(char)c,'z',0};struct rate_ctr_desc cd={name,"byte"};struct rate_ctr_group_desc d={"identifier","counter-byte",-1,1,&cd};char *before=snapshot();nodes[0]=rate_ctr_group_alloc(NULL,&d,0);char *after=snapshot();printf("%u\t%u\tidentifier-counter\t0\t%u\t0\t",scenario,step++,c);hex(stdout,name);printf("\t%d\t0\t0\t%s\t%s\t-\n",!!nodes[0],before,after);free(before);free(after);if(nodes[0]){rate_ctr_group_free(nodes[0]);nodes[0]=NULL;}}
    for(unsigned variant=0;variant<3;variant++){clear();record("alloc",0,0,variant?UINT_MAX:5,NULL);record("alloc",1,0,variant==2?7:0,NULL);record("alloc",2,0,variant?UINT_MAX:5,NULL);record("index",0,7,0,NULL);record("index",1,7,0,NULL);record("lookup",0,7,0,"g");record("alloc",3,4,7,NULL);record("free",1,0,0,NULL);record("alloc",1,0,7,NULL);}
    for(unsigned mode=0;mode<7;mode++)for(int rc=-3;rc<=4;rc+=7){clear();record("alloc",0,0,0,NULL);record("alloc",1,0,1,NULL);record("alloc",2,0,2,NULL);record("groups",0,mode,mode==6?-3:rc,NULL);}
    for(unsigned mode=0;mode<3;mode++)for(int rc=-3;rc<=4;rc+=7){clear();record("alloc",0,0,0,NULL);for(unsigned i=0;i<3;i++)record("seed",0,i,-(int64_t)(i+1),NULL);record("counters",0,mode,rc,NULL);record("reset",0,0,0,NULL);}
    const uint64_t phases[]={0,58,59,60,3598,3599,3600,86398,86399,86400,UINT64_MAX-1,UINT64_MAX};
    const int increments[]={0,1,-1,INT_MIN,INT_MAX};
    for(unsigned p=0;p<12;p++)for(unsigned inc=0;inc<5;inc++){clear();record("phase",0,phases[p],0,NULL);record("expire",0,1,0,NULL);record("alloc",0,0,3,NULL);record("alloc",1,4,9,NULL);for(unsigned i=0;i<3;i++)record("seed",0,i,-(int64_t)(i+1),NULL);record("add",0,0,increments[inc],NULL);record("inc",0,1,0,NULL);record("difference",0,0,0,NULL);record("expire",0,1,0,NULL);record("expire",0,3,0,NULL);record("difference",0,0,0,NULL);record("counter-reset",0,1,0,NULL);record("expire",0,2,0,NULL);record("reset",0,0,0,NULL);record("expire",0,1,0,NULL);record("free",0,0,0,NULL);record("free",1,0,0,NULL);record("expire",0,1,0,NULL);}
    clear();record("alloc",0,0,0,NULL);record("add",0,0,123,NULL);record("expire",0,86401,0,NULL);
    /* Invalid descriptors safely backed by live arrays. */
    for(unsigned bad=0;bad<5;bad++) {clear();struct rate_ctr_desc cd={bad==2?NULL:bad==3?"":"bad name","D"};struct rate_ctr_group_desc d={bad==0?NULL:bad==1?"":"valid","G",0,bad<2?0u:1u,&cd};char *before=snapshot();nodes[0]=rate_ctr_group_alloc(NULL,&d,1);char *after=snapshot();printf("%u\t%u\tinvalid\t0\t%u\t0\ts:-\t%d\t0\t0\t%s\t%s\t-\n",scenario,step++,bad,!!nodes[0],before,after);free(before);free(after);OSMO_ASSERT(!nodes[0]);}
    clear();alias_case(0);clear();alias_case(1);clear();return 0;
}
