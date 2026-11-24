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
        .abi_version=DASHBORNE_WIDGET_ABI,.struct_size=sizeof(widget),
        .id=WIDGET_ID,.title=WIDGET_TITLE,
        .description="A responsive example with independent state for each placement.",
        .sizes=DBW_ALL_SIZES,.tab=WIDGET_TAB,.category=WIDGET_CATEGORY,
        .create=create,.update=update,.click=click,.destroy=destroy,
#define PREVIEW(size) "../../../mnt/extsd/Dashborne/extensions/" WIDGET_ID "/assets/preview-" size ".png"
        .preview_images={PREVIEW("1x1"),PREVIEW("2x1"),PREVIEW("1x2"),PREVIEW("2x2"),
                         PREVIEW("3x2"),PREVIEW("4x2"),PREVIEW("5x2"),PREVIEW("6x2")}
#undef PREVIEW
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
