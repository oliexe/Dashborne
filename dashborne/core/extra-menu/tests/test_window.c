#include <assert.h>
#include <string.h>
#include "../window.c"
static int opens,closes,refreshes,launches;
static unsigned int available,last_token;
static int ids[]={LIST,OPEN,BACK,599202,599203,599204};
static char labels[6][128];
static int shown[6];
static unsigned int menu_count(const struct dashborne_host *h) {(void)h;return available;}
static int menu_entry(const struct dashborne_host *h,unsigned int n,struct dashborne_extra_entry *out) {
    (void)h;if(n>=available)return -1;
    *out=(struct dashborne_extra_entry){.token=n+100};
    strcpy(out->title,n?"Second extension":"Screen Mirroring");strcpy(out->description,"Open an extension");return 0;
}
static int menu_open(const struct dashborne_host *h,unsigned int token) {
    (void)h;last_token=token;++launches;return 0;
}
static void *get_context(void) {return (void *)123;}
static void open_fake(void *c,const char *name,void *intent) {assert(c==(void *)123 && name && !intent);++opens;}
static void close_fake(void *c,const char *name) {assert(c==(void *)123 && name);++closes;}
static int id_fake(void *p) {return *(int *)p;}
static void *find_fake(void *s,int id) {(void)s;for(int i=0;i<6;++i)if(ids[i]==id)return &ids[i];return 0;}
static void visible_fake(void *p,int n) {shown[(int *)p-ids]=n;}
static void text_fake(void *p,const char *t) {assert(t);strcpy(labels[(int *)p-ids],t);}
static void mark_fake(void *p,int n) {(void)p;(void)n;}
static void bind_fake(void *p,void *adapter) {(void)p;(void)adapter;}
static void refresh_fake(void *p) {(void)p;++refreshes;}
static void logger(const struct dashborne_host *h,const char *t) {(void)h;assert(t);}
int main(void) {
    struct dashborne_host host={.log=logger,.extra_count=menu_count,.extra_entry=menu_entry,.open_extra=menu_open};
    api=&host;context=get_context;open_activity=open_fake;close_activity=close_fake;
    id_of=id_fake;find=find_fake;visible=visible_fake;text=text_fake;mark=mark_fake;
    ctor=noop;dtor=noop;base_created=noop;set_adapter=bind_fake;set_listener=bind_fake;refresh=refresh_fake;
    extra_window_open();extra_window_open();assert(opens==1 && pending);
    void *s=create();assert(s && live==s && !create());created(s);resumed(s);
    assert(!count && selected==-1 && shown[5] && !shown[1]);
    available=64;resumed(s);assert(count==64 && selected==0 && shown[1] && !shown[5]);
    assert(row_count(0,0)==64);row_click(0,&ids[0],63,0);assert(selected==63);
    int button=OPEN;click(s,&button);assert(launches==1 && last_token==163);
    resumed(s);assert(selected==63); /* Return from child preserves selection. */
    available=1;resumed(s);assert(selected==0 && count==1);
    click(s,&button);assert(last_token==100 && launches==2);
    row_click(0,&ids[0],-1,0);row_click(0,&ids[0],64,0);assert(selected==0);
    assert(extra_window_cleanup()==-1);
    button=BACK;click(s,&button);click(s,&button);assert(closes==1 && closing);
    button=OPEN;click(s,&button);assert(launches==2);
    destroy(s);assert(!live && !pending && refreshes>0);
    assert(!extra_window_cleanup());return 0;
}
