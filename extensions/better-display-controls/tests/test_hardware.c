#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define open fake_open
#define close fake_close
#define read fake_read
#define ioctl fake_ioctl
#define crc32 fake_crc32
#define dlopen fake_dlopen
#define dlclose fake_dlclose
#include "../hardware.c"
#undef open
#undef close
#undef read
#undef ioctl
#undef crc32
#undef dlopen
#undef dlclose
static int level=32,luma=50,fail_op,fail_count,writes;
static union {float f[8];int i[8];} native;
static int get_screen(const void *p){assert(p==&native);return native.i[4];}
static int get_brightness(const void *p){assert(p==&native);return native.i[2];}
int fake_open(const char *p,int mode,...){(void)p;(void)mode;return 3;}
int fake_read(int f,void *p,unsigned int n){(void)f;(void)p;(void)n;return -1;}
int fake_close(int f){assert(f==3);return 0;}
unsigned long fake_crc32(unsigned long a,const unsigned char *p,unsigned int n){(void)a;(void)p;(void)n;return 0;}
void *fake_dlopen(const char *p,int mode){(void)p;(void)mode;return 0;}
int fake_dlclose(void *p){(void)p;return 0;}
int fake_ioctl(int f,unsigned long op,...){
 assert(f==3);va_list ap;va_start(ap,op);unsigned long *a=va_arg(ap,unsigned long *);va_end(ap);
 if((int)op==fail_op&&fail_count){--fail_count;return -1;}
 switch(op){case 0x103:return level;case 0x191:return luma;case 0x102:level=(int)a[1];writes++;return 0;
 case 0x190:luma=(int)a[1];writes++;return 0;default:assert(0);return -1;}
}
int main(void){
 helper=&native;stock_brightness=get_brightness;screen_on=get_screen;native.i[0]=32;native.i[1]=100;native.i[2]=0;native.f[3]=2.23f;
 assert(!display_hardware_restore()&&!writes);
 assert(display_hardware_apply(0,50)==-1&&!writes);
 for(int low=0;low<6;low++)assert(display_hardware_apply(low,50)==-1&&!writes);
 native.i[4]=1;
 assert(!display_hardware_apply(6,40)&&level==6&&luma==40&&native.i[0]==6&&native.f[3]==0);
 int n=writes;assert(!display_hardware_apply(6,40)&&writes==n);
 /* Subsequent stock requests still map to our absolute level, sleep stays off. */
 native.i[2]=75;assert(native.i[0]+(int)(native.i[2]*native.f[3])==6);
 native.i[4]=0;level=78; /* LCD disable retains a nonzero brightness cache. */
 n=writes;assert(!display_hardware_apply(255,50)&&level==78&&writes==n);
 assert(!display_hardware_restore()&&level==78&&native.i[0]==32&&native.f[3]>2.2f);
 native.i[4]=1;native.i[2]=20;level=76;
 assert(!display_hardware_apply(6,49));assert(!display_hardware_restore()&&level==76&&luma==50);
 fail_op=0x190;fail_count=1;assert(display_hardware_apply(6,30)==-1&&level==76&&luma==50&&!touched);
 fail_count=2;assert(display_hardware_apply(6,30)==-1&&touched);assert(!display_hardware_restore()&&!touched);
 puts("display hardware: full lit range, sleep, stock requests, restore, rollback and retry passed");
}
