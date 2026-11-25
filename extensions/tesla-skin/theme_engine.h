#ifndef DASHBORNE_THEME_ENGINE_H
#define DASHBORNE_THEME_ENGINE_H
/* Palette conversion retains geometry, alpha and saturated semantic colors. */
int theme_png(const unsigned char *input, unsigned int length, int source_light,
              unsigned int background, unsigned int foreground,
              unsigned char **output, unsigned int *output_length);
void theme_resources_init(const char *directory, const unsigned int colors[2][7]);
const char *theme_resource(const char *path, int light);
void theme_resources_clear(void);
#endif
