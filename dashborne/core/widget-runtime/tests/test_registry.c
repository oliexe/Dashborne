#include <assert.h>
#include <stdio.h>
#include <string.h>
#define access test_access
#include "../registry.c"
#undef access
static const char *readable;
int test_access(const char *p,int m){assert(m==4);return readable && !strcmp(p,readable)?0:-1;}
struct widget_provider providers[PROVIDERS];
const struct widget_geometry widget_sizes[8]={{1,1,0,250,224},{2,1,2,510,224},{1,2,4,250,458},{2,2,5,510,458},{3,2,13,770,458},{4,2,14,1030,458},{5,2,15,1290,458},{6,2,16,1550,458}};
static unsigned char image[0x35a000],editor[80],grid[80],state[64],custom[80],old[80];
static void *entries[2],*vector[3],*edit_pointer=editor;
static int expected_stock=1;
static int categories[3],can_fit=1,added,removed,closed,delegated,fail_add,test_save_result,save_calls,ram_calls;
static unsigned short custom_source=0x212,stock_source=0x201;
static struct layout last;
static char caption[160],picture_path[512];
static struct rect icon_bounds;
static void assign_pointer(void *p,void *v){memcpy(p,&v,sizeof(v));}
static void widget(unsigned char *p,int type,unsigned short id,unsigned short *source) {
    memset(p,0,80);memcpy(p+36,&type,4);memcpy(p+48,&id,2);
    assign_pointer(p+52,source);assign_pointer(p+52+sizeof(void *),source+1);
}
static void *get_book(void){return (void *)1;}
static void *get_grid(void *p){assert(p==(void *)1);return grid;}
static void *get_widgets(void *p){assert(p==grid);return vector;}
static int free_cell(void *p,int t,struct cell want,struct cell *out,unsigned short id){assert(p==grid && t>=0 && t<=16 && want.x==0 && want.y==0);(void)id;*out=(struct cell){0,0};return can_fit;}
static void add(void *p,struct layout *l){assert(p==editor);++added;last=*l;if(fail_add && *l->begin==custom_source)return;l->id=42;widget(custom,l->type,42,*l->begin==custom_source?&custom_source:&stock_source);entries[0]=custom;vector[1]=entries+1;}
static void remove_entry(void *p,unsigned short id){assert(p==editor && id==9);++removed;vector[1]=entries;}
static void close_edit(void *p,int ok){assert(p==editor && ok);++closed;}
static void stock_click(void *l,int r,int c){(void)l;(void)r;(void)c;++delegated;}
static void stock_data(void *l,void *i,int r){(void)l;(void)i;assert(r>=0 && r<expected_stock);}
static int stock_total(void *l){(void)l;return expected_stock;}
static int list_id(const void *l){return 80002+(int)(__INTPTR_TYPE__)l;}
static void *sub(const void *p,int id){return id==24002 || id==24001 || id==24007?(void *)p:0;}
static void label(void *p,const char *s){assert(p==(void *)2);snprintf(caption,sizeof(caption),"%s",s);}
static void picture(void *p,int i,const char *s){assert(p==(void *)2 && i==0);snprintf(picture_path,sizeof(picture_path),"%s",s);}
static void icon_position(void *p,const void *r){assert(p==(void *)2);icon_bounds=*(const struct rect *)r;}
static void position(void *p,const void *r){(void)p;assert(((const struct rect *)r)->w==1030);}
static void log_message(const struct dashborne_host *h,const char *m){(void)h;assert(strstr(m,"widgets:"));}
static int write_json(const char *p,const void *j){assert(j==(void *)3);if(!strcmp(p,WIDGET_LAYOUT_RAM))++ram_calls;return 1;}
int layout_save(void){++save_calls;return test_save_result;}
int layout_prepare(void){return 0;}void layout_cleanup(void){}
int memory_prepare(struct memory_edit *p,void *a,const void *b,const void *c,unsigned int d){(void)p;(void)a;(void)b;(void)c;(void)d;return 0;}
static int restore_count,restore_indices[40];
int memory_apply(struct memory_edit *p){(void)p;return 0;}
int memory_restore(struct memory_edit *p){restore_indices[restore_count++]=(int)(p-edits);return 0;}
static void *get_loader(void){return (void *)6;}
static int reload(void *p){assert(p==(void *)6);assert(restore_count>=2 && restore_indices[0]==17 && restore_indices[1]==18);return 1;}
static void native_resume(void *self){assert(self==(void *)4);assert(short_at(intent+36)==65535);}
static void reset(void) {
    vector[0]=vector[1]=entries;vector[2]=entries+2;memset(state,0,sizeof(state));
    state[0]=1;unsigned short none=65535;memcpy(state+36,&none,2);can_fit=1;added=removed=closed=fail_add=0;
}
int main(void) {
    const struct dashborne_host host={.log=log_message};api=&host;base=image;
    struct dashborne_widget definition={.struct_size=sizeof(definition),.title="Example",.description="Test",.sizes=255};
    providers[0]=(struct widget_provider){&host,&definition,custom_source};
    category=categories;intent=state;caller=&edit_pointer;
    assign_pointer(editor+48,grid);book=get_book;active_grid=get_grid;widgets=get_widgets;nearest=free_cell;
    add_widget=add;remove_widget=remove_entry;close_editor=close_edit;find_sub=sub;set_text=label;set_picture=picture;set_position=position;set_icon_position=icon_position;get_id=list_id;
    for(int i=0;i<3;++i){original_click[i]=stock_click;original_data[i]=stock_data;original_count[i]=stock_total;}
    uintptr stock[17][18];for(int t=0;t<17;++t)for(int i=0;i<18;++i)stock[t][i]=(uintptr)(t*100+i);
    prepare_catalog(&stock[0][0],(void *)5);
    assert(!memcmp(stock,catalog,sizeof(stock)) && catalog[255][15]==5 && !catalog[255][16]);
    registry_publish(custom_source,&definition);assert(!strcmp((char *)catalog[18][0],"Example"));
    const int counts[]={4,12,9};const unsigned int offsets[]={0x3598e0,0x359850,0x3597e4};
    for(int t=0;t<3;++t)for(int c=0;c<counts[t];++c) {
        definition.tab=t;definition.category=c;categories[t]=c;
        const unsigned int special[]={1,4,2,1,1,2,2,2,2};
        unsigned int n=t==2?special[c]:(unsigned int)(1+c%3);expected_stock=(int)n;
        /* Deliberately different source and presentation counts in Special. */
        unsigned int source_count=t==2?6:n;memcpy(base+offsets[t]+c*12+8,&source_count,4);
        if(t==2)memcpy(base+0x336e78+c*8,&n,4);
        void *list=(void *)(__INTPTR_TYPE__)t;
        assert(item_count(list)==expected_stock+8);
        for(int s=0;s<8;++s) {
            reset();int actual=-1;assert(row_provider(t,expected_stock+s,&actual)==&providers[0] && actual==s);
            item_data(list,(void *)2,expected_stock+s);assert(strstr(caption,"Example"));
            assert(strstr(picture_path,"widget-runtime/preview-"));
            assert(icon_bounds.w>0 && icon_bounds.h>0 && icon_bounds.x>=0 && icon_bounds.y>=0);
            assert(icon_bounds.x+icon_bounds.w<=(t==2?510:250) && icon_bounds.y+icon_bounds.h<=(t==2?352:147));
            int error=icon_bounds.w*widget_sizes[s].height-icon_bounds.h*widget_sizes[s].width;
            assert(error<widget_sizes[s].height+widget_sizes[s].width && error>-widget_sizes[s].height-widget_sizes[s].width);
            readable="/preview.png";definition.preview_images[s]=readable;
            item_data(list,(void *)2,expected_stock+s);assert(!strcmp(picture_path,readable));
            readable=0;item_data(list,(void *)2,expected_stock+s);assert(strstr(picture_path,"widget-runtime/preview-"));
            definition.preview_images[s]=0;
            item_data(list,(void *)2,0);assert(!*picture_path);
            definition.struct_size=DASHBORNE_WIDGET_V1_SIZE;
            item_data(list,(void *)2,expected_stock+s);assert(strstr(picture_path,"widget-runtime/preview-"));
            definition.struct_size=sizeof(definition);
            item_click(list,expected_stock+s,0);assert(added==1 && closed==1 && last.type==widget_sizes[s].type && registry_source(custom)==custom_source);
            reset();can_fit=0;item_click(list,expected_stock+s,0);assert(!added && !removed);
        }
        reset();item_click(list,0,0);assert(!added);assert(!row_provider(t,expected_stock+8,&c));
    }
    definition.tab=2;definition.category=1;categories[2]=1;expected_stock=4;
    reset();widget(old,5,9,&stock_source);entries[0]=old;vector[1]=entries+1;unsigned short id=9;memcpy(state+36,&id,2);fail_add=1;
    item_click((void *)2,4,0);assert(removed==1 && added==2 && !closed && !registry_source(custom));
    reset();memcpy(state+36,&id,2);item_click((void *)2,4,0);assert(!added && !removed);
    memcpy(state+38,&custom_source,2);picker_resume=native_resume;resume_picker((void *)4);assert(short_at(state+36)==id);
    json_write=write_json;assert(write_layout("/data/unrelated.json",(void *)3)==1 && !save_calls);
    assert(write_layout(WIDGET_LAYOUT_RAM,(void *)3)==1 && save_calls==1 && ram_calls==1);
    test_save_result=-1;assert(!write_layout(WIDGET_LAYOUT_RAM,(void *)3));
    persistence_busy=1;int before=ram_calls;assert(!write_layout(WIDGET_LAYOUT_RAM,(void *)3) && ram_calls==before);
    registry_publish(custom_source,0);assert(!strcmp((char *)catalog[18][0],"Unavailable widget"));
    /* An actual short allocation catches accidental reads beyond the old ABI. */
    union {void *align;unsigned char bytes[DASHBORNE_WIDGET_V1_SIZE];} legacy;
    definition.struct_size=DASHBORNE_WIDGET_V1_SIZE;
    memcpy(legacy.bytes,&definition,sizeof(legacy.bytes));
    assert(strstr(preview_picture((const struct dashborne_widget *)legacy.bytes,DBW_6X2),"preview-6x2.png"));
    loader=get_loader;loader_load=reload;path_edits=13;registered=1;edit_count=20;
    assert(!registry_cleanup() && !registered && !api && restore_count==22);
    puts("Widget picker: 25 categories × 8 sizes, bounds, stock delegation, replacement rollback, re-edit, card-only persistence and cleanup passed");
}
