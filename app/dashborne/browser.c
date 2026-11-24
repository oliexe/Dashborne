/* SD browser and one-use application handoff. No persistent boot edits. */
#include "browser.h"
typedef __SIZE_TYPE__ usize;
#ifdef __arm__
/* glibc's Linux dirent64 ABI, independent of the 32-bit off_t setting. */
struct dirent64 { unsigned long long ino; long long off; unsigned short reclen; unsigned char type; char name[256]; };
_Static_assert(__builtin_offsetof(struct dirent64, name) == 19, "Linux dirent64 name offset");
extern void *opendir(const char *);
extern struct dirent64 *readdir64(void *);
extern int closedir(void *);
#define ENTRY_NAME(e) ((e)->name)
#define READDIR(d) readdir64(d)
#else
#include <dirent.h>
#define ENTRY_NAME(e) ((e)->d_name)
#define READDIR(d) readdir(d)
#endif
extern void *malloc(usize);
extern void free(void *);
extern usize strlen(const char *);
extern int strcmp(const char *, const char *);
extern int strncmp(const char *, const char *, usize);
extern void *memcpy(void *, const void *, usize);
extern int snprintf(char *, usize, const char *, ...);
extern void qsort(void *, usize, usize, int (*)(const void *, const void *));
extern char *realpath(const char *, char *);
extern int open(const char *, int, ...), read(int, void *, unsigned int), write(int, const void *, unsigned int), close(int);
extern long lseek(int, long, int);
extern int fsync(int), rename(const char *, const char *), unlink(const char *);
#ifndef BROWSER_ROOT
#define BROWSER_ROOT "/mnt/extsd"
#endif
#ifndef BROWSER_REQUEST
#define BROWSER_REQUEST "/tmp/dashborne-app.once"
#endif
#ifndef BROWSER_RETURN
#define BROWSER_RETURN "/tmp/dashborne-app-return.once"
#endif
#define SELF BROWSER_ROOT "/Dashborne/lib/libdashborne.so"
#define FILE_LIMIT (32u * 1024u * 1024u)
static struct browser_entry entries[BROWSER_MAX];
static unsigned int count;
static char directory[BROWSER_PATH] = BROWSER_ROOT, selected[BROWSER_PATH];
static const char *message = "Select an app";
static int loadable;
static int canonical(const char *path, char out[BROWSER_PATH]) {
    char *p = realpath(path, (void *)0);
    usize root = sizeof(BROWSER_ROOT)-1;
    int ok = p && strlen(p) < BROWSER_PATH && !strncmp(p, BROWSER_ROOT, root) && (!p[root] || p[root] == '/');
    if (ok) memcpy(out, p, strlen(p)+1);
    free(p); return ok;
}
static int join(char out[BROWSER_PATH], const char *name) {
    int n = snprintf(out, BROWSER_PATH, "%s/%s", directory, name);
    return n > 0 && n < BROWSER_PATH;
}
static unsigned int u16(const unsigned char *p) { return p[0] | (unsigned int)p[1]<<8; }
static unsigned int u32(const unsigned char *p) { return u16(p) | u16(p+2)<<16; }
static int span(unsigned int at, unsigned int n, unsigned int size) { return at <= size && n <= size-at; }
static int symbol(const unsigned char *p, unsigned int size, const char *wanted) {
    unsigned int n = (unsigned int)strlen(wanted);
    if (size <= n) return 0;
    for (unsigned int i=0; i<n; ++i) if (p[i] != (unsigned char)wanted[i]) return 0;
    return !p[n];
}
int browser_validate(const char *path) {
    char real[BROWSER_PATH];
    if (!canonical(path, real) || !strcmp(real, SELF)) return 0;
    usize len = strlen(real);
    if (len < 3 || strcmp(real+len-3, ".so")) return 0;
    int fd = open(real, 0); if (fd < 0) return 0;
    long end = lseek(fd, 0, 2);
    if (end < 52 || (unsigned long)end > FILE_LIMIT || lseek(fd, 0, 0) < 0) { close(fd); return 0; }
    unsigned int size = (unsigned int)end, used=0;
    unsigned char *b = malloc(size); if (!b) { close(fd); return 0; }
    while (used < size) { int n=read(fd,b+used,size-used); if(n<=0) break; used+=(unsigned int)n; }
    close(fd); int ok=0;
    /* ELF32, little endian, ARM ET_DYN; hard-float ABI. */
    if (used != size || b[0]!=127 || b[1]!='E' || b[2]!='L' || b[3]!='F' || b[4]!=1 || b[5]!=1 ||
        b[6]!=1 || u16(b+16)!=3 || u16(b+18)!=40 || !(u32(b+36)&0x400u) || u16(b+46)!=40) goto done;
    unsigned int sh=u32(b+32), ns=u16(b+48), found=0;
    if (!ns || !span(sh,ns*40u,size)) goto done;
    for(unsigned int i=0;i<ns;++i) {
        const unsigned char *s=b+sh+i*40u;
        if(u32(s+4)!=11) continue; /* SHT_DYNSYM */
        unsigned int off=u32(s+16), bytes=u32(s+20), link=u32(s+24);
        if(u32(s+36)!=16 || bytes%16 || link>=ns || !span(off,bytes,size)) goto done;
        const unsigned char *strings=b+sh+link*40u;
        unsigned int so=u32(strings+16), sz=u32(strings+20);
        if(u32(strings+4)!=3 || !span(so,sz,size)) goto done;
        for(unsigned int at=0;at<bytes;at+=16) {
            const unsigned char *sym=b+off+at; unsigned int name=u32(sym), bind=sym[12]>>4, visibility=sym[13]&3;
            if(name>=sz || !u16(sym+14) || (sym[12]&15)!=2 || (bind!=1 && bind!=2) || (visibility!=0 && visibility!=3)) continue;
            const unsigned char *text=b+so+name;
            if(symbol(text,sz-name,"onEasyUIInit")) found|=1;
            if(symbol(text,sz-name,"onStartupApp")) found|=2;
            if(symbol(text,sz-name,"onEasyUIDeinit")) found|=4;
        }
    }
    ok=found==7;
done: free(b); return ok;
}
static int compare(const void *a, const void *b) {
    const struct browser_entry *x=a,*y=b;
    return x->directory != y->directory ? y->directory-x->directory : strcmp(x->name,y->name);
}
void browser_refresh(void) {
    count=0; selected[0]=0; loadable=0; message="Select an app";
    char real[BROWSER_PATH];
    if(!canonical(directory,real)) { message="microSD folder unavailable"; return; }
    void *d=opendir(real); if(!d) { message="Cannot open folder"; return; }
    int truncated=0, failed=0;
    for (;;) {
#ifdef __arm__
        struct dirent64 *e=READDIR(d);
#else
        struct dirent *e=READDIR(d);
#endif
        if(!e) break;
        const char *name=ENTRY_NAME(e);
        if(!strcmp(name,".") || !strcmp(name,"..")) continue;
        usize n=strlen(name);
        if(!n || n>255) continue;
        int printable=1; for(usize i=0;i<n;++i) if((unsigned char)name[i]<32) printable=0;
        if(!printable) continue;
        if(count==BROWSER_MAX) { truncated=1; break; }
        char path[BROWSER_PATH], resolved[BROWSER_PATH];
        if(!join(path,name) || !canonical(path,resolved)) { failed=1; continue; }
        void *child=opendir(resolved);
        entries[count].directory=child!=0;
        if(child) closedir(child);
        memcpy(entries[count].name,name,n+1); ++count;
    }
    closedir(d); qsort(entries,count,sizeof(entries[0]),compare);
    if(truncated) message="Folder limit: 256 entries. Use smaller folders.";
    else if(!count) message=failed ? "No accessible entries" : "Folder is empty";
}
void browser_init(void) { memcpy(directory,BROWSER_ROOT,sizeof(BROWSER_ROOT)); browser_refresh(); }
void browser_up(void) {
    if(!strcmp(directory,BROWSER_ROOT)) return;
    usize n=strlen(directory); while(n && directory[n-1]!='/') --n;
    if(n <= sizeof(BROWSER_ROOT)) memcpy(directory,BROWSER_ROOT,sizeof(BROWSER_ROOT));
    else directory[n-1]=0;
    browser_refresh();
}
void browser_select(unsigned int index) {
    if(index>=count) return;
    char path[BROWSER_PATH], real[BROWSER_PATH];
    if(!join(path,entries[index].name) || !canonical(path,real)) {
        selected[0]=0;loadable=0;message="Entry is no longer available";return;
    }
    if(entries[index].directory) {
        memcpy(directory,real,strlen(real)+1); browser_refresh(); return;
    }
    memcpy(selected,real,strlen(real)+1);
    loadable=browser_validate(selected);
    message=loadable ? "Ready to load selected app" : "Select an ARM DASH app (.so with startup interface)";
}
static int save(const char *path,const char *value) {
    char pending[BROWSER_PATH]; int n=snprintf(pending,sizeof(pending),"%s.next",path);
    if(n<0 || n>=(int)sizeof(pending)) return 0;
    int fd=open(pending,1|0100|01000,0600); if(fd<0) return 0;
    unsigned int len=(unsigned int)strlen(value),at=0;
    while(at<len) { int used=write(fd,value+at,len-at);if(used<=0)break;at+=(unsigned int)used; }
    int ok=at==len && fsync(fd)==0; if(close(fd))ok=0;
    if(ok && rename(pending,path)==0)return 1;
    unlink(pending);return 0;
}
int browser_prepare(void) {
    if(!loadable || !browser_validate(selected)) { loadable=0;message="Selected app changed or is unavailable";return 0; }
    if(!save(BROWSER_RETURN,"return") || !save(BROWSER_REQUEST,selected)) {
        browser_cancel_request();message="Could not prepare app launch";return 0;
    }
    message="Loading app...";return 1;
}
int browser_prepare_path(const char *path) {
    char real[BROWSER_PATH];
    if(!canonical(path,real) || !browser_validate(real)) {
        message="Autolaunch app is missing or unsupported";return 0;
    }
    memcpy(selected,real,strlen(real)+1);loadable=1;return browser_prepare();
}
int browser_take_request(char path[BROWSER_PATH]) {
    int fd=open(BROWSER_REQUEST,0); if(fd<0)return 0;
    unlink(BROWSER_REQUEST);
    unsigned int used=0;int n=0;
    while(used<BROWSER_PATH && (n=read(fd,path+used,BROWSER_PATH-used))>0) used+=(unsigned int)n;
    close(fd);
    if(n<0 || !used || used>=BROWSER_PATH)return -1;
    path[used]=0;
    if(strlen(path)!=used || !browser_validate(path))return -1;
    return 1;
}
int browser_take_return(void) { return unlink(BROWSER_RETURN)==0; }
void browser_cancel_request(void) { unlink(BROWSER_REQUEST);unlink(BROWSER_RETURN); }
unsigned int browser_count(void) { return count; }
const struct browser_entry *browser_entry(unsigned int i) { return i<count ? &entries[i] : (void *)0; }
const char *browser_path(void) { return directory; }
const char *browser_selection(void) { return selected; }
const char *browser_status(void) { return message; }
int browser_can_load(void) { return loadable; }
