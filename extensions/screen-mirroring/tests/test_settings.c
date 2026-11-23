#include <assert.h>
#include <string.h>
#include "../settings.c"
static int opens,clicks,rows,selected_value;
static void *expected;
static const char *label;
void window_open(void) {++opens;}
static int fake_id(void *p) {return *(int *)p;}
/* Model an already hooked list: four stock entries plus Contrast. */
static int previous_count(void *s,void *l) {assert(s==expected);return *(int *)l==80001?5:8;}
static void previous_data(void *s,void *l,void *i,int n) {(void)l;(void)i;assert(s==expected && n<5);++rows;}
static void previous_click(void *s,void *l,int n,int sub) {(void)l;(void)sub;assert(s==expected && n<5);++clicks;}
static void set_text(void *p,const char *s) {(void)p;label=s;}
static void set_selected(void *p,int n) {(void)p;selected_value=n;}
int main(void) {
    char activity[80];int list=80001,other=80002,item=1;expected=activity;
    id_of=fake_id;text=set_text;selected=set_selected;
    original_count=previous_count;original_data=previous_data;original_click=previous_click;
    assert(count_secondary(activity+32,&list)==6 && own_index==5);
    assert(count(activity,&other)==8);
    data_secondary(activity+32,&list,&item,5);assert(!strcmp(label,"* Screen Mirroring") && !selected_value && !rows);
    data(activity,&list,&item,4);assert(rows==1);
    click_secondary(activity+28,&list,5,0);assert(opens==1 && !clicks);
    click(activity,&list,4,0);assert(clicks==1); /* Contrast still receives its row. */
    click(activity,&other,0,0);assert(clicks==2);
    return 0;
}
