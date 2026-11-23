#ifndef MIRROR_WINDOW_H
#define MIRROR_WINDOW_H
#include "dashborne_extension.h"
#include "session.h"
int window_bind(const struct dashborne_host *, struct mirror_session *);
void window_open(void);
int window_cleanup(void);
#endif
