#include <assert.h>
#include <string.h>
#include "../wifi_editor.h"
int main(int argc,char **argv) {
    struct wifi_editor e;char text[65];assert(argc>=2);
    assert(wifi_editor_open(&e));
    if(!strcmp(argv[1],"keyboard")) {
        unsigned char seen[128]={0};
        for(int symbols=0;symbols<2;++symbols)for(int shift=0;shift<2;++shift) {
            e.symbols=symbols;e.shift=shift;
            for(unsigned int i=0;i<40;++i)seen[(unsigned char)wifi_editor_key(&e,i)]=1;
        }
        for(int c=33;c<=126;++c)assert(seen[c]);
        assert(!wifi_editor_key(&e,40));
        wifi_editor_clear_field(&e);
        for(int i=0;i<33;++i)wifi_editor_type(&e,'s');
        assert(strlen(e.ssid)==32 && e.error);
        wifi_editor_backspace(&e);assert(strlen(e.ssid)==31 && !e.error);
        e.field=1;wifi_editor_clear_field(&e);
        for(int i=0;i<64;++i)wifi_editor_type(&e,'p');
        assert(strlen(e.password)==63 && e.error);
        wifi_editor_display(&e,1,text);assert(!strchr(text,'p') && strchr(text,'|'));
        e.reveal=1;wifi_editor_display(&e,1,text);assert(strchr(text,'p'));
        strcpy(e.ssid,"caf\xc3\xa9");e.field=0;wifi_editor_backspace(&e);assert(!strcmp(e.ssid,"caf"));
        wifi_editor_type(&e,'\n');assert(!strcmp(e.ssid,"caf"));
        wifi_editor_close(&e);
        for(unsigned int i=0;i<sizeof(e);++i)assert(!((unsigned char *)&e)[i]);
        return 0;
    }
    assert(argc==4);
    e.field=0;wifi_editor_clear_field(&e);for(const char *p=argv[2];*p;++p)wifi_editor_type(&e,*p);
    e.field=1;wifi_editor_clear_field(&e);for(const char *p=argv[3];*p;++p)wifi_editor_type(&e,*p);
    int ok=wifi_editor_save(&e);assert(ok || e.error);
    wifi_editor_close(&e);return ok ? 0 : 1;
}
