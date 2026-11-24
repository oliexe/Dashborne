#ifndef DASHBORNE_CORE_SERVICES_H
#define DASHBORNE_CORE_SERVICES_H
#include "../extensions/dashborne_extension.h"
int core_widgets_start(const struct dashborne_host *);
int core_widgets_stop(void);
int core_extra_start(const struct dashborne_host *);
int core_extra_stop(void);
#endif
