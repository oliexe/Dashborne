#include <assert.h>
#include "../extension.c"
/* Exercise the actual callback adapters without opening the device display. */
struct control { int id, selected, visible, progress; const char *text; };
static struct control controls[]={ {TOPICS,0,1,0,0},{STOCK_PANEL,0,1,0,0},
    {PANEL,0,0,0,0},{CONTRAST,0,1,0,0},{LUMINANCE,0,1,0,0},
    {ENABLE,0,1,0,0},{RESET,0,1,0,0} };
static int old_clicks,old_data,old_progress,refreshes;
static void *last_self;
static void *find_fake(void *self,int id) {
    (void)self;
    for(unsigned int i=0;i<sizeof(controls)/sizeof(controls[0]);++i)if(controls[i].id==id)return &controls[i];
    return (void *)0;
}
static int id_fake(void *c) {return ((struct control *)c)->id;}
static void text_fake(void *c,const char *s) {((struct control *)c)->text=s;}
static void visible_fake(void *c,int n) {((struct control *)c)->visible=n;}
static void selected_fake(void *c,int n) {((struct control *)c)->selected=n;}
static void refresh_fake(void *c) {(void)c;refreshes++;}
static void progress_fake(void *c,int n) {((struct control *)c)->progress=n;}
static int count_fake(void *s,void *l) {last_self=s;return id_fake(l)==TOPICS?3:7;}
static void data_fake(void *s,void *l,void *i,int n) {(void)l;(void)n;last_self=s;old_data++;selected_fake(i,1);}
static void click_fake(void *s,void *l,int n,int sub) {(void)l;(void)n;(void)sub;last_self=s;old_clicks++;}
static void change_fake(void *s,void *b,int n) {(void)b;(void)n;last_self=s;old_progress++;}
int display_capture(struct display_state *s) {(void)s;return 0;}
int display_apply(struct display_state *s,int c,int l) {s->enabled=1;s->contrast=c;s->luminance=l;return 0;}
int display_restore(struct display_state *s) {s->enabled=0;return 0;}
int main(void) {
    char self[64];struct control row={0},other={123,0,0,0,0};
    find=find_fake;id_of=id_fake;text=text_fake;visible=visible_fake;select_control=selected_fake;
    refresh=refresh_fake;progress=progress_fake;original_items=count_fake;original_data=data_fake;
    original_item_click=click_fake;original_progress=change_fake;
    display=(struct display_state){50,50,50,50,1,0,0};
    assert(items_secondary(self+32,&controls[0])==4 && last_self==self);
    assert(item_count(self,&other)==7);
    data_secondary(self+32,&controls[0],&row,3);assert(row.text && !row.selected && !old_data);
    click_secondary(self+28,&controls[0],3,0);
    assert(selected && controls[2].visible && !controls[1].visible && !old_clicks);
    item_data(self,&controls[0],&row,1);assert(old_data==1 && !row.selected);
    item_clicked(self,&controls[0],1,0);assert(!selected && !controls[2].visible && controls[1].visible && old_clicks==1);
    item_clicked(self,&other,2,0);assert(old_clicks==2);
    progress_secondary(self+24,&controls[3],62);assert(display.contrast==62 && !display.enabled);
    clicked(self,&controls[5]);assert(display.enabled);
    changed(self,&controls[4],47);assert(display.luminance==47);
    changed(self,&controls[3],101);assert(display.contrast==62);
    progress_secondary(self+24,&other,70);assert(old_progress==1 && last_self==self);
    clicked(self,&controls[6]);assert(!display.enabled && display.contrast==50 && display.luminance==50);
    assert(refreshes==2);
    return 0;
}
