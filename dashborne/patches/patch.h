#ifndef DASHBORNE_PATCH_H
#define DASHBORNE_PATCH_H
/* Returned buffer belongs to the caller. Zero means incompatible/invalid input. */
int patch_layout(const unsigned char *base, unsigned int base_size,
                 const unsigned char *patch, unsigned int patch_size,
                 unsigned char **output, unsigned int *output_size);
struct layout_patch { const unsigned char *data; unsigned int size; };
int patch_layout_many(const unsigned char *base, unsigned int base_size,
                      const struct layout_patch *patches, unsigned int count,
                      unsigned char **output, unsigned int *output_size);
#endif
