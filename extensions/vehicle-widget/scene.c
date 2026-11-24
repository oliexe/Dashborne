#include "scene.h"
#include "../tesla-ui/motion_data.h"

#define BODY_SLOT 65
#define HINGE_SLOT 66
#define FRAME_US 16667ull

static void clear(struct vehicle_scene *s,int slot) {
    if(s->assets[slot]>=0)s->ops.unload(s->context,slot);
    s->assets[slot]=-1;
}
static int load(struct vehicle_scene *s,int slot,int index) {
    if(s->assets[slot]==index)return 1;
    clear(s,slot);
    if(!s->ops.load(s->context,slot,vehicle_sprites[index].name)) {
        s->ops.unload(s->context,slot);return 0;
    }
    s->assets[slot]=index;return 1;
}
static int group_for(int mask) {
    const int groups[]={1,2,4,8,16,32,63};
    for(int i=0;i<7;++i)if(groups[i]==mask)return i;
    return -1;
}
static void hinge_group(struct vehicle_scene *s,int drive,int mask) {
    int group=group_for(mask);
    if(s->group==group && s->group_view==drive)return;
    for(int i=0;i<11;++i)clear(s,HINGE_SLOT+i);
    s->group=group;s->group_view=drive;
    if(group>=0)for(int i=0;i<11;++i)load(s,HINGE_SLOT+i,193+drive*77+group*11+i);
}
static void body_prepare(struct vehicle_scene *s,int drive,int mask) {
    s->body=mask;s->body_view=drive;s->dirty=1;
    s->body_failed=!load(s,BODY_SLOT,65+drive*64+mask);
    hinge_group(s,drive,mask);
}
void vehicle_scene_close(struct vehicle_scene *s) {
    if(!s || !s->active)return;
    /* Retire callbacks before releasing images. The adapter detaches its
       control before unloading the extension or destroying the widget. */
    s->active=0;s->queued=0;
    for(int i=0;i<VEHICLE_BITMAP_SLOTS;++i)clear(s,i);
}
int vehicle_scene_open(struct vehicle_scene *s,const struct vehicle_scene_ops *ops,void *context) {
    if(!s || !ops || !ops->load || !ops->unload || !ops->invalidate || !ops->draw || s->active)return -1;
    *s=(struct vehicle_scene){.ops=*ops,.context=context,.active=1,.frame=-1,.group=-1,.group_view=-1};
    for(int i=0;i<VEHICLE_BITMAP_SLOTS;++i)s->assets[i]=-1;
    for(int i=0;i<65;++i)if(!load(s,i,i)) {vehicle_scene_close(s);return -1;}
    return 0;
}
void vehicle_scene_update(struct vehicle_scene *s,int connected,int gear,
                          unsigned int known,unsigned int open,unsigned long long now) {
    if(!s || !s->active)return;
    int valid=connected && gear>=1 && gear<=4,drive=valid && gear!=1;
    int mask=valid && (known&63)==63?(int)(open&63):0;
    int immediate=!valid || !s->connected || !s->camera.primed;
    int changed_view=s->camera.primed && s->camera.target!=(drive?1:0);
    s->connected=valid;s->panels_known=valid && (known&63)==63;
    camera_target(&s->camera,drive,now,immediate || mask || s->body);
    if(immediate || changed_view) {
        if(s->body!=mask)s->dirty=1;
        if(mask)body_prepare(s,drive,mask);
        else {s->body=0;s->body_failed=0;clear(s,BODY_SLOT);hinge_group(s,drive,0);}
        s->body_view=drive;s->position=s->from=mask?1:0;
        s->target=mask?1:0;s->started=now;s->requested=mask;
    } else if(mask!=s->requested) {
        s->requested=mask;
        if(s->position==0 && mask)body_prepare(s,drive,mask);
        s->from=s->position;s->target=mask==s->body && mask?1:0;s->started=now;
        if(!s->body && mask){body_prepare(s,drive,mask);s->target=1;}
    }
    /* Loss of panel validity must not leave a previous open pose animating. */
    if(!s->panels_known && s->body) {
        s->body=s->requested=s->target=0;s->position=s->from=0;s->body_failed=0;
        clear(s,BODY_SLOT);hinge_group(s,drive,0);s->dirty=1;
    }
    vehicle_scene_tick(s,now);
}
void vehicle_scene_tick(struct vehicle_scene *s,unsigned long long now) {
    if(!s || !s->active || !s->camera.primed)return;
    int frame=camera_frame(&s->camera,now),old=(int)(s->position*12+.5f);
    float t=now>=s->started?(float)(now-s->started)/450000.0f:1;
    if(t>1)t=1;
    t=t*t*(3-2*t);s->position=s->from+(s->target-s->from)*t;
    if(s->position==0 && !s->requested) {s->body=0;s->body_failed=0;clear(s,BODY_SLOT);}
    if(s->position==0 && s->requested && s->requested!=s->body) {
        body_prepare(s,s->body_view,s->requested);s->from=0;s->target=1;s->started=now;
    }
    if(frame!=s->frame || old!=(int)(s->position*12+.5f)){s->frame=frame;s->dirty=1;}
    if(s->dirty && (!s->next_frame || now>=s->next_frame || s->next_frame-now>FRAME_US)) {
        s->dirty=0;s->next_frame=now+FRAME_US;
        if(!s->queued){s->queued=1;s->ops.invalidate(s->context);}
    }
}
void vehicle_scene_draw(struct vehicle_scene *s) {
    if(!s || !s->active || !s->camera.primed)return;
    s->queued=0;
    if(s->body_failed)return;
    int slot=s->frame;
    if((slot==0 || slot==64) && s->body) {
        int frame=(int)(s->position*12+.5f);
        if(frame>0 && frame<12 && s->assets[HINGE_SLOT+frame-1]>=0)slot=HINGE_SLOT+frame-1;
        else if(frame>=12 || (frame>0 && s->target))slot=BODY_SLOT;
    }
    if(slot<0 || s->assets[slot]<0)return;
    const struct vehicle_sprite *v=&vehicle_sprites[s->assets[slot]];
    s->ops.draw(s->context,slot,v->x,v->y,v->w,v->h);
}
