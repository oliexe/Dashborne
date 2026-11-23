#include "dashborne_extension.h"
extern int open(const char *, int, ...), close(int);
extern int *__errno_location(void);
static int activate(const struct dashborne_host *host) {
    if (host->overlay_file(host,"same.ftu","/res/ui/screensaver.ftu")) return -1;
    int fd=open("/res/ui/screensaver.ftu",1);
    if(fd>=0){close(fd);return -1;}
    if(*__errno_location()!=30)return -1;
    host->log(host,"overlay-probe confirmed read-only resource mount");return 0;
}
static int deactivate(void) { return 0; }
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension d={1,sizeof(d),"overlay-probe",activate,deactivate};return &d;
}
