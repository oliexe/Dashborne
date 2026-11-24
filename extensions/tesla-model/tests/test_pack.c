#include "../pack.h"
#include <assert.h>
#include <string.h>
static int parse(const char *s){struct tm_pack p;return tm_pack_parse(&p,s,(unsigned int)strlen(s));}
int main(void) {
    assert(!parse("tesla-model-v1\nhighland-gray\nhighland-red\n"));
    assert(!parse("tesla-model-v1\nmodel-s-silver"));
    assert(parse("tesla-model-v1\n"));assert(parse("tesla-model-v1\n../escape\n"));
    assert(parse("tesla-model-v1\na\na\n"));assert(parse("tesla-model-v2\na\n"));
    assert(parse("tesla-model-v1\n\n"));assert(parse("tesla-model-v1\n/absolute\n"));
    assert(parse("tesla-model-v1\n-white\n"));
}
