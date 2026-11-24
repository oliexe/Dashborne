#ifndef TESLA_UI_WINDOW_H
#define TESLA_UI_WINDOW_H
#include "dashborne_extension.h"
/* Logical UI actions, not device key codes. The stock adapter must call this
   on the UI thread only after its own navigation/configuration gates pass. */
enum cluster_input {CLUSTER_PREVIOUS=1,CLUSTER_NEXT,CLUSTER_SELECT,CLUSTER_BACK,
                    CLUSTER_PREVIOUS_PAGE,CLUSTER_NEXT_PAGE};
int cluster_window_input(enum cluster_input action,int navigation_allowed);
int cluster_window_bind(const struct dashborne_host *);
void cluster_window_open(void);
int cluster_window_cleanup(void);
#endif
