#ifndef DASHBORNE_USB_DEBUG_H
#define DASHBORNE_USB_DEBUG_H
void usb_debug_bind(void *runtime, void (*logger)(const char *));
void usb_debug_toggle(void);
void usb_debug_toggle_wifi(void);
int usb_debug_wifi_owned(void);
void usb_debug_shutdown(void);
const char *usb_debug_status(void);
int usb_debug_owned(void);
void usb_debug_collect(void);
void usb_debug_poll(void);
#endif
