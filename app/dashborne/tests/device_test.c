#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define pread64 fake_pread64
#define dlsym fake_dlsym
#include "../device.c"
static unsigned int metadata_reads;
static int changed, invalid_data, unreadable;
void *fake_dlsym(void *handle,const char *name) {(void)handle;(void)name;return (void *)100;}
__PTRDIFF_TYPE__ fake_pread64(int fd,void *out,usize n,long long offset) {
    (void)fd;if(unreadable)return -1;
    if(offset==100) {unsigned int header[2]={200,3};if(metadata_reads++ && changed)header[0]=300;assert(n==8);memcpy(out,header,n);}
    else {assert(offset==200 && n==3);memcpy(out,invalid_data ? "1\n3" : "1.3",3);}return n;
}
static const char *value(const char *name) {
    for(unsigned int i=0;i<device_count();++i)if(!strcmp(device_row(i)->label,name))return device_row(i)->value;
    return "missing";
}
int main(int argc,char **argv) {
    assert(argc==2);
    char out[192],serial[192];unsigned long n;
    unsigned char uid[16]={1,0,0,0,2,0,0,0,3,0,0,0,4,0,0,0};
    identifiers(uid,16,"202601011230",out,serial);
    assert(!strcmp(out,"00000001-00000002-00000003-00000004"));
    assert(!strcmp(serial,"44415348.0000000A.20260101"));
    identifiers(uid,16,"bad",out,serial);assert(*out && !*serial);
    identifiers(uid,15,"20260101",out,serial);assert(!*out && !*serial);
    memset(uid,0,16);identifiers(uid,16,"20260101",out,serial);assert(!*out && !*serial);

    assert(!decimal("no number",&n));assert(!decimal("184467440737095516160",&n));assert(!decimal("-2",&n));
    assert(decimal("123.45",&n) && n==123);
    field("OtherVersion=wrong\nVersionExtra=wrong\nVersion = 2.3\n","Version",'=',out,sizeof(out));assert(!strcmp(out,"2.3"));
    const char *bad[]={"{\"proddate\"","{\"proddate\":","{\"proddate\":\"abc","{\"proddate\":\"a\\nb\"}","{\"nested\":{\"proddate\":\"wrong\"}}","{\"proddate\":123}"};
    for(unsigned int i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {json_string(bad[i],"proddate",out,sizeof(out));assert(!*out);}
    json_string("{\"nested\":{\"proddate\":\"wrong\"},\"proddate\":\"20260101\"}","proddate",out,sizeof(out));assert(!strcmp(out,"20260101"));
    json_string("{\"proddate\":\"0123456789\"}","proddate",out,4);assert(!*out);
    component_string(1,"version",out);assert(!strcmp(out,"1.3"));
    metadata_reads=0;changed=1;component_string(1,"version",out);assert(!*out);
    changed=0;invalid_data=1;component_string(1,"version",out);assert(!*out);
    invalid_data=0;unreadable=1;component_string(1,"version",out);assert(!*out);
    device_refresh();assert(device_count()>20 && !device_row(device_count()));
    if(!strcmp(argv[1],"empty")) {
        assert(!strcmp(value("System firmware"),"Unavailable"));
        assert(!strcmp(value("Hardware ID"),"Unavailable"));
        assert(!strcmp(value("Usable RAM"),"Unavailable"));
    } else if(!strcmp(argv[1],"populated")) {
        assert(!strcmp(value("System firmware"),"test-2.3"));
        assert(!strcmp(value("Device name"),"Test DASH"));
        assert(!strcmp(value("Production stamp"),"202601011230"));
        assert(!strcmp(value("Usable RAM"),"256.0 MiB"));
        assert(!strcmp(value("Available RAM"),"128.5 MiB"));
        assert(!strcmp(value("Uptime"),"2 days, 1 h 2 min"));
        assert(!strcmp(value("microSD serial"),"0x11223344"));
        assert(!strcmp(value("microSD capacity"),"512.0 GiB"));
        assert(!strcmp(value("Official dashboard"),"Unrecognized build"));
    } else if(!strcmp(argv[1],"malformed")) {
        assert(!strcmp(value("Production stamp"),"Unavailable"));
        assert(!strcmp(value("Usable RAM"),"Unavailable"));
        assert(!strcmp(value("Uptime"),"Unavailable"));
    } else assert(0);
    /* A refresh replaces stale fields, rather than accumulating rows. */
    unsigned int before=device_count();device_refresh();assert(device_count()==before);
    return 0;
}
