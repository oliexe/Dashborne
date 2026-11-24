#include "model.h"
static int bounded(int n,int lo,int hi) {return n>=lo && n<=hi?n:UNKNOWN;}
void cluster_reset(struct cluster *c) {
    *c=(struct cluster){0};
    for(int i=0;i<HISTORY;++i)c->history[i]=UNKNOWN;
}
void cluster_update(struct cluster *c,const struct readings *r,unsigned int seconds) {
    c->data=*r;
    struct readings *d=&c->data;
    if(!r->connected || r->demo || r->gear<1 || r->gear>4) {
        c->mode=WAITING;c->count=0;c->sampled=0;
        for(int i=0;i<HISTORY;++i)c->history[i]=UNKNOWN;
        d->speed=d->limit=d->battery=d->range=d->temperature=d->gear=UNKNOWN;
        d->power_valid=d->nav_valid=d->left=d->right=0;d->body_known=d->body_open=0;return;
    }
    d->body_known &= BODY_ALL;d->body_open &= d->body_known;
    d->speed=bounded(r->speed,0,400);d->limit=bounded(r->limit,1,300);
    d->battery=bounded(r->battery,0,100);d->range=bounded(r->range,0,1500);
    d->temperature=bounded(r->temperature,r->fahrenheit?-148:-100,r->fahrenheit?176:80);
    d->gear=bounded(r->gear,1,4);
    d->power_valid=r->power_valid && r->power_kw>=-500 && r->power_kw<=1500;
    d->nav_valid=r->nav_valid && r->nav_distance>=0 && r->nav_distance<=10000 &&
        r->nav_minutes>=0 && r->nav_minutes<=10080 && r->nav_battery>=0 && r->nav_battery<=100;
    c->mode=d->gear==1?(r->charging?CHARGING:PARKED):DRIVING;
    if(!c->sampled || seconds!=c->last_second) {
        unsigned int gap=c->sampled?seconds-c->last_second:1;
        if(gap>HISTORY)gap=HISTORY;
        for(unsigned int n=0;n<gap;++n) {
            for(int i=0;i<HISTORY-1;++i)c->history[i]=c->history[i+1];
            c->history[HISTORY-1]=(n+1==gap && d->power_valid)?(int)(d->power_kw*10):UNKNOWN;
        }
        c->count+=(int)gap;if(c->count>HISTORY)c->count=HISTORY;
        c->last_second=seconds;c->sampled=1;
    }
}
