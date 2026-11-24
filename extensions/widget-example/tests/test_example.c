#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../extension.c"
static char value[32],status[64];
static void set_text(void *p,const char *s){if(p==(void *)1)snprintf(value,sizeof(value),"%s",s);if(p==(void *)2)snprintf(status,sizeof(status),"%s",s);}
int main(void) {
    struct dashborne_widgets sdk={.text=set_text};widgets=&sdk;
    struct dashborne_widget_view a={.instance=0,.width=250,.height=224,.visible=1,.value=(void *)1,.status=(void *)2},b=a;b.instance=1;
    assert(!create(&a) && !create(&b) && a.user!=b.user);
    update(&a,1);assert(!strcmp(value,"0") && strstr(status,"250 × 224"));
    click(&a);update(&a,2);assert(!strcmp(value,"1"));update(&b,2);assert(!strcmp(value,"0"));
    a.editing=1;update(&a,300000);assert(strstr(status,"Editing"));
    a.visible=0;click(&a);update(&a,600000);assert(!strcmp(value,"1"));
    a.visible=1;update(&a,600000);assert(!strcmp(value,"2"));
    destroy(&a);assert(!a.user);assert(!create(&a));update(&a,700000);assert(!strcmp(value,"0"));
    destroy(&a);destroy(&b);
    puts("Widget example: independent placements, visibility, editing, throttling and recycling passed");
}
