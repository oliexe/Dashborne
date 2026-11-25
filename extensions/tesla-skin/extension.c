#include "stock_compat.h"
/* Stock 6.11.2 / FlyThings theme adapter. State belongs to this UI process. */
#include "dashborne_extension.h"
#include "layout_patch.h"
typedef __SIZE_TYPE__ usize;
typedef __UINTPTR_TYPE__ uptr;
extern int open(const char *,int,...),read(int,void *,unsigned int),write(int,const void *,unsigned int),close(int),fsync(int);
extern int rename(const char *,const char *),unlink(const char *),access(const char *,int),mprotect(void *,usize,int);
extern void *malloc(usize);extern void free(void *);
extern unsigned long crc32(unsigned long,const unsigned char *,unsigned int);
extern int snprintf(char *,usize,const char *,...),sscanf(const char *,const char *,...);
extern void *fopen(const char *,const char *);extern char *fgets(char *,int,void *);extern int fclose(void *);
struct dl_info {const char *filename;void *base;const char *symbol;void *address;};
extern int dladdr(const void *,struct dl_info *);
#define TABLE_BYTES 2320
static const struct dashborne_host *host;
static unsigned char original_table[TABLE_BYTES],applied_table[TABLE_BYTES],*table;
static char paths[4][145][256];
static int table_changed,table_prot;
static void *(*font_create)(const char *,unsigned int,unsigned int);
static void (*font_destroy)(void *);
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static unsigned int size_of(const char *s){unsigned int n=0;while(s[n])n++;return n;}
static int starts(const char *a,const char *b){while(*b)if(*a++!=*b++)return 0;return 1;}
static void copy(void *a,const void *b,usize n){unsigned char *x=a;const unsigned char *y=b;while(n--)*x++=*y++;}
static unsigned int file_crc(const char *path,unsigned int *size){
 int fd=open(path,0);unsigned char b[8192];unsigned long crc=0;int n;*size=0;
 if(fd<0)return 0;
 while((n=read(fd,b,sizeof(b)))>0){*size+=(unsigned int)n;crc=crc32(crc,b,(unsigned int)n);}
 close(fd);if(n<0)*size=0;return (unsigned int)crc;
}
static unsigned char *load(const char *path,unsigned int *size){
 int fd=open(path,0);if(fd<0)return 0;
 unsigned char *b=malloc(2097153);unsigned int n=0;int got=0;
 if(b)while(n<2097153&&(got=read(fd,b+n,2097153-n))>0)n+=(unsigned int)got;
 close(fd);if(!b||got<0||n>2097152){free(b);return 0;}*size=n;return b;
}
static int permissions(void *address,usize length){
 void *f=fopen("/proc/self/maps","r");if(!f)return -1;
 char line[512],mode[5];unsigned long lo,hi;int result=-1;
 while(fgets(line,sizeof(line),f))if(sscanf(line,"%lx-%lx %4s",&lo,&hi,mode)==3&&
 (uptr)address>=lo&&(uptr)address+length<=hi){result=(mode[0]=='r'?1:0)|(mode[1]=='w'?2:0)|(mode[2]=='x'?4:0);break;}
 fclose(f);return result;
}
static int protect(void *address,usize length,int mode){
 uptr lo=(uptr)address&~(uptr)4095,hi=((uptr)address+length+4095)&~(uptr)4095;
 return mprotect((void *)lo,hi-lo,mode);
}
static int patch_font(void *base){
 char path[256];
 *(void **)(&font_create)=host->resolve(host->ui_runtime,"zk_font_create");
 *(void **)(&font_destroy)=host->resolve(host->ui_runtime,"zk_font_destroy");
 if(!font_create||!font_destroy)return 0;
 void *probe=font_create("Inter-Regular",24,0);
 if(!probe){
  if(snprintf(path,sizeof(path),"%s/assets/Inter-Regular.ttf",host->extension_directory)>=(int)sizeof(path))return 0;
  void (*register_font)(const char *)=(void (*)(const char *))((unsigned char *)base+0x86464);
  register_font(path);probe=font_create("Inter-Regular",24,0);
 }
 if(!probe)return 0;font_destroy(probe);
 return 1;
}
static int patch_theme(void *base){
 table=(unsigned char *)base+DB_STOCK_OFFSET(0x3360f8,0x3356b0);
 table_prot=permissions(table,TABLE_BYTES);if(table_prot<0)return 0;
 copy(original_table,table,TABLE_BYTES);
 for(unsigned int t=0;t<4;t++)for(unsigned int i=0;i<145;i++)paths[t][i][0]=0;
 unsigned int replacements=0;
 for(unsigned int t=0;t<4;t++)for(unsigned int i=0;i<145;i++){
  unsigned int off=i*4;
  if(off==0x38||(off>=0x208&&off<0x224))continue;
  const char *name=*(const char **)(table+t*580+off);
  if((uptr)name<(uptr)base+0x28b400||(uptr)name>=(uptr)base+0x3184e1||!starts(name,"/res/ui/"))continue;
  if(!size_of(name)||name[size_of(name)-1]=='/')continue;
  int n=snprintf(paths[t][i],256,"%s/assets/ui/%s",host->extension_directory,name+8);
  if(n<0||n>=256)return 0;
  if(access(paths[t][i],4)){paths[t][i][0]=0;continue;}
  replacements++;
 }
 if(replacements<250)return 0;
 if(protect(table,TABLE_BYTES,table_prot|2))return 0;
 table_changed=1;
 for(unsigned int t=0;t<4;t++){
  for(unsigned int i=0;i<145;i++)if(paths[t][i][0])*(const char **)(table+t*580+i*4)=paths[t][i];
  unsigned int *colors=(unsigned int *)(table+t*580+0x208);
  const unsigned int dark[]={0xff17181a,0xf4f4f4,0xffffff,0xa2a5a9,0xa2a5a9,0xa2a5a9,0x686b70};
  const unsigned int light[]={0xfff1f2f3,0x171a20,0x171a20,0x62666d,0x62666d,0x62666d,0xa4a7ac};
  copy(colors,(t&1)?light:dark,sizeof(dark));
 }
 copy(applied_table,table,TABLE_BYTES);
 return !protect(table,TABLE_BYTES,table_prot);
}
#define true 1
#define false 0
struct style {int id,has_text,has_bg;unsigned int bg,text[5];const char *background,*pics[5],*progress;};
#include "styles.inc"
struct page {
 const char *name,*vtable_name;void **vtable;
 const struct style *styles[2];unsigned int counts[2];
 char layout[2][256];void *live;const void *last_theme;
 void (*old_intent)(void *,const void *);void (*old_create)(void *),(*old_resume)(void *);
 int (*old_timer)(void *,int);int installed;unsigned int intent_slot,resume_slot;
};
#define PAGE(n,m,is,rs) {#n,m,0,{n##_dash,n##_light},{sizeof(n##_dash)/sizeof(struct style),sizeof(n##_light)/sizeof(struct style)},{{0}},0,0,0,0,0,0,0,is,rs}
static struct page pages[]={
 PAGE(dash1act,"_ZTV16dash1actActivity",8,9),
 PAGE(awlsettings,"_ZTV19awlsettingsActivity",8,9),
 PAGE(widgetEdit,"_ZTV18widgetEditActivity",8,9),
 PAGE(pageEdit,"_ZTV16pageEditActivity",8,9),
 PAGE(dialog,"_ZTV14dialogActivity",8,9),
 PAGE(btCommanderPair,"_ZTV23btCommanderPairActivity",8,9),
 /* The keyboard vtable orders resume/pause before intent. */
 PAGE(UserIme,"_ZTV15UserImeActivity",10,8)
};
#define PAGE_COUNT (sizeof(pages)/sizeof(pages[0]))
static void *(*active_theme)(void),*(*find)(void *,int);
static void (*background_color)(void *,unsigned int),(*background_pic)(void *,const char *);
static void (*text_color)(void *,int,unsigned int),(*button_pic)(void *,int,const char *);
static void (*progress_pic)(void *,const char *);
static int light(void) {const unsigned char *c=active_theme();return c && (*(const unsigned int *)(c+0x208)&0xffffffu)>0x808080u;}
static struct page *page_for(void *self) {
 if(!self)return 0;void **v=*(void ***)self;
 for(unsigned int i=0;i<PAGE_COUNT;i++)if(v==pages[i].vtable+2)return &pages[i];
 return 0;
}
static void **file_got;
static void *(*file_load)(const char *,unsigned int *);
static int file_prot,file_changed;
static void *load_layout(const char *path,unsigned int *size) {
 if(path && starts(path,"/res/ui/"))for(unsigned int i=0;i<PAGE_COUNT;i++) {
  char expected[80];snprintf(expected,sizeof(expected),"/res/ui/%s.ftu",pages[i].name);
  if(equal(path,expected))return file_load(pages[i].layout[light()],size);
 }
 return file_load(path,size);
}
static int install_file_route(void *base) {
 *(void **)(&file_load)=host->resolve(host->ui_runtime,"_ZN5utils12loadFileDataEPKcRj");
 file_got=(void **)((unsigned char *)base+0xd0ef8);
 if(!file_load || *file_got!=(void *)file_load)return 0;
 file_prot=permissions(file_got,sizeof(void *));
 if(file_prot<0 || protect(file_got,sizeof(void *),file_prot|2))return 0;
 *file_got=(void *)load_layout;file_changed=1;
 return !protect(file_got,sizeof(void *),file_prot);
}
static void apply_styles(struct page *p,void *self) {
 const void *theme=active_theme();if(p->last_theme==theme)return;
 int mode=light();
 for(unsigned int i=0;i<p->counts[mode];++i) {
  const struct style *s=&p->styles[mode][i];void *control=find(self,s->id);if(!control)continue;
  if(s->has_bg)background_color(control,s->bg);
  if(s->has_text)for(int status=0;status<5;status++)text_color(control,status,s->text[status]);
  if(s->background[0])background_pic(control,s->background);
  for(int status=0;status<5;status++)if(s->pics[status][0])button_pic(control,status,s->pics[status]);
  if(s->progress[0])progress_pic(control,s->progress);
 }
 p->last_theme=theme;
}
static void page_create(void *self) {
 struct page *p=page_for(self);if(!p)return;
 p->live=self;p->last_theme=0;p->old_create(self);apply_styles(p,self);
}
static void page_resume(void *self) {
 struct page *p=page_for(self);if(!p)return;p->old_resume(self);p->last_theme=0;apply_styles(p,self);
}
static void page_intent(void *self,const void *intent) {
 struct page *p=page_for(self);if(!p)return;
 p->old_intent(self,intent);p->last_theme=0;apply_styles(p,self);
}
static int page_timer(void *self,int id) {
 struct page *p=page_for(self);if(!p)return 1;
 int result=p->old_timer(self,id);apply_styles(p,self);return result;
}
#include "image_routes.inc"
#define IMAGE_COUNT (sizeof(image_assets)/sizeof(image_assets[0]))
static char image_paths[IMAGE_COUNT][256];
static void **image_got;
static int (*image_load)(void *,const char *,int,int);
static int image_prot,image_changed;
static const char *image_path(const char *path) {
 if(!path)return path;
 const char *tail=path;
 if(starts(tail,"/res/ui/"))tail+=8;
 if(starts(tail,"dash/"))tail+=5;
 else if(starts(tail,"light/"))tail+=6;
 else return path;
 int mode=light();
 for(unsigned int i=0;i<IMAGE_COUNT;i++) {
  const char *name=image_assets[i];
  if((starts(name,"light/")?1:0)!=mode)continue;
  if(equal(tail,name+(mode?6:5)))return image_paths[i];
 }
 return path;
}
static int load_image(void *self,const char *path,int scale,int flags) {
 return image_load(self,image_path(path),scale,flags);
}
static int install_image_route(void *base) {
 for(unsigned int i=0;i<IMAGE_COUNT;i++) {
  int n=snprintf(image_paths[i],256,"%s/assets/ui/%s",host->extension_directory,image_assets[i]);
  if(n<0 || n>=256 || access(image_paths[i],4))return 0;
 }
 *(void **)(&image_load)=host->resolve(host->ui_runtime,"_ZN13BitmapWrapper18loadBitmapFromFileEPKcii");
 image_got=(void **)((unsigned char *)base+0xd1420);
 if(!image_load || *image_got!=(void *)image_load)return 0;
 image_prot=permissions(image_got,sizeof(void *));
 if(image_prot<0 || protect(image_got,sizeof(void *),image_prot|2))return 0;
 *image_got=(void *)load_image;image_changed=1;
 return !protect(image_got,sizeof(void *),image_prot);
}
static int save_file(const char *path,const unsigned char *bytes,unsigned int n) {
 int fd=open(path,1|0100|01000,0600);if(fd<0)return 0;unsigned int at=0;
 while(at<n){int result=write(fd,bytes+at,n-at);if(result<=0)break;at+=(unsigned int)result;}
 int ok=at==n&&!fsync(fd);if(close(fd))ok=0;return ok;
}
static int prepare_pages(void) {
 for(unsigned int i=0;i<PAGE_COUNT;i++) {
  struct page *p=&pages[i];char path[256];unsigned int n=0;
  if(snprintf(path,sizeof(path),"/res/ui/%s.ftu",p->name)>=(int)sizeof(path))return 0;
  unsigned char *base=load(path,&n);if(!base)return 0;
  for(int mode=0;mode<2;mode++) {
   unsigned int pn=0,on=0;unsigned char *out=0;
   int len=snprintf(path,sizeof(path),"%s/assets/%s-%s.atp",host->extension_directory,p->name,mode?"light":"dash");
   if(len<0||len>=(int)sizeof(path)){free(base);return 0;}
   unsigned char *patch=load(path,&pn);
   int ok=patch&&tesla_layout(base,n,patch,pn,&out,&on);free(patch);
   if(!ok){free(base);host->log(host,p->name);return 0;}
   len=snprintf(path,sizeof(path),"%s/%s-%d.ftu",host->extension_directory,p->name,mode);
   if(len<0||len>=(int)sizeof(path)||!save_file(path,out,on)){free(out);free(base);return 0;}
   free(out);
   len=snprintf(p->layout[mode],sizeof(p->layout[mode]),"%s",path);
   if(len<0||len>=(int)sizeof(p->layout[mode])){free(base);return 0;}
  }
  free(base);
 }
 return 1;
}
static int owned_callback(void *callback) {
 if(host->owns_callback && host->owns_callback(host,callback))return 1;
 struct dl_info candidate={0},core={0};
 return callback && host->log && dladdr(callback,&candidate) &&
        dladdr((const void *)host->log,&core) && candidate.base==core.base &&
        (permissions(callback,1)&4);
}
static int install_pages(void *base) {
 (void)base;
 for(unsigned int i=0;i<PAGE_COUNT;i++) {
  struct page *p=&pages[i];p->vtable=host->resolve(host->official_library,p->vtable_name);
  const unsigned int slots[]={6,7,p->resume_slot,p->intent_slot};
  if(!p->vtable || permissions(p->vtable,44)<0)return 0;
  for(int k=0;k<4;k++) {
   void *callback=p->vtable[slots[k]];
   char symbol[96];
   const char *methods[]={"8onCreateEv","7onTimerEi","8onResumeEv","8onIntentEPK6Intent"};
   if(snprintf(symbol,sizeof(symbol),"_ZN%s%s",p->vtable_name+4,methods[k])>=(int)sizeof(symbol))return 0;
   void *expected=host->resolve(host->official_library,symbol);
   if(!expected || (callback!=expected && !owned_callback(callback)))return 0;
  }
  int mode=permissions(p->vtable,44);if(protect(p->vtable,44,mode|2))return 0;
  p->old_intent=(void (*)(void *,const void *))p->vtable[p->intent_slot];p->old_create=(void (*)(void *))p->vtable[6];
  p->old_timer=(int (*)(void *,int))p->vtable[7];p->old_resume=(void (*)(void *))p->vtable[p->resume_slot];
  p->installed=mode+1;p->vtable[6]=(void *)page_create;
  p->vtable[7]=(void *)page_timer;p->vtable[p->intent_slot]=(void *)page_intent;p->vtable[p->resume_slot]=(void *)page_resume;
  if(protect(p->vtable,44,mode))return 0;
 }
 return 1;
}
static int deactivate(void) {
 for(unsigned int i=PAGE_COUNT;i>0;i--) {
  struct page *p=&pages[i-1];if(!p->installed)continue;
  if((p->vtable[6]!=(void *)page_create && p->vtable[6]!=(void *)p->old_create) ||
     (p->vtable[7]!=(void *)page_timer && p->vtable[7]!=(void *)p->old_timer) ||
     (p->vtable[p->intent_slot]!=(void *)page_intent && p->vtable[p->intent_slot]!=(void *)p->old_intent) ||
     (p->vtable[p->resume_slot]!=(void *)page_resume && p->vtable[p->resume_slot]!=(void *)p->old_resume))return -1;
  int mode=p->installed-1;if(protect(p->vtable,44,mode|2))return -1;
  p->vtable[6]=(void *)p->old_create;
  p->vtable[7]=(void *)p->old_timer;p->vtable[p->intent_slot]=(void *)p->old_intent;p->vtable[p->resume_slot]=(void *)p->old_resume;
  if(protect(p->vtable,44,mode))return -1;p->installed=0;p->live=0;p->last_theme=0;
 }
 if(image_changed){
  if(*image_got!=(void *)load_image && *image_got!=(void *)image_load)return -1;
  if(protect(image_got,sizeof(void *),image_prot|2))return -1;
  *image_got=(void *)image_load;
  if(protect(image_got,sizeof(void *),image_prot))return -1;image_changed=0;
 }
 if(file_changed){
  if(*file_got!=(void *)load_layout && *file_got!=(void *)file_load)return -1;
  if(protect(file_got,sizeof(void *),file_prot|2))return -1;
  *file_got=(void *)file_load;
  if(protect(file_got,sizeof(void *),file_prot))return -1;file_changed=0;
 }
 if(table_changed){
  for(unsigned int i=0;i<TABLE_BYTES;i++)if(table[i]!=applied_table[i] && table[i]!=original_table[i])return -1;
  if(protect(table,TABLE_BYTES,table_prot|2))return -1;
  copy(table,original_table,TABLE_BYTES);
  if(protect(table,TABLE_BYTES,table_prot))return -1;table_changed=0;
 }
 host=0;return 0;
}
static int activate(const struct dashborne_host *api) {
 if(!api||api->abi_version!=DASHBORNE_EXTENSION_ABI||api->struct_size<sizeof(*api)||!api->resolve||!api->log||host)return -1;
 host=api;unsigned int n;struct dl_info official={0},runtime={0};
 if(!db_stock_version()||
 file_crc("/lib/libeasyui.so",&n)!=0xece0feecu||n!=826052u){host->log(host,"tesla-skin: unsupported app/runtime fingerprint");return -1;}
 void *config=host->resolve(host->official_library,"_ZN12ThemeManager9getConfigEj");
 void *font=host->resolve(host->ui_runtime,"zk_font_create");
 if(!config||!font||!dladdr(config,&official)||!dladdr(font,&runtime))return -1;
#define UI(v,s) do{*(void **)(&(v))=host->resolve(host->ui_runtime,s);if(!(v))return -1;}while(0)
 UI(find,"_ZN7BaseApp15findControlByIDEi");
 UI(background_color,"_ZN6ZKBase18setBackgroundColorEj");UI(background_pic,"_ZN6ZKBase16setBackgroundPicEPKc");
 UI(text_color,"_ZN10ZKTextView18setTextStatusColorEij");UI(button_pic,"_ZN8ZKButton18setButtonStatusPicEiPKc");
 UI(progress_pic,"_ZN9ZKSeekBar14setProgressPicEPKc");
#undef UI
 *(void **)(&active_theme)=host->resolve(host->official_library,"_ZN12ThemeManager9getActiveEv");
 if(!active_theme||!prepare_pages()){host->log(host,"tesla-skin: layout compatibility check failed");return -1;}
 if(!patch_theme(official.base)){host->log(host,"tesla-skin: theme table rejected");return -1;}
 if(!patch_font(runtime.base)){host->log(host,"tesla-skin: font registration failed");return -1;}
 if(!install_file_route(runtime.base)){host->log(host,"tesla-skin: layout route rejected");return -1;}
 if(!install_pages(official.base)){host->log(host,"tesla-skin: activity hooks rejected");return -1;}
 if(!install_image_route(runtime.base)){host->log(host,"tesla-skin: image route rejected");return -1;}
 host->log(host,"tesla-skin: stock dashboard, settings and picker skin active");return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
 static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(struct dashborne_extension),"tesla-skin",activate,deactivate};return &extension;
}
