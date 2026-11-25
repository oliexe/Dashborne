#include <assert.h>
#include <stdio.h>
#include "../model.h"
int main(void){
 struct display_settings s;display_defaults(&s);assert(!s.enabled&&display_valid(&s));
 assert(display_profile_at(&s,-1)==-1);
 for(int m=0;m<1440;m++)assert(display_profile_at(&s,m)==!(m>=420&&m<1200));
 s.day_start=1200;s.night_start=420;
 for(int m=0;m<1440;m++)assert(display_profile_at(&s,m)==!(m>=1200||m<420));
 s.mode=1;assert(display_profile_at(&s,-1)==0);s.mode=2;assert(display_profile_at(&s,-1)==1);
 s.night_start=s.day_start;assert(!display_valid(&s));
 display_defaults(&s);for(int low=0;low<6;low++){s.day.brightness=low;assert(!display_valid(&s));}s.day.brightness=6;assert(display_valid(&s));s.day.brightness=255;assert(display_valid(&s));
 s.night.black=101;assert(!display_valid(&s));s.night.black=0;assert(display_valid(&s));
 assert(display_time_step(0,-15)==1425&&display_time_step(1430,15)==5);
 puts("display model: all minutes, midnight wrap, missing clock, manual profiles and limits passed");
}
