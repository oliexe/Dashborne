#ifndef DASHBORNE_STOCK_COMPAT_H
#define DASHBORNE_STOCK_COMPAT_H
/* Verified app builds. Runtime ABI checks remain at each integration point. */
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int);
extern unsigned long crc32(unsigned long,const unsigned char *,unsigned int);
static int db_stock_cached_version;
static inline int db_stock_version(void) {
    if(db_stock_cached_version)return db_stock_cached_version;
    int fd=open("/res/lib/libzkgui.so",0);if(fd<0)return 0;
    unsigned char buffer[4096];unsigned int size=0;unsigned long crc=0;int n;
    while((n=read(fd,buffer,sizeof(buffer)))>0) {
        if((unsigned int)n>3464120u-size){close(fd);return 0;}
        size+=(unsigned int)n;crc=crc32(crc,buffer,(unsigned int)n);
    }
    close(fd);if(n || size!=3464120u)return 0;
    if(crc==0x9de99542u)db_stock_cached_version=6112;
    if(crc==0xed0e1aa4u)db_stock_cached_version=6114;
    return db_stock_cached_version;
}
#define DB_STOCK_OFFSET(old,current) (db_stock_version()==6114?(current):(old))
#endif
