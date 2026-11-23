#ifndef DASHBORNE_USB_CONFIGFS_H
#define DASHBORNE_USB_CONFIGFS_H
int usb_configfs_available(void);
int usb_configfs_prepare(void (*logger)(const char *));
int usb_configfs_bind(void); /* 1 bound, 0 waiting, -1 failed */
int usb_configfs_stop(void);
const char *usb_configfs_status(void);
const char *usb_configfs_error(void);
#endif
