/* Resource adapter: authored overrides first, device-local palette fallback second. */
#include "theme_engine.h"
typedef __SIZE_TYPE__ usize;
extern void *malloc(usize), *calloc(usize,usize);extern void free(void *);
extern int open(const char *,int,...),read(int,void *,unsigned int),write(int,const void *,unsigned int),close(int),unlink(const char *),getpid(void);
extern int snprintf(char *,usize,const char *,...);
extern unsigned long crc32(unsigned long,const unsigned char *,unsigned int),compressBound(unsigned long);
extern int uncompress(unsigned char *,unsigned long *,const unsigned char *,unsigned long);
extern int compress2(unsigned char *,unsigned long *,const unsigned char *,unsigned long,int);
static unsigned int be(const unsigned char *b){return (unsigned int)b[0]<<24|b[1]<<16|b[2]<<8|b[3];}
static void put(unsigned char *b,unsigned int n){for(int i=3;i>=0;i--){b[i]=n;n>>=8;}}
static void copy(unsigned char *a,const unsigned char *b,unsigned int n){while(n--)*a++=*b++;}
static int eq(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int prefix(const char *a,const char *b){while(*b)if(*a++!=*b++)return 0;return 1;}
static void pixel(unsigned char *rgb,int source_light,unsigned int bg,unsigned int fg){
 int lo=rgb[0],hi=lo;for(int i=1;i<3;i++){if(rgb[i]<lo)lo=rgb[i];if(rgb[i]>hi)hi=rgb[i];}
 if(hi-lo>20)return; /* Status, warnings and action colors retain their meaning. */
 int value=(rgb[0]+rgb[1]+rgb[2])/3;if(source_light)value=255-value;
 for(int i=0;i<3;i++){int a=(bg>>(16-i*8))&255,b=(fg>>(16-i*8))&255;rgb[i]=(unsigned char)((a*(255-value)+b*value+127)/255);}
}
static unsigned int chunk(unsigned char *out,const char *type,const unsigned char *data,unsigned int n){
 put(out,n);copy(out+4,(const unsigned char *)type,4);copy(out+8,data,n);put(out+8+n,(unsigned int)crc32(0,out+4,n+4));return n+12;
}
int theme_png(const unsigned char *in,unsigned int size,int source_light,unsigned int bg,unsigned int fg,unsigned char **out,unsigned int *out_n){
 *out=0;*out_n=0;if(!in||size<57||size>8388608)return 0;
 const unsigned char signature[]={137,80,78,71,13,10,26,10};
 for(int i=0;i<8;i++)if(in[i]!=signature[i])return 0;
 unsigned int w=0,h=0,type=0,depth=0,at=8,idat_n=0,palette_n=0;int ended=0;
 while(at<size){
  if(size-at<12)return 0;unsigned int n=be(in+at);if(n>size-at-12)return 0;
  const unsigned char *t=in+at+4,*d=in+at+8;
  if(be(in+at+n+8)!=(unsigned int)crc32(0,t,n+4))return 0;
  if(at==8){if(n!=13||be(t)!=0x49484452)return 0;w=be(d);h=be(d+4);depth=d[8];type=d[9];
   if(!w||!h||w>4096||h>4096||w*h>4194304||d[10]||d[11]||d[12])return 0;
  }else if(be(t)==0x49484452)return 0;
  if(be(t)==0x49444154)idat_n+=n;
  if(be(t)==0x504c5445){if(!n||n%3||n>768)return 0;palette_n=n;}
  if(be(t)==0x49454e44){if(n||at+12!=size)return 0;ended=1;}
  at+=n+12;
 }
 if(!ended||!idat_n)return 0;
 if(type==3){
  if(!palette_n||(depth!=1&&depth!=2&&depth!=4&&depth!=8))return 0;
  unsigned char *b=malloc(size);if(!b)return 0;copy(b,in,size);
  for(at=8;at<size;at+=be(b+at)+12)if(be(b+at+4)==0x504c5445){
   unsigned int n=be(b+at);for(unsigned int i=0;i<n;i+=3)pixel(b+at+8+i,source_light,bg,fg);
   put(b+at+8+n,(unsigned int)crc32(0,b+at+4,n+4));
  }
  *out=b;*out_n=size;return 1;
 }
 if(w*h>1048576||depth!=8||type!=6)return 0;
 unsigned int channels=type==6?4:3,row=w*channels,total=(row+1)*h;
 unsigned char *packed=malloc(idat_n),*raw=malloc(total),*result=0;int ok=0;
 if(!packed||!raw)goto done;
 unsigned int used=0;for(at=8;at<size;at+=be(in+at)+12)if(be(in+at+4)==0x49444154){unsigned int n=be(in+at);copy(packed+used,in+at+8,n);used+=n;}
 unsigned long expanded=total;if(uncompress(raw,&expanded,packed,idat_n)||expanded!=total)goto done;
 for(unsigned int y=0;y<h;y++){
  unsigned char *p=raw+y*(row+1)+1,*prior=y?p-row-1:0;unsigned int filter=p[-1];if(filter>4)goto done;
  for(unsigned int x=0;x<row;x++){
   int a=x>=channels?p[x-channels]:0,b=prior?prior[x]:0,c=prior&&x>=channels?prior[x-channels]:0,v=0;
   if(filter==1)v=a;else if(filter==2)v=b;else if(filter==3)v=(a+b)/2;
   else if(filter==4){int q=a+b-c,pa=q-a,pb=q-b,pc=q-c;pa=pa<0?-pa:pa;pb=pb<0?-pb:pb;pc=pc<0?-pc:pc;v=pa<=pb&&pa<=pc?a:pb<=pc?b:c;}
   p[x]=(unsigned char)(p[x]+v);
  }
 }
 for(unsigned int y=0;y<h;y++){unsigned char *p=raw+y*(row+1)+1;p[-1]=0;for(unsigned int x=0;x<row;x+=channels)pixel(p+x,source_light,bg,fg);}
 free(packed);packed=0;unsigned long capacity=compressBound(total);packed=malloc(capacity);if(!packed)goto done;
 if(compress2(packed,&capacity,raw,total,1))goto done;
 result=malloc((usize)capacity+57);if(!result)goto done;copy(result,signature,8);
 at=8;at+=chunk(result+at,"IHDR",in+16,13);at+=chunk(result+at,"IDAT",packed,(unsigned int)capacity);at+=chunk(result+at,"IEND",in,0);
 *out=result;*out_n=at;result=0;ok=1;
 done:free(result);free(packed);free(raw);return ok;
}

#define CACHE_COUNT 4096
struct resource {char source[224],path[320];int light,temporary;};
static struct resource *cache;static unsigned int count;static char root[256];static unsigned int palette[2][7];
static unsigned char *load_png(const char *path,unsigned int *n){
 int fd=open(path,0);if(fd<0)return 0;unsigned char *b=malloc(8388609);unsigned int at=0;int got=0;
 if(b)while(at<8388609&&(got=read(fd,b+at,8388609-at))>0)at+=(unsigned int)got;
 close(fd);if(!b||got<0||at>8388608){free(b);return 0;}*n=at;return b;
}
void theme_resources_clear(void){if(cache){for(unsigned int i=0;i<count;i++)if(cache[i].temporary)unlink(cache[i].path);free(cache);}cache=0;count=0;root[0]=0;}
void theme_resources_init(const char *directory,const unsigned int colors[2][7]){
 theme_resources_clear();if(snprintf(root,sizeof(root),"%s",directory)>=(int)sizeof(root)){root[0]=0;return;}
 copy((unsigned char *)palette,(const unsigned char *)colors,sizeof(palette));cache=calloc(CACHE_COUNT,sizeof(*cache));
}
static const char *resource_unlocked(const char *path,int light){
 if(!path||!root[0]||!cache)return path;const char *name=prefix(path,"/res/ui/")?path+8:path;
 unsigned int len=0;while(name[len]){if(len>=223)return path;if(name[len]=='\\')return path;len++;}
 if(len<5||name[0]=='/'||!eq(name+len-4,".png"))return path;
 for(unsigned int i=0;i<len;i++)if(name[i]=='.'&&(i==0||name[i-1]=='/')&&(name[i+1]=='/'||name[i+1]=='.'))return path;
 light=!!light;for(unsigned int i=0;i<count;i++)if(cache[i].light==light&&eq(cache[i].source,name))return cache[i].path[0]?cache[i].path:path;
 if(count==CACHE_COUNT)return path;struct resource *r=&cache[count++];r->light=light;snprintf(r->source,sizeof(r->source),"%s",name);
 const char *tail=prefix(name,"dash/")?name+5:prefix(name,"light/")?name+6:0;
 char candidate[512];snprintf(candidate,sizeof(candidate),"%s/assets/ui/%s%s",root,light?"light/":"dash/",tail?tail:name);
 int fd=open(candidate,0);if(fd<0&&!tail){snprintf(candidate,sizeof(candidate),"%s/assets/ui/%s",root,name);fd=open(candidate,0);}
 if(fd>=0){close(fd);if(snprintf(r->path,sizeof(r->path),"%s",candidate)>=(int)sizeof(r->path))r->path[0]=0;return r->path[0]?r->path:path;}
 snprintf(candidate,sizeof(candidate),"/res/ui/%s",name);unsigned int n=0,out_n=0;unsigned char *b=load_png(candidate,&n),*out=0;
 int ok=b&&theme_png(b,n,prefix(name,"light/"),palette[light][0],palette[light][1],&out,&out_n);free(b);if(!ok)return path;
 snprintf(r->path,sizeof(r->path),"/tmp/dashborne-theme-%d-%u.png",getpid(),count);
 fd=open(r->path,1|0100|01000,0600);unsigned int at=0;if(fd>=0){while(at<out_n){int wrote=write(fd,out+at,out_n-at);if(wrote<=0)break;at+=(unsigned int)wrote;}if(close(fd))at=0;}free(out);
 if(at!=out_n){unlink(r->path);r->path[0]=0;return path;}r->temporary=1;return r->path;
}

static volatile unsigned int resource_lock;
extern int sched_yield(void);
const char *theme_resource(const char *path,int light){
 while(__sync_lock_test_and_set(&resource_lock,1))sched_yield();
 const char *result=resource_unlocked(path,light);
 __sync_lock_release(&resource_lock);return result;
}
