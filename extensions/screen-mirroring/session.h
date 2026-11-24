#ifndef MIRROR_SESSION_H
#define MIRROR_SESSION_H
#include "aspect.h"
#include "protocol.h"
struct video_status {
    unsigned int generation;
    unsigned char ready, reserved;
    unsigned short width, height, padding;
};
_Static_assert(sizeof(struct video_status)==12, "Official video-status ABI");
_Static_assert(__builtin_offsetof(struct video_status,width)==6, "Official video width offset");
struct session_ops {
    int (*mode)(int protocol), (*enable)(void), (*manual_off)(void), (*type)(void);
    void (*restart_mirror)(void), (*restart_stock)(void), (*hide)(void), (*hold)(int);
    int (*show)(unsigned int,int aspect);
    struct video_status (*video)(void);
    void (*play)(void);
    void (*network)(int mode);
    int (*network_mode)(void);
};
enum { MIRROR_CLOSED, MIRROR_WAITING, MIRROR_PLAYING, MIRROR_ERROR, MIRROR_STARTING, MIRROR_RESTORING };
enum { MIRROR_NO_ERROR, MIRROR_START_FAILED, MIRROR_NETWORK_FAILED, MIRROR_RESTORE_FAILED };
enum { MIRROR_NETWORK_TICKS=300 }; /* 30 seconds at the viewer's 100 ms timer. */
struct mirror_session {
    const struct session_ops *ops;
    int state, changed, held, play_requested;
    unsigned int previous, displayed;
    int aspect, displayed_aspect;
    int protocol, error, network_owned, restore_requested, wait_ticks;
    unsigned int supported;
};
int session_open(struct mirror_session *);
int session_close(struct mirror_session *);
void session_tick(struct mirror_session *);
void session_next_aspect(struct mirror_session *);
#endif
