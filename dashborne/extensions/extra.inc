#define MAX_EXTRA_ENTRIES 64
struct extra_entry {
    const struct dashborne_host *owner;
    char id[32];
    struct dashborne_extra_entry info;
    void (*open)(void);
};
static struct extra_entry extra_entries[MAX_EXTRA_ENTRIES];
static unsigned int extra_token;
static int extra_label(const char *text,unsigned int limit,int empty) {
    if(!text || (!empty && !*text) || length(text)>=limit)return 0;
    for(;*text;++text)if((unsigned char)*text<32 || *text==127)return 0;
    return 1;
}
static int register_extra(const struct dashborne_host *host,const char *id,const char *title,
                          const char *description,void (*open_entry)(void)) {
    struct loaded_extension *owner=owner_of(host);
    struct image_info callback,image;
    if(!owner || !activating || poisoned || !valid_id(id) || !open_entry ||
       !extra_label(title,64,0) || !extra_label(description,128,1) ||
       !dladdr((void *)open_entry,&callback) ||
       !dladdr((void *)owner->descriptor->activate,&image) || callback.base!=image.base)return -1;
    unsigned int free_slot=MAX_EXTRA_ENTRIES;
    for(unsigned int i=0;i<MAX_EXTRA_ENTRIES;++i) {
        if(extra_entries[i].owner==host && equal(extra_entries[i].id,id))return -1;
        if(!extra_entries[i].owner && free_slot==MAX_EXTRA_ENTRIES)free_slot=i;
    }
    if(free_slot==MAX_EXTRA_ENTRIES || extra_token==~0u)return -1;
    struct extra_entry *e=&extra_entries[free_slot];
    *e=(struct extra_entry){.owner=host,.open=open_entry};
    copy(e->id,id,sizeof(e->id));copy(e->info.title,title,sizeof(e->info.title));
    copy(e->info.description,description,sizeof(e->info.description));
    e->info.token=++extra_token;
    return 0;
}
static unsigned int extra_count(const struct dashborne_host *host) {
    if(!owner_of(host) || poisoned)return 0;
    unsigned int n=0;
    for(unsigned int i=0;i<MAX_EXTRA_ENTRIES;++i)n+=extra_entries[i].owner!=0;
    return n;
}
static int extra_entry_at(const struct dashborne_host *host,unsigned int index,struct dashborne_extra_entry *out) {
    if(!owner_of(host) || poisoned || !out)return -1;
    for(unsigned int i=0;i<MAX_EXTRA_ENTRIES;++i)if(extra_entries[i].owner) {
        if(!index--) {*out=extra_entries[i].info;return 0;}
    }
    return -1;
}
static int open_extra(const struct dashborne_host *host,unsigned int token) {
    if(!owner_of(host) || poisoned || activating || !token)return -1;
    for(unsigned int i=0;i<MAX_EXTRA_ENTRIES;++i) {
        struct extra_entry *e=&extra_entries[i];
        if(e->owner && e->info.token==token && e->open) {e->open();return 0;}
    }
    return -1;
}
static void retire_extra(const struct dashborne_host *host) {
    for(unsigned int i=0;i<MAX_EXTRA_ENTRIES;++i)
        if(extra_entries[i].owner==host)extra_entries[i]=(struct extra_entry){0};
}
