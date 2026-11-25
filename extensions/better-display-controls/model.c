#include "model.h"
void display_defaults(struct display_settings *s) {
    *s=(struct display_settings){0,0,420,1200,{160,50},{16,50}};
}
static int profile_valid(const struct display_profile *p) {
    return p->brightness>=DISPLAY_MIN_BRIGHTNESS && p->brightness<=255 && p->black>=0 && p->black<=100;
}
int display_valid(const struct display_settings *s) {
    return s && s->enabled>=0 && s->enabled<=1 && s->mode>=0 && s->mode<=2 &&
        s->day_start>=0 && s->day_start<1440 && s->night_start>=0 && s->night_start<1440 &&
        s->day_start!=s->night_start && profile_valid(&s->day) && profile_valid(&s->night);
}
int display_profile_at(const struct display_settings *s,int minute) {
    if(!display_valid(s))return -1;
    if(s->mode)return s->mode-1;
    if(minute<0 || minute>=1440)return -1;
    int day=s->day_start<s->night_start?
        minute>=s->day_start && minute<s->night_start:
        minute>=s->day_start || minute<s->night_start;
    return !day;
}
int display_time_step(int minute,int delta) {int n=(minute+delta)%1440;return n<0?n+1440:n;}
