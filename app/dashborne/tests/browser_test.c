#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../browser.h"
extern int unlink(const char *);
static unsigned int find(const char *name) {
    for(unsigned int i=0;i<browser_count();++i)
        if(!strcmp(browser_entry(i)->name,name))return i;
    assert(!"Missing fixture entry");return 0;
}
int main(int argc,char **argv) {
    assert(argc>=2);
    if(!strcmp(argv[1],"validate")) { assert(argc==3);return browser_validate(argv[2]) ? 0 : 1; }
    browser_init();
    assert(!strcmp(browser_path(),BROWSER_ROOT));
    browser_up();assert(!strcmp(browser_path(),BROWSER_ROOT));
    assert(browser_entry(browser_count())==NULL);
    browser_select(browser_count());assert(!browser_can_load());
    if(!strcmp(argv[1],"limit")) { assert(browser_count()==BROWSER_MAX && strstr(browser_status(),"256"));return 0; }
    assert(browser_entry(0)->directory); /* folders first */
    for(unsigned int i=0;i<browser_count();++i)assert(strcmp(browser_entry(i)->name,"outside"));
    browser_select(find("Apps"));assert(strstr(browser_path(),"/Apps"));
    if(!strcmp(argv[1],"unsupported")) {
        browser_select(find("readme.txt"));assert(!browser_can_load() && !browser_prepare());return 0;
    }
    browser_select(find("A sample.so"));assert(browser_can_load());
    char selected[BROWSER_PATH];snprintf(selected,sizeof(selected),"%s",browser_selection());
    if(!strcmp(argv[1],"removed")) { assert(!unlink(selected));assert(!browser_prepare() && !browser_can_load());return 0; }
    assert(browser_prepare());
    char path[BROWSER_PATH];assert(browser_take_request(path)==1 && !strcmp(path,selected));
    assert(browser_take_request(path)==0); /* consumed before app code runs */
    assert(browser_take_return()==1 && browser_take_return()==0);
    assert(browser_prepare());browser_cancel_request();
    assert(browser_take_request(path)==0 && browser_take_return()==0);
    browser_refresh();assert(!browser_can_load() && !*browser_selection());
    browser_up();assert(!strcmp(browser_path(),BROWSER_ROOT));
    browser_select(find("Empty"));assert(!browser_count() && strstr(browser_status(),"empty"));
    browser_up();assert(!strcmp(browser_path(),BROWSER_ROOT));
    return 0;
}
