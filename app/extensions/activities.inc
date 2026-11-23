/* Resident factory trampolines: FlyThings has no unregisterActivity API. */
#define MAX_ACTIVITIES 32
struct activity_entry {
    char name[96], owner_id[32];
    const struct dashborne_host *owner;
    void *(*create)(void);
};
static struct activity_entry activities[MAX_ACTIVITIES];
static unsigned int activity_count;
static void *activity_0(void) {return activities[0].create?activities[0].create():0;}
static void *activity_1(void) {return activities[1].create?activities[1].create():0;}
static void *activity_2(void) {return activities[2].create?activities[2].create():0;}
static void *activity_3(void) {return activities[3].create?activities[3].create():0;}
static void *activity_4(void) {return activities[4].create?activities[4].create():0;}
static void *activity_5(void) {return activities[5].create?activities[5].create():0;}
static void *activity_6(void) {return activities[6].create?activities[6].create():0;}
static void *activity_7(void) {return activities[7].create?activities[7].create():0;}
static void *activity_8(void) {return activities[8].create?activities[8].create():0;}
static void *activity_9(void) {return activities[9].create?activities[9].create():0;}
static void *activity_10(void) {return activities[10].create?activities[10].create():0;}
static void *activity_11(void) {return activities[11].create?activities[11].create():0;}
static void *activity_12(void) {return activities[12].create?activities[12].create():0;}
static void *activity_13(void) {return activities[13].create?activities[13].create():0;}
static void *activity_14(void) {return activities[14].create?activities[14].create():0;}
static void *activity_15(void) {return activities[15].create?activities[15].create():0;}
static void *activity_16(void) {return activities[16].create?activities[16].create():0;}
static void *activity_17(void) {return activities[17].create?activities[17].create():0;}
static void *activity_18(void) {return activities[18].create?activities[18].create():0;}
static void *activity_19(void) {return activities[19].create?activities[19].create():0;}
static void *activity_20(void) {return activities[20].create?activities[20].create():0;}
static void *activity_21(void) {return activities[21].create?activities[21].create():0;}
static void *activity_22(void) {return activities[22].create?activities[22].create():0;}
static void *activity_23(void) {return activities[23].create?activities[23].create():0;}
static void *activity_24(void) {return activities[24].create?activities[24].create():0;}
static void *activity_25(void) {return activities[25].create?activities[25].create():0;}
static void *activity_26(void) {return activities[26].create?activities[26].create():0;}
static void *activity_27(void) {return activities[27].create?activities[27].create():0;}
static void *activity_28(void) {return activities[28].create?activities[28].create():0;}
static void *activity_29(void) {return activities[29].create?activities[29].create():0;}
static void *activity_30(void) {return activities[30].create?activities[30].create():0;}
static void *activity_31(void) {return activities[31].create?activities[31].create():0;}

static void *(*const factories[MAX_ACTIVITIES])(void)={
activity_0,activity_1,activity_2,activity_3,activity_4,activity_5,activity_6,activity_7,activity_8,activity_9,activity_10,activity_11,activity_12,activity_13,activity_14,activity_15,activity_16,activity_17,activity_18,activity_19,activity_20,activity_21,activity_22,activity_23,activity_24,activity_25,activity_26,activity_27,activity_28,activity_29,activity_30,activity_31};
static struct loaded_extension *owner_of(const struct dashborne_host *host) {
    for(unsigned int i=0;i<loaded_count;++i)
        if(host==&loaded[i].host)return &loaded[i];
    return 0;
}
static int owns_callback(const struct dashborne_host *host,const void *callback) {
    struct image_info candidate,owner;
    if(!owner_of(host) || !callback || !dladdr(callback,&candidate))return 0;
    for(unsigned int i=0;i<loaded_count;++i)
        if(loaded[i].descriptor && loaded[i].entered &&
           dladdr((const void *)loaded[i].descriptor->activate,&owner) &&
           owner.base==candidate.base)return 1;
    return 0;
}
static int register_plugin_activity(const struct dashborne_host *host,const char *name,void *(*create)(void)) {
    struct loaded_extension *entry=owner_of(host);
    if(!entry || !activating || poisoned || !create || !name ||
       !prefix(name,"dashborneExtension_") || length(name)>=96)return -1;
    for(const char *p=name;*p;++p)
        if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'))return -1;
    if(!owns_callback(host,(const void *)create))return -1;
    for(unsigned int i=0;i<activity_count;++i)if(equal(name,activities[i].name)) {
        if(!equal(entry->descriptor->id,activities[i].owner_id) || activities[i].create)return -1;
        activities[i].owner=host;activities[i].create=create;return 0;
    }
    if(activity_count==MAX_ACTIVITIES)return -1;
    void *(*instance)(void)=0;
    int (*register_native)(void *,const char *,void *(*)(void))=0;
    *(void **)(&instance)=dlsym(host->ui_runtime,"_ZN15ActivityFactory11getInstanceEv");
    *(void **)(&register_native)=dlsym(host->ui_runtime,"_ZN15ActivityFactory16registerActivityEPKcPFP8ActivityvE");
    if(!instance || !register_native)return -1;
    unsigned int i=activity_count;
    copy(activities[i].name,name,sizeof(activities[i].name));
    copy(activities[i].owner_id,entry->descriptor->id,sizeof(activities[i].owner_id));
    activities[i].owner=host;activities[i].create=create;
    if(!register_native(instance(),activities[i].name,factories[i])) {
        activities[i]=(struct activity_entry){0};return -1;
    }
    ++activity_count;return 0;
}
static void retire_activities(const struct dashborne_host *host) {
    for(unsigned int i=0;i<activity_count;++i)if(activities[i].owner==host) {
        activities[i].create=0;activities[i].owner=0;
    }
}
