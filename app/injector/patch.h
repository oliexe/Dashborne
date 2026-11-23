#ifndef DASHBORNE_PATCH_H
#define DASHBORNE_PATCH_H
/* Returned buffer belongs to the caller. Zero means incompatible/invalid input. */
int patch_layout(const unsigned char *base, unsigned int base_size,
                 const unsigned char *patch, unsigned int patch_size,
                 unsigned char **output, unsigned int *output_size);
void injector_apply(void (*logger)(const char *));
#endif
