#ifndef MIRROR_SESSION_H
#define MIRROR_SESSION_H
#include "aspect.h"
struct video_status {
    unsigned int generation;
    unsigned char ready, reserved;
    unsigned short width, height, padding;
};
_Static_assert(sizeof(struct video_status)==12, "Official video-status ABI");
_Static_assert(__builtin_offsetof(struct video_status,width)==6, "Official video width offset");
struct session_ops {
    int (*mode)(int airplay), (*enable)(void), (*manual_off)(void), (*type)(void);
    void (*restart_mirror)(void), (*restart_stock)(void), (*hide)(void), (*hold)(int);
    int (*show)(unsigned int,int aspect);
    struct video_status (*video)(void);
    void (*play)(void);
};
enum { MIRROR_CLOSED, MIRROR_WAITING, MIRROR_PLAYING, MIRROR_ERROR };
struct mirror_session {
    const struct session_ops *ops;
    int state, changed, held, play_requested;
    unsigned int previous, displayed;
    int aspect, displayed_aspect;
};
int session_open(struct mirror_session *);
int session_close(struct mirror_session *);
void session_tick(struct mirror_session *);
void session_next_aspect(struct mirror_session *);
#endif
