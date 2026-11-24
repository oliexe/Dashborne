#include "data.h"
void vehicle_panel_read(struct readings *r,const volatile unsigned char *data) {
    r->body_known=r->body_open=0;
    if(!data || !r->connected || r->demo)return;
    /* The six stock action-status getters use these slowData bytes in 6.11.2.
       Unexpected encodings stay unknown instead of implying a closed panel. */
    const unsigned int offsets[]={0xd1,0xd3,0xd0,0xd2,0xd4,0xd5};
    for(unsigned int i=0;i<6;++i) {
        unsigned int value=data[offsets[i]];
        if(value<=1){r->body_known|=1u<<i;if(value)r->body_open|=1u<<i;}
    }
}
