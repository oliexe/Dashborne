#include <stdio.h>
#include <string.h>
#include "../config.h"
int main(int argc,char **argv) {
    char value[1024];
    if(argc<2)return 2;
    if(!strcmp(argv[1],"wifi")) {
        char ssid[33],password[64];int ok=config_wifi(ssid,password);
        if(ok)printf("%s\n%s\n",ssid,password);return ok ? 0 : 1;
    }
    if(!config_load())return 1;
    int ok=1;
    if(!strcmp(argv[1],"get") && argc==3) {ok=config_get(argv[2],value,sizeof(value));if(ok)puts(value);}
    else if(!strcmp(argv[1],"set") && argc==4)ok=config_set(argv[2],argv[3]) && config_commit();
    else if(strcmp(argv[1],"load"))ok=0;
    config_clear();return ok ? 0 : 1;
}
