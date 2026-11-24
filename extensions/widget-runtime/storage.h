#ifndef DBW_STORAGE_H
#define DBW_STORAGE_H
#define WIDGET_LAYOUT_RAM "/tmp/db-widgets.json"
int layout_prepare(void);
int layout_save(void);
void layout_cleanup(void);
#endif
