#ifndef BETTER_DISPLAY_MODEL_H
#define BETTER_DISPLAY_MODEL_H
/* ceil(255 * 0.02): never expose a backlight below two percent. */
#define DISPLAY_MIN_BRIGHTNESS 6
struct display_profile { int brightness, black; };
struct display_settings { int enabled, mode, day_start, night_start; struct display_profile day, night; };
void display_defaults(struct display_settings *);
int display_valid(const struct display_settings *);
/* -1: no valid clock in automatic mode; 0: day; 1: night. */
int display_profile_at(const struct display_settings *,int minute);
int display_time_step(int minute,int delta);
int display_settings_load(struct display_settings *);
int display_settings_save(const struct display_settings *);
#endif
