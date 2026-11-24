#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../catalog.c"
static char saved[LAST_SOURCE-FIRST_SOURCE][64];
static int live,bind_fail,write_fail,published,removed,unbinds;
int runtime_live(void){return live;}
int runtime_bind(void){return bind_fail;}
int runtime_unbind(void){++unbinds;return 0;}
int registry_publish(unsigned int source,const struct dashborne_widget *w){assert(source>=FIRST_SOURCE && source<=LAST_SOURCE);if(w)++published;else ++removed;return 0;}
void views_text(void *c,const char *s){(void)c;(void)s;}
void views_invalidate(struct dashborne_widget_view *v){(void)v;}
int config_load(void){return 1;}
int config_get(const char *key,char *out,unsigned int n){unsigned int source;assert(sscanf(key,"widgets.source.%u",&source)==1);source-=FIRST_SOURCE+1;assert(source<sizeof(saved)/sizeof(saved[0]));if(!saved[source][0])return 0;snprintf(out,n,"%s",saved[source]);return 1;}
int config_set(const char *key,const char *value){unsigned int source;assert(sscanf(key,"widgets.source.%u",&source)==1);if(write_fail)return 0;snprintf(saved[source-FIRST_SOURCE-1],64,"%s",value);return 1;}
int config_commit(void){return !write_fail;}
void config_clear(void){}
int main(void) {
    const struct dashborne_host core={0},a={0},b={0},car={0};
    struct dashborne_widget w={.abi_version=1,.struct_size=sizeof(w),.id="one",.title="One",.description="First",.sizes=255,.tab=2,.category=1};
    assert(!core_widgets_start(&core));
    assert(!register_widget(&a,&w));unsigned int first=providers[0].source;assert(first==FIRST_SOURCE+1);
    assert(register_widget(&b,&w)==-1);
    struct dashborne_widget other=w;other.id="two";assert(!register_widget(&b,&other));unsigned int second=providers[1].source;assert(first!=second);
    struct dashborne_widget old=w;old.id="vehicle-widget";assert(!register_widget(&car,&old) && provider_for(FIRST_SOURCE));
    live=1;assert(unregister_widget(&a)==-1 && provider_for(first));assert(register_widget(&a,&other)==-1);live=0;
    assert(!unregister_widget(&a) && !unregister_widget(&b) && !unregister_widget(&car));
    assert(!provider_for(first) && removed==3 && unbinds==1);
    assert(!register_widget(&b,&other) && provider_for(second)->host==&b);
    assert(!register_widget(&a,&w) && provider_for(first)->host==&a);
    assert(!unregister_widget(&a) && !unregister_widget(&b));
    const unsigned int counts[]={4,12,9};
    for(unsigned int t=0;t<3;++t)for(unsigned int c=0;c<counts[t];++c){w.tab=t;w.category=c;assert(!register_widget(&a,&w));assert(!unregister_widget(&a));}
    w.struct_size=DASHBORNE_WIDGET_V1_SIZE;assert(!register_widget(&a,&w));assert(!unregister_widget(&a));
    w.struct_size=DASHBORNE_WIDGET_V1_SIZE-1;assert(register_widget(&a,&w)==-1);w.struct_size=sizeof(w);
    w.tab=3;assert(register_widget(&a,&w)==-1);w.tab=0;w.category=4;assert(register_widget(&a,&w)==-1);
    w.category=0;w.sizes=256;assert(register_widget(&a,&w)==-1);w.sizes=0;assert(register_widget(&a,&w)==-1);w.sizes=255;
    w.id="../escape";assert(register_widget(&a,&w)==-1);w.id="new";write_fail=1;assert(register_widget(&a,&w)==-1);write_fail=0;
    bind_fail=1;assert(register_widget(&a,&w)==-1);bind_fail=0;
    assert(!core_widgets_stop());
    puts("Widget catalog: stable IDs across ordering/disable, legacy car, duplicate ownership, 25 categories, invalid metadata and failures passed");
}
