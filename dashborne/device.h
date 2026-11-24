#ifndef DASHBORNE_DEVICE_H
#define DASHBORNE_DEVICE_H
struct device_row { const char *label; char value[192]; };
void device_bind(void *stock, void *runtime);
void device_refresh(void);
unsigned int device_count(void);
const struct device_row *device_row(unsigned int index);
#endif
