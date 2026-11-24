#include "pack.h"
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int);
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
int tm_image_valid(const char *path) {
    unsigned char header[33];int fd=open(path,0);if(fd<0)return 0;
    unsigned int used=0;int n;
    while(used<sizeof(header) && (n=read(fd,header+used,sizeof(header)-used))>0)used+=(unsigned int)n;
    close(fd);if(used!=sizeof(header))return 0;
    const unsigned char signature[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82};
    for(unsigned int i=0;i<sizeof(signature);++i)if(header[i]!=signature[i])return 0;
    unsigned int w=(unsigned int)header[16]<<24|(unsigned int)header[17]<<16|(unsigned int)header[18]<<8|header[19];
    unsigned int h=(unsigned int)header[20]<<24|(unsigned int)header[21]<<16|(unsigned int)header[22]<<8|header[23];
    return w && h && w<=512 && h<=480;
}
int tm_pack_parse(struct tm_pack *p,const char *text,unsigned int size) {
    *p=(struct tm_pack){0};unsigned int i=0;
    const char magic[]="tesla-model-v1\n";
    for(;i<sizeof(magic)-1;++i)if(i>=size || text[i]!=magic[i])return -1;
    while(i<size) {
        if(p->count==TM_VARIANTS)return -1;
        int len=0;char *id=p->ids[p->count];
        while(i<size && text[i]!='\n') {
            char c=text[i++];
            if(len==47 || !((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return -1;
            id[len++]=c;
        }
        if(!len || id[0]=='-' || id[len-1]=='-')return -1;
        id[len]=0;
        for(int v=0;v<p->count;++v){int k=0;while(id[k] && id[k]==p->ids[v][k])++k;if(!id[k]&&!p->ids[v][k])return -1;}
        ++p->count;if(i<size)++i;
    }
    return p->count?0:-1;
}
int tm_pack_load(struct tm_pack *p,const char *directory) {
    char path[512],text[1024];
    int len=snprintf(path,sizeof(path),"%s/models/index",directory);
    if(len<0 || (unsigned int)len>=sizeof(path))return -1;
    int fd=open(path,0);if(fd<0)return -1;
    unsigned int used=0;int n=0;
    while(used<sizeof(text) && (n=read(fd,text+used,sizeof(text)-used))>0)used+=(unsigned int)n;
    close(fd);if(n<0 || used==sizeof(text))return -1;
    return tm_pack_parse(p,text,used);
}
