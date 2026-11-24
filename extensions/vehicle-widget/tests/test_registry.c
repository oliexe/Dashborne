#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../registry.c"
static unsigned char editor[80],grid[80],state[64],car[80],old[80];
static void *entries[2],*vector[3];
static int test_tab=2,categories[3]={0,0,1},counts[4],can_fit=1,added,removed,closed,delegated,data_calls,fail_add;
static int test_write_result=1,test_save_result,save_calls,ram_calls;
static unsigned short car_source=VEHICLE_SOURCE,stock_source=0x201;
static struct layout last;
static char caption[80];
static void *edit_pointer=editor;
static void assign_pointer(void *p,void *v){memcpy(p,&v,sizeof(v));}
static void widget(unsigned char *p,int type,unsigned short id,unsigned short *source) {
    memset(p,0,80);memcpy(p+36,&type,4);memcpy(p+48,&id,2);
    assign_pointer(p+52,source);assign_pointer(p+52+sizeof(void *),source+1);
}
static void *get_book(void){return (void *)1;}
static void *get_grid(void *p){assert(p==(void *)1);return grid;}
static void *get_widgets(void *p){assert(p==grid);return vector;}
static int count(void *p,int t){assert(p==editor && t>=5 && t<=8);return counts[t-5];}
static int free_cell(void *p,int t,struct cell want,struct cell *out,unsigned short id){assert(p==grid && t>=5 && t<=8 && want.x==3 && want.y==0);(void)id;*out=(struct cell){3,0};return can_fit;}
static void add(void *p,struct layout *l){assert(p==editor);if(*l->begin==VEHICLE_SOURCE)assert(l->end-l->begin==3 && l->begin[1]==0 && l->begin[2]==35);++added;last=*l;if(fail_add && *l->begin==VEHICLE_SOURCE)return;widget(car,l->type,42,l->begin[0]==VEHICLE_SOURCE?&car_source:&stock_source);entries[0]=car;vector[1]=entries+1;}
static void remove_entry(void *p,unsigned short id){assert(p==editor && id==9);++removed;vector[1]=entries;}
static void close_edit(void *p,int ok){assert(p==editor && ok);++closed;}
static void stock_click(void *l,int r,int c){(void)l;(void)r;(void)c;++delegated;}
static void stock_data(void *l,void *i,int r){(void)l;(void)i;(void)r;++data_calls;}
static void *sub(const void *p,int id){assert(id==24002);return (void *)p;}
static void label(void *p,const char *s){assert(p==(void *)2);snprintf(caption,sizeof(caption),"%s",s);}
static void log_message(const struct dashborne_host *h,const char *m){(void)h;assert(strstr(m,"vehicle-widget:"));}
static int write_json(const char *p,const void *j){assert(j==(void *)3);if(!strcmp(p,VEHICLE_LAYOUT_RAM))++ram_calls;return test_write_result;}
int layout_save(void){++save_calls;return test_save_result;}
int layout_prepare(void){return 0;}void layout_cleanup(void){}
int memory_prepare(struct memory_edit *p,void *a,const void *b,const void *c,unsigned int d){(void)p;(void)a;(void)b;(void)c;(void)d;return 0;}
static int restore_count,restore_indices[40],load_fail;
int memory_apply(struct memory_edit *p){(void)p;return 0;}
int memory_restore(struct memory_edit *p){restore_indices[restore_count++]=(int)(p-edits);return 0;}
static void *get_loader(void){return (void *)6;}
static int reload(void *p){assert(p==(void *)6);assert(restore_count>=2 && restore_indices[0]==14 && restore_indices[1]==15);return !load_fail;}
static void native_resume(void *self){assert(self==(void *)4);assert(short_at(intent+36)==65535);}
static void reset(void) {
    vector[0]=vector[1]=entries;vector[2]=entries+2;memset(state,0,sizeof(state));
    state[0]=1;int x=3;memcpy(state+4,&x,4);unsigned short none=65535;memcpy(state+36,&none,2);
    memset(counts,0,sizeof(counts));can_fit=1;added=removed=closed=fail_add=0;
}
int main(void) {
    const struct dashborne_host host={.log=log_message};api=&host;
    tab=&test_tab;category=categories;intent=state;caller=&edit_pointer;
    assign_pointer(editor+48,grid);book=get_book;active_grid=get_grid;widgets=get_widgets;count_on_page=count;nearest=free_cell;
    add_widget=add;remove_widget=remove_entry;close_editor=close_edit;original_click=stock_click;original_data=stock_data;find_sub=sub;set_text=label;
    uintptr stock[17][18];for(int t=0;t<17;++t)for(int i=0;i<18;++i)stock[t][i]=(uintptr)(t*100+i);
    prepare_catalog(&stock[0][0],(void *)5);
    assert(!memcmp(stock,catalog,sizeof(stock)) && !strcmp((char *)descriptor[0],"3D Vehicle"));
    assert(descriptor[15]==5 && !descriptor[16] && descriptor[14]==stock[1][14]);
    reset();item_data((void *)1,(void *)2,4);assert(!strcmp(caption,"3D Vehicle"));
    item_click((void *)1,0,0);assert(delegated==1 && !added);
    item_click((void *)1,4,0);assert(added==1 && closed==1 && last.type==5 && last.x==3 && last.y==0 && registry_widget(car));
    item_click((void *)1,4,0);assert(added==1); /* No duplicate on a page. */
    reset();counts[0]=1;item_click((void *)1,4,0);assert(last.type==6 && added==1);
    reset();for(int i=0;i<4;++i)counts[i]=1;item_click((void *)1,4,0);assert(!added);
    reset();can_fit=0;item_data((void *)1,(void *)2,4);assert(strstr(caption,"no free"));item_click((void *)1,4,0);assert(!added && !removed);
    item_data((void *)1,(void *)2,0);assert(!caption[0]); /* Recycled stock row. */
    reset();widget(old,5,9,&stock_source);entries[0]=old;vector[1]=entries+1;unsigned short id=9;memcpy(state+36,&id,2);counts[0]=1;
    item_click((void *)1,4,0);assert(removed==1 && added==1 && closed==1 && last.type==5);
    reset();widget(old,5,9,&stock_source);entries[0]=old;vector[1]=entries+1;memcpy(state+36,&id,2);fail_add=1;
    item_click((void *)1,4,0);assert(removed==1 && added==2 && !closed && !registry_widget(car));
    reset();memcpy(state+36,&id,2);item_click((void *)1,4,0);assert(!added && !removed); /* Stale selection. */
    reset();memcpy(state+36,&id,2);memcpy(state+38,&car_source,2);picker_resume=native_resume;resume_picker((void *)4);assert(short_at(state+36)==id);
    json_write=write_json;assert(write_layout("/data/unrelated.json",(void *)3)==1 && !save_calls);
    assert(write_layout(VEHICLE_LAYOUT_RAM,(void *)3)==1 && save_calls==1 && ram_calls==1);
    test_save_result=-1;assert(write_layout(VEHICLE_LAYOUT_RAM,(void *)3)==0); /* Never falls through to flash. */
    test_write_result=0;int before=save_calls;assert(!write_layout(VEHICLE_LAYOUT_RAM,(void *)3) && save_calls==before);
    persistence_busy=1;before=ram_calls;assert(!write_layout(VEHICLE_LAYOUT_RAM,(void *)3) && ram_calls==before);
    loader=get_loader;loader_load=reload;path_edits=10;registered=1;edit_count=17;
    load_fail=1;assert(registry_cleanup()==-1 && registered && restore_count==2); /* No write path restored on failure. */
    load_fail=0;restore_count=0;assert(!registry_cleanup() && !registered && !api && restore_count==19);
    assert(restore_indices[2]==16 && restore_indices[18]==0);
    puts("Vehicle registry: picker selection, placement, occupancy, replacement, rollback, re-edit and persistence routing passed");
}
