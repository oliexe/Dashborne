#include <assert.h>
#include <string.h>
#include <stdlib.h>
#define fopen mock_open
#define fgets mock_gets
#define fclose mock_close
#define mprotect mock_protect
#include "../extension.c"
#undef fopen
#undef fgets
#undef fclose
#undef mprotect
extern int puts(const char *);
static int map_read,fail_protect;
void *mock_open(const char *path,const char *mode){(void)mode;assert(!strcmp(path,"/proc/self/maps"));map_read=0;return (void *)1;}
char *mock_gets(char *s,int n,void *file){(void)file;if(map_read++)return 0;snprintf(s,(usize)n,"0-%lx rw-p",~0ul);return s;}
int mock_close(void *file){(void)file;return 0;}
int mock_protect(void *address,usize length,int mode){(void)address;(void)length;(void)mode;return fail_protect?-1:0;}
static unsigned char colors[580];
static void *theme(void){return colors;}
static const char *requested;
static void *read_layout(const char *path,unsigned int *size){requested=path;*size=17;return (void *)1;}
static int image_read(void *self,const char *path,int scale,int flags){
 assert(self==(void *)23 && scale==2 && flags==7);requested=path;return 11;
}
static void *tables[7][11];
static int calls,lookups;
static void created(void *self){(void)self;calls++;}
static int timed(void *self,int id){(void)self;calls++;return id;}
static void intended(void *self,const void *intent){(void)self;(void)intent;calls++;}
static void resumed(void *self){(void)self;calls++;}
static void *lookup(void *self,int id){(void)self;(void)id;lookups++;return 0;}
static const char *named(void *self){(void)self;return "native-name";}
static void *resolve(void *library,const char *name){(void)library;for(unsigned i=0;i<PAGE_COUNT;i++)if(!strcmp(name,pages[i].vtable_name))return tables[i];if(strstr(name,"8onCreateEv"))return (void *)created;if(strstr(name,"7onTimerEi"))return (void *)timed;if(strstr(name,"8onResumeEv"))return (void *)resumed;if(strstr(name,"8onIntentEPK6Intent"))return (void *)intended;return 0;}
static int owns(const struct dashborne_host *api,const void *callback){(void)api;return callback==(void *)created||callback==(void *)timed||callback==(void *)resumed||callback==(void *)intended;}
int main(void){
 active_theme=theme;file_load=read_layout;
 for(unsigned i=0;i<PAGE_COUNT;i++)for(int mode=0;mode<2;mode++)snprintf(pages[i].layout[mode],256,"/card/%s-%d.ftu",pages[i].name,mode);
 unsigned int size=0;
 *(unsigned int *)(colors+0x208)=0xff17181a;
 assert(load_layout("/res/ui/dash1act.ftu",&size)==(void *)1 && size==17);
 assert(!strcmp(requested,"/card/dash1act-0.ftu"));
 *(unsigned int *)(colors+0x208)=0xfff1f2f3;
 load_layout("/res/ui/widgetEdit.ftu",&size);assert(!strcmp(requested,"/card/widgetEdit-1.ftu"));
 const char *unrelated="/res/ui/widgetEdit.ftu.backup";
 load_layout(unrelated,&size);assert(requested==unrelated);
 unrelated="/res/ui/icons/vehicle.png";load_layout(unrelated,&size);assert(requested==unrelated);
 load_layout(0,&size);assert(!requested);
 for(unsigned int i=0;i<IMAGE_COUNT;i++)snprintf(image_paths[i],256,"/card/assets/ui/%s",image_assets[i]);
 *(unsigned int *)(colors+0x208)=0xfff1f2f3;
 assert(!strcmp(image_path("/res/ui/dash/preview/special/tripPreview.png"),"/card/assets/ui/light/preview/special/tripPreview.png"));
 *(unsigned int *)(colors+0x208)=0xff17181a;
 assert(!strcmp(image_path("light/elements/btn_back_bg_n.png"),"/card/assets/ui/dash/elements/btn_back_bg_n.png"));
 image_load=image_read;
 assert(load_image((void *)23,"/res/ui/light/preview/special/tripPreview.png",2,7)==11);
 assert(!strcmp(requested,"/card/assets/ui/dash/preview/special/tripPreview.png"));
 unrelated="/res/ui/dash/vehicle/brake-warning.png";
 assert(image_path(unrelated)==unrelated && image_path(0)==0);
 unrelated="/res/ui/dash/preview/special/tripPreview.png.extra";assert(image_path(unrelated)==unrelated);
 struct dashborne_host api={.resolve=resolve,.owns_callback=owns};host=&api;
 for(unsigned i=0;i<PAGE_COUNT;i++){tables[i][5]=(void *)named;tables[i][6]=(void *)created;tables[i][7]=(void *)timed;tables[i][9]=(void *)named;tables[i][pages[i].resume_slot]=(void *)resumed;tables[i][pages[i].intent_slot]=(void *)intended;}
 assert(install_pages(0));
 void *image_slot=(void *)load_image;image_got=&image_slot;image_prot=1;image_changed=1;
 for(unsigned i=0;i<PAGE_COUNT;i++){
  assert(tables[i][5]==(void *)named); /* Native navigation identity must never change. */
  assert(tables[i][6]==(void *)page_create);
 }
 find=lookup;
 void **object=tables[0]+2;
 page_create(&object);assert(calls==1 && lookups>0);
 int initial=lookups;
 assert(page_timer(&object,17)==17 && calls==2 && lookups==initial);
 page_intent(&object,0);assert(calls==3 && lookups>initial);initial=lookups;
 page_resume(&object);assert(calls==4 && lookups>initial);
 tables[6][6]=(void *)named;
 assert(deactivate()==-1 && tables[6][6]==(void *)named);
 tables[6][6]=(void *)page_create;
 fail_protect=1;assert(deactivate()==-1);fail_protect=0;
 assert(!deactivate());
 for(unsigned i=0;i<PAGE_COUNT;i++){assert(tables[i][5]==(void *)named);assert(tables[i][6]==(void *)created);assert(tables[i][7]==(void *)timed);assert(tables[i][pages[i].resume_slot]==(void *)resumed);assert(tables[i][pages[i].intent_slot]==(void *)intended);}
 assert(tables[6][9]==(void *)named); /* Keyboard onPause is not a style hook. */
 assert(!host && !image_changed && image_slot==(void *)image_read);
 puts("Tesla Skin: exact layout routing, native navigation identity, hook conflicts and rollback passed");
 return 0;
}
