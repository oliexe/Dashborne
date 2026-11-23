#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>
#include "../layout_patch.h"
static void put(unsigned char *p,uint32_t n){for(int i=0;i<4;i++){p[i]=n;n>>=8;}}
int main(void){
 const char *raw="{\"fontSize\":28}";unsigned char b[200]={0},p[42]={0};unsigned long n=150;
 memcpy(b,"ZKSW\xaa\x55",6);put(b+6,18);put(b+10,strlen(raw)+8);
 if(compress2(b+26,&n,(const unsigned char *)raw,strlen(raw),9))return 1;
 put(b+14,n+16);memcpy(b+26+n,"\x55\xaaZKSW",6);unsigned int size=n+32;
 memcpy(p,"ATP1",4);put(p+4,size);put(p+8,crc32(0,b,size));put(p+12,strlen(raw));put(p+16,crc32(0,(const unsigned char *)raw,strlen(raw)));put(p+20,1);
 put(p+28,12);put(p+32,2);put(p+36,2);memcpy(p+40,"24",2);put(p+24,crc32(0,p+28,14));
 unsigned char *out=0;unsigned int out_n=0;
 if(!tesla_layout(b,size,p,42,&out,&out_n))return 2;free(out);
 for(unsigned int i=0;i<42;i++){if(tesla_layout(b,size,p,i,&out,&out_n))return 3;}
 uint32_t rand=932041;
 for(unsigned int i=0;i<10000;i++){
  unsigned char mutant[42];memcpy(mutant,p,42);rand=rand*1664525u+1013904223u;
  mutant[(rand>>16)%42]^=(rand&255)|1;
  if(i&1)put(mutant+24,crc32(0,mutant+28,14));
  tesla_layout(b,size,mutant,42,&out,&out_n);free(out);
 }
 puts("Layout bounds and 10000 mutation cases passed");return 0;
}
