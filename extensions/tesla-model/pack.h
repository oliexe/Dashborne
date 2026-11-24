#ifndef TESLA_MODEL_PACK_H
#define TESLA_MODEL_PACK_H
#define TM_VARIANTS 16
struct tm_pack { int count; char ids[TM_VARIANTS][48]; };
int tm_pack_parse(struct tm_pack *,const char *,unsigned int);
int tm_pack_load(struct tm_pack *,const char *directory);
int tm_image_valid(const char *path);
#endif
