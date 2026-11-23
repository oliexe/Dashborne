/* Inspected 6.11.2 / FlyThings adapter. All edits are scoped to this process. */
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
static unsigned char original_table[TABLE_BYTES],*table;
static char paths[4][145][256];
static int table_changed,table_prot,got_changed,got_prot;
static void **font_got;
static void *(*font_create)(const char *,unsigned int,unsigned int);
static void (*font_destroy)(void *);
static unsigned int size_of(const char *s){unsigned int n=0;while(s[n])n++;return n;}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
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
static void *create_font(const char *name,unsigned int size,unsigned int style){
 if(!name||!*name||equal(name,"a")||equal(name,"digits"))name="Inter-Regular";
 return font_create(name,size,style);
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
 font_got=(void **)((unsigned char *)base+0xd19a4);
 if(*font_got!=(void *)font_create)return 0;
 got_prot=permissions(font_got,sizeof(void *));if(got_prot<0||protect(font_got,sizeof(void *),got_prot|2))return 0;
 *font_got=(void *)create_font;got_changed=1;
 return !protect(font_got,sizeof(void *),got_prot);
}
static int patch_theme(void *base){
 table=(unsigned char *)base+0x3360f8;
 table_prot=permissions(table,TABLE_BYTES);if(table_prot<0)return 0;
 copy(original_table,table,TABLE_BYTES);
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
  const unsigned int dark[]={0xff171717,0xf0f0f0,0xffffff,0xa2a3a5,0xa2a3a5,0xa2a3a5,0x757575};
  const unsigned int light[]={0xfff0f0f0,0x171717,0x171717,0x757575,0x757575,0x757575,0xa2a3a5};
  copy(colors,(t&1)?light:dark,sizeof(dark));
 }
 return !protect(table,TABLE_BYTES,table_prot);
}
static int layout(void){
 char input[256],output[256],pending[256];unsigned int n=0,pn=0,on=0;unsigned char *out=0;
 if(snprintf(input,sizeof(input),"%s/assets/layout.atp",host->extension_directory)>=(int)sizeof(input)||
 snprintf(output,sizeof(output),"%s/generated.ftu",host->extension_directory)>=(int)sizeof(output)||
 snprintf(pending,sizeof(pending),"%s/generated.next",host->extension_directory)>=(int)sizeof(pending))return 0;
 unsigned char *b=load("/res/ui/dash1act.ftu",&n),*p=load(input,&pn);
 int ok=b&&p&&tesla_layout(b,n,p,pn,&out,&on);free(b);free(p);if(!ok)return 0;
 int fd=open(pending,1|0100|01000,0600);unsigned int at=0;
 if(fd<0){free(out);return 0;}
 while(at<on){int wrote=write(fd,out+at,on-at);if(wrote<=0)break;at+=(unsigned int)wrote;}
 ok=at==on&&!fsync(fd);if(close(fd))ok=0;free(out);
 if(!ok||rename(pending,output)){unlink(pending);return 0;}
 return !host->overlay_file(host,"generated.ftu","/res/ui/dash1act.ftu");
}
static int deactivate(void){
 if(got_changed){
  if(protect(font_got,sizeof(void *),got_prot|2))return -1;
  *font_got=(void *)font_create;
  if(protect(font_got,sizeof(void *),got_prot))return -1;
  got_changed=0;
 }
 if(table_changed){
  if(protect(table,TABLE_BYTES,table_prot|2))return -1;
  copy(table,original_table,TABLE_BYTES);
  if(protect(table,TABLE_BYTES,table_prot))return -1;
  table_changed=0;
 }
 if(host)host->log(host,"Actually Tesla UI: runtime theme and font routing restored");
 return 0;
}
static int activate(const struct dashborne_host *api){
 host=api;unsigned int n;struct dl_info official={0},runtime={0};
 /* Entire binary identities guard the private table, registry and GOT offsets. */
 if(file_crc("/res/lib/libzkgui.so",&n)!=0x9de99542u||n!=3464120u||
 file_crc("/lib/libeasyui.so",&n)!=0xece0feecu||n!=826052u){host->log(host,"Actually Tesla UI: unsupported app/runtime fingerprint");return -1;}
 void *config=host->resolve(host->official_library,"_ZN12ThemeManager9getConfigEj");
 void *font=host->resolve(host->ui_runtime,"zk_font_create");
 if(!config||!font||!dladdr(config,&official)||!dladdr(font,&runtime))return -1;
 if(!layout()){host->log(host,"Actually Tesla UI: layout mismatch or conflict; disable other layout patches");return -1;}
 if(!patch_theme(official.base)||!patch_font(runtime.base)){host->log(host,"Actually Tesla UI: theme/font adapter failed");return -1;}
 host->log(host,"Actually Tesla UI: light/dark widget theme, layouts and Inter active");return 0;
}
static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(struct dashborne_extension),"actually-tesla-ui",activate,deactivate};
__attribute__((visibility("default"))) const struct dashborne_extension *dashborne_extension_v1(void){return &extension;}
