#include <assert.h>
#include "../presentation.c"
static union {unsigned int align; unsigned char bytes[28];} storage;
static int refreshes,logs;
static void log_message(const struct dashborne_host *h,const char *s) {(void)h;(void)s;++logs;}
static struct dashborne_host host={.log=log_message};
static void stock_refresh(void *p) {
    assert(p==manager);++refreshes;
    manager[25]=manager[24] && manager[26] && manager[27];
}
int main(void) {
    presentation_update(1); presentation_reset(); /* Partial activation. */
    api=&host;manager=storage.bytes;refresh=stock_refresh;
    manager[26]=manager[27]=1;
    presentation_update(1);assert(!acquired && !refreshes); /* No widget. */
    *(unsigned int *)(manager+16)=1;
    presentation_update(0);assert(!refreshes);
    presentation_update(1);assert(acquired && manager[24] && manager[25] && refreshes==1);
    presentation_update(1);assert(refreshes==1); /* No redraw loop. */
    manager[26]=0;presentation_update(1);
    assert(!manager[25] && !manager[26] && manager[27]); /* Transition preserved. */
    manager[26]=1;manager[27]=0;presentation_update(1);
    assert(!manager[25] && manager[26] && !manager[27]); /* Modal preserved. */
    manager[27]=1;presentation_update(1);assert(manager[25]);
    presentation_update(0);assert(!acquired && !manager[24] && !manager[25]);
    manager[24]=1;presentation_update(1);presentation_release();
    assert(manager[24]==1); /* Preserve a preexisting stock grant. */
    presentation_update(1);manager[24]=0;presentation_release();
    assert(manager[24]==0); /* Preserve a later stock revocation. */
    presentation_update(1);*(unsigned int *)(manager+16)=0;presentation_update(1);
    assert(!acquired && !manager[24]); /* Widget removed while streaming. */
    *(unsigned int *)(manager+16)=1;presentation_update(1);presentation_reset();
    assert(!manager && !api && !acquired && !storage.bytes[24]);
    presentation_reset();assert(logs>0);
    return 0;
}
