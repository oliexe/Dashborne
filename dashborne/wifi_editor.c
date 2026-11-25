#include "wifi_editor.h"
#include "config.h"
static unsigned int length(const char *s) {unsigned int n=0;while(s[n])++n;return n;}
static void wipe(void *p,unsigned int n) {volatile unsigned char *b=p;while(n--)*b++=0;}
void wifi_editor_close(struct wifi_editor *e) {wipe(e,sizeof(*e));}
void wifi_saved_name(char out[33]) {
    out[0]=0;
    if(config_load())(void)config_get("wifi.ssid",out,33);
    config_clear();
}
int wifi_editor_open(struct wifi_editor *e) {
    wifi_editor_close(e);
    int ok=config_load();
    if(ok) {
        (void)config_get("wifi.ssid",e->ssid,sizeof(e->ssid));
        (void)config_get("wifi.password",e->password,sizeof(e->password));
        e->active=1;
    }
    config_clear();return ok;
}
char wifi_editor_key(const struct wifi_editor *e,unsigned int index) {
    static const char letters[4][11]={"1234567890","qwertyuiop","asdfghjkl","zxcvbnm"};
    static const char symbols[4][11]={"!@#$%^&*()","-_=+[]{}\\|",";:\"',.<>/?","`~"};
    if(index>=40)return 0;
    char c=e->symbols ? symbols[index/10][index%10] : letters[index/10][index%10];
    if(e->shift && c>='a' && c<='z')c-='a'-'A';
    return c;
}
void wifi_editor_type(struct wifi_editor *e,char c) {
    if(!e->active || c<32 || c>126)return;
    char *s=e->field ? e->password : e->ssid;
    unsigned int n=length(s),limit=e->field ? 63 : 32;
    if(n==limit){e->error=e->field ? "Password limit: 63 bytes" : "SSID limit: 32 bytes";return;}
    s[n]=c;s[n+1]=0;e->error=0;
}
void wifi_editor_backspace(struct wifi_editor *e) {
    char *s=e->field ? e->password : e->ssid;unsigned int n=length(s);
    if(n) {unsigned int start=n-1;while(start && ((unsigned char)s[start]&0xc0)==0x80)--start;wipe(s+start,n-start);}
    e->error=0;
}
void wifi_editor_clear_field(struct wifi_editor *e) {
    if(e->field)wipe(e->password,sizeof(e->password));else wipe(e->ssid,sizeof(e->ssid));
    e->error=0;
}
void wifi_editor_display(const struct wifi_editor *e,int field,char out[65]) {
    const char *s=field ? e->password : e->ssid;unsigned int n=length(s),start=0;
    /* Keep the insertion end visible on the narrow input fields. */
    if(n>24){start=n-24;while(((unsigned char)s[start]&0xc0)==0x80)++start;}
    unsigned int at=0;if(start){out[at++]='.';out[at++]='.';out[at++]='.';}
    for(unsigned int i=start;i<n;++i)out[at++]=field && !e->reveal ? '*' : s[i];
    if(e->field==field)out[at++]='|';out[at]=0;
}
int wifi_editor_save(struct wifi_editor *e) {
    if(!e->active)return 0;
    unsigned int a=length(e->ssid),b=length(e->password);
    if(!a || a>32){e->field=0;e->error="Enter an SSID (1-32 bytes)";return 0;}
    if(b<8 || b>63){e->field=1;e->error="Password must be 8-63 bytes";return 0;}
    int ok=config_load() && config_set("wifi.ssid",e->ssid) &&
        config_set("wifi.password",e->password) && config_commit();
    config_clear();
    if(!ok)e->error="Could not save dashborne.conf";
    return ok;
}
