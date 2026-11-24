#ifndef VEHICLE_STORAGE_H
#define VEHICLE_STORAGE_H
#define VEHICLE_LAYOUT_RAM "/tmp/db-widgets.json"
int layout_prepare(void);
int layout_save(void);
void layout_cleanup(void);
#endif
