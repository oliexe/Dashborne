#include "dashborne_widget.h"
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
/* Change these along with the manifest ID when copying this directory. */
#define WIDGET_ID "widget-example"
#define WIDGET_TITLE "Widget Example"
#define WIDGET_TAB DBW_SPECIAL
#define WIDGET_CATEGORY 1 /* Gauges; all 25 categories are supported. */
static const struct dashborne_host *host;
static const struct dashborne_widgets *widgets;
/* One state per pooled view: two placements do not share their tap count. */
struct state {unsigned int taps;unsigned long long next;};
static struct state states[36];
static int create(struct dashborne_widget_view *view) {
    if(view->instance>=36)return -1;
    struct state *state=&states[view->instance];*state=(struct state){0};view->user=state;
    widgets->text(view->action,"Tap");return 0;
}
static void update(struct dashborne_widget_view *view,unsigned long long now) {
    struct state *state=view->user;
    if(!view->visible || now<state->next)return;
    state->next=now+250000;char value[32],status[64];
    snprintf(value,sizeof(value),"%u",state->taps);
    snprintf(status,sizeof(status),"%d × %d px%s",view->width,view->height,view->editing?" · Editing":"");
    widgets->text(view->value,value);widgets->text(view->status,status);
}
static void click(struct dashborne_widget_view *view) {
    struct state *state=view->user;++state->taps;state->next=0;
}
static void destroy(struct dashborne_widget_view *view) {view->user=0;}
static int activate(const struct dashborne_host *api) {
    host=api;widgets=dashborne_widget_api();if(!widgets)return -1;
    static const struct dashborne_widget widget={
        DASHBORNE_WIDGET_ABI,sizeof(widget),WIDGET_ID,WIDGET_TITLE,
        "A responsive example with independent state for each placement.",
        DBW_ALL_SIZES,WIDGET_TAB,WIDGET_CATEGORY,create,update,0,click,destroy,0
    };
    return widgets->register_widget(api,&widget);
}
static int deactivate(void) {
    if(widgets && widgets->unregister_widget(host))return -1;
    host=0;widgets=0;return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(extension),WIDGET_ID,activate,deactivate};return &extension;
}
