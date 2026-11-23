/* Variable-length, fingerprinted edits. All untouched bytes come from the device. */
#include "layout_patch.h"
typedef __SIZE_TYPE__ usize;
extern void *malloc(usize);extern void free(void *);
extern int uncompress(unsigned char *, unsigned long *,const unsigned char *,unsigned long);
extern int compress2(unsigned char *,unsigned long *,const unsigned char *,unsigned long,int);
extern unsigned long compressBound(unsigned long),crc32(unsigned long,const unsigned char *,unsigned int);
static unsigned int u32(const unsigned char *p){return p[0]|p[1]<<8|p[2]<<16|(unsigned int)p[3]<<24;}
static void put(unsigned char *p,unsigned int n){for(int i=0;i<4;i++){p[i]=n;n>>=8;}}
static void cp(unsigned char *a,const unsigned char *b,unsigned int n){while(n--)*a++=*b++;}
static int eq(const unsigned char *a,const char *b,unsigned int n){while(n--)if(*a++!=(unsigned char)*b++)return 0;return 1;}
int tesla_layout(const unsigned char *base,unsigned int size,const unsigned char *patch,unsigned int length,unsigned char **out,unsigned int *out_size){
 *out=0;*out_size=0;
 if(size<32||length<28||!eq(base,"ZKSW\xaa\x55",6)||!eq(base+size-6,"\x55\xaaZKSW",6)||!eq(patch,"ATP1",4))return 0;
 unsigned int offset=u32(base+6),plain_n=u32(patch+12),count=u32(patch+20);
 if(offset<18||offset>256||offset+8>size-6||u32(base+10)!=plain_n+8||plain_n>2097152||count>8192||
 u32(base+14)!=size-offset+2||u32(patch+4)!=size||u32(patch+8)!=(unsigned int)crc32(0,base,size)||
 u32(patch+24)!=(unsigned int)crc32(0,patch+28,length-28))return 0;
 unsigned char *plain=malloc(plain_n+1),*changed=0,*packed=0;int ok=0;
 unsigned long n=plain_n;
 if(!plain||uncompress(plain,&n,base+offset+8,size-offset-14)||n!=plain_n||u32(patch+16)!=(unsigned int)crc32(0,plain,plain_n))goto done;
 unsigned int at=28,end=0,total=plain_n;
 for(unsigned int i=0;i<count;i++){
  if(length-at<12)goto done;
  unsigned int pos=u32(patch+at),del=u32(patch+at+4),add=u32(patch+at+8);at+=12;
  if(pos<end||pos>plain_n||del>plain_n-pos||add>length-at||add>2097152||total-del>2097152-add)goto done;
  total=total-del+add;end=pos+del;at+=add;
 }
 if(at!=length)goto done;
 changed=malloc(total+1);if(!changed)goto done;
 at=28;end=0;unsigned int written=0;
 for(unsigned int i=0;i<count;i++){
  unsigned int pos=u32(patch+at),del=u32(patch+at+4),add=u32(patch+at+8);at+=12;
  cp(changed+written,plain+end,pos-end);written+=pos-end;
  cp(changed+written,patch+at,add);written+=add;at+=add;end=pos+del;
 }
 cp(changed+written,plain+end,plain_n-end);
 unsigned long capacity=compressBound(total);packed=malloc(capacity+32);if(!packed)goto done;
 if(compress2(packed+26,&capacity,changed,total,9))goto done;
 cp(packed,(const unsigned char *)"ZKSW\xaa\x55",6);put(packed+6,18);put(packed+10,total+8);put(packed+14,capacity+16);
 for(int i=18;i<26;i++)packed[i]=0;
 cp(packed+26+capacity,(const unsigned char *)"\x55\xaaZKSW",6);
 *out=packed;*out_size=capacity+32;packed=0;ok=1;
 done:free(plain);free(changed);free(packed);return ok;
}
