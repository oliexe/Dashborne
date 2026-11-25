#include <assert.h>
#include <stdio.h>
#include <string.h>
#define mprotect test_mprotect
#define fopen test_fopen
#define fgets test_fgets
#define fclose test_fclose
#include "../menu.c"
#undef mprotect
#undef fopen
#undef fgets
#undef fclose
void *test_fopen(const char *p,const char *m){return fopen(p,m);}
char *test_fgets(char *b,int n,void *f){return fgets(b,n,f);}
int test_fclose(void *f){return fclose(f);}
static unsigned char activity[128];
static int window,button,shown,registered,resets,removed,opens,clicks,touches,timer_calls,missing,protect_fail;
static void *expected_self;
unsigned long crc32(unsigned long c,const unsigned char *p,unsigned int n){(void)p;(void)n;return c;}
int test_mprotect(void *p,usize n,int flags){(void)p;(void)n;(void)flags;return protect_fail?-1:0;}
int extra_window_bind(const struct dashborne_host *h){(void)h;return 0;}
int extra_window_cleanup(void){return 0;}
void extra_window_open(void){assert(!shown);++opens;}
static void *find(void *p,int id){assert(p==activity);if(id==299200)return missing?0:&button;assert(id==EXTRA_WINDOW);return missing?0:&window;}
static int shown_now(void *p){assert(p==&window);return shown;}
static void show(void *p,int v){assert(p==&window);shown=v;}
static int identify(void *p){assert(p==&button);return 299200;}
static int stock_touch(void *p,const void *event){assert(p==expected_self);(void)event;++touches;return 1;}
static int stock_tick(void *p,int id){assert(p==expected_self && id!=EXTRA_TIMER);++timer_calls;return 7;}
static void stock_click(void *p,void *c){assert(p==activity);(void)c;++clicks;}
static void reg(void *p,int id,int ms){assert(p==activity && id==EXTRA_TIMER && ms==4000);++registered;}
static void reset(void *p,int id,int ms){assert(p==activity && id==EXTRA_TIMER && ms==4000);++resets;}
static void unreg(void *p,int id){assert(p==activity && id==EXTRA_TIMER);++removed;}
static int created_calls,parsed,bound;
static void stock_created(void *p){assert(p==activity);++created_calls;}
static void construct_json(void *p,int type){assert(p && !type);}
static void construct_reader(void *p){assert(p);}
static void destruct(void *p){assert(p);}
static int parse(void *r,const char *first,const char *last,void *j,int comments){
    assert(r && j && !comments && last-first==(long)sizeof(entry_document)-1);
    assert(strstr(first,"window__zz_extra"));++parsed;return 1;
}
static void children(void *root,const void *j){assert(root==&window && j);missing=0;}
static void bind_click(void *control,void *target){assert(control==&button && target==activity);++bound;}
int main(void) {
    find_control=find;is_visible=shown_now;visible=show;id_of=identify;
    original_click=stock_click;original_touch=original_touch_secondary=stock_touch;
    original_timer=original_timer_secondary=stock_tick;
    register_timer=reg;reset_timer=reset;unregister_timer=unreg;
    original_create=stock_created;reader_ctor=construct_reader;reader_dtor=json_dtor=destruct;
    json_ctor=construct_json;json_parse=parse;parse_controls=children;listener=bind_click;
    ((void **)activity)[2]=&window;missing=1;created(activity);
    assert(created_calls==1 && parsed==1 && bound==1);
    created(activity);assert(created_calls==2 && parsed==1 && bound==1);
    expected_self=activity;
    assert(touched(activity,&button)==1 && shown && registered==1 && resets==1);
    assert(touched(activity,&button)==1 && registered==1 && resets==2 && touches==2);
    assert(timer(activity,6)==7 && shown && timer_calls==1);
    assert(timer(activity,EXTRA_TIMER)==0 && !shown);
    expected_self=activity+10*sizeof(void *);
    assert(touched_secondary(expected_self,&button)==1 && shown && registered==2);
    expected_self=activity+sizeof(void *);
    assert(timer_secondary(expected_self,6)==7 && timer_calls==2);
    assert(timer_secondary(expected_self,EXTRA_TIMER)==0 && !shown);
    expected_self=activity;touched(activity,&button);clicked(activity,&button);
    assert(opens==1 && removed==1 && !shown);clicked(activity,0);assert(clicks==1);
    missing=1;int previous=resets;touched(activity,&button);assert(resets==previous && !shown);
    assert(timer(activity,EXTRA_TIMER)==0);missing=0;
    touched(activity,0);assert(!shown);
    void *native[64]={0};table=native;installed=1;host=(void *)1;
    void *callbacks[]={(void *)clicked,(void *)timer,(void *)touched,(void *)timer_secondary,(void *)touched_secondary,(void *)created};
    for(int i=0;i<6;++i){saved[i]=(void *)stock_touch;native[indices[i]]=callbacks[i];}
    native[52]=(void *)stock_tick;assert(core_extra_stop()==-1 && installed && native[4]==(void *)clicked);
    native[52]=(void *)touched_secondary;protect_fail=1;assert(core_extra_stop()==-1 && installed);
    protect_fail=0;assert(!core_extra_stop() && !installed && !host);
    for(int i=0;i<6;++i)assert(native[indices[i]]==(void *)stock_touch);
    puts("Extra entry: touch delegation, timeout/rearming, secondary callbacks, missing controls and cleanup passed");
}
