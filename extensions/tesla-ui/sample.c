#include "sample.h"
const char *sample_read(struct readings *r,unsigned long long elapsed) {
    static const char *names[SAMPLE_SCENES]={"Park","Front left door","Rear left door",
        "Front right door","Rear right door","Frunk","Trunk","All panels open",
        "Park to Drive","Regeneration","Left turn","Right turn","Left blind spot",
        "Blind spot + turn","Right warning","Hazards","Reverse","Charging","Disconnected"};
    /* Start with Park, then shift after two seconds to show the camera orbit. */
    unsigned int scene=(unsigned int)((elapsed/SAMPLE_SCENE_US%SAMPLE_SCENES+8)%SAMPLE_SCENES);
    float progress=(float)(elapsed%SAMPLE_SCENE_US)/(float)SAMPLE_SCENE_US;
    int blink=(elapsed/500000)%2==0;
    *r=(struct readings){.connected=1,.gear=1,.speed=0,.limit=80,.battery=68,.range=312,
        .temperature=21,.body_known=BODY_ALL,.blind_known=SIDES_ALL,
        .power_valid=1,.power_kw=0,.clock="12:34"};
    if(scene>=1 && scene<=6)r->body_open=1u<<(scene-1);
    if(scene==7)r->body_open=BODY_ALL;
    if(scene>=8 && scene<=16) {
        r->gear=4;r->speed=72;r->power_kw=24;
        r->nav_valid=1;r->nav_distance=42;r->nav_minutes=36;r->nav_battery=57;
    }
    switch(scene) {
    case 8:
        if(progress<1.0f/3) {r->gear=1;r->speed=0;r->power_kw=0;r->nav_valid=0;}
        else {float p=(progress-1.0f/3)*1.5f;r->speed=(int)(90*p);r->power_kw=15+70*p;}
        break;
    case 9:r->speed=90-(int)(65*progress);r->power_kw=-12-25*progress;break;
    case 10:r->left=blink;break;
    case 11:r->right=blink;break;
    case 12:r->blind_active=SIDE_LEFT;break;
    case 13:r->blind_active=SIDE_LEFT;r->left=blink;break;
    case 14:r->blind_active=r->blind_urgent=SIDE_RIGHT;break;
    case 15:r->left=r->right=blink;break;
    case 16:r->gear=2;r->speed=6;r->power_kw=3;r->nav_valid=0;break;
    case 17:r->charging=1;r->power_kw=11;break;
    case 18:r->connected=0;break;
    }
    return names[scene];
}
