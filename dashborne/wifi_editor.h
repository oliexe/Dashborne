#ifndef DASHBORNE_WIFI_EDITOR_H
#define DASHBORNE_WIFI_EDITOR_H
struct wifi_editor {
    char ssid[33], password[64];
    int active, field, shift, symbols, reveal;
    const char *error;
};
int wifi_editor_open(struct wifi_editor *);
void wifi_editor_close(struct wifi_editor *);
char wifi_editor_key(const struct wifi_editor *, unsigned int index);
void wifi_editor_type(struct wifi_editor *, char);
void wifi_editor_backspace(struct wifi_editor *);
void wifi_editor_clear_field(struct wifi_editor *);
void wifi_editor_display(const struct wifi_editor *, int field, char out[65]);
int wifi_editor_save(struct wifi_editor *);
void wifi_saved_name(char out[33]);
#endif
