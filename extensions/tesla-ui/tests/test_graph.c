#include <assert.h>
#include "../graph.h"
int main(void) {
    int h[HISTORY];
    for(int i=0;i<HISTORY;++i)h[i]=UNKNOWN;
    assert(graph_average(h)==UNKNOWN);
    for(int x=0;x<GRAPH_WIDTH;++x)assert(!graph_column(h,x).valid);
    h[59]=100;
    assert(graph_average(h)==100);
    assert(graph_column(h,413).valid);
    assert(!graph_column(h,412).valid);
    h[57]=-500;h[58]=UNKNOWN;
    for(int x=400;x<413;++x)assert(!graph_column(h,x).valid);
    for(int i=0;i<HISTORY;++i)h[i]=i%2?15000:-5000;
    for(int x=0;x<GRAPH_WIDTH;++x) {
        struct graph_column c=graph_column(h,x);
        assert(c.valid && c.top>=GRAPH_TOP && c.bottom<=GRAPH_BOTTOM);
        assert(c.bottom>=c.top && c.fill_height>=0);
        assert(c.fill_top>=GRAPH_TOP && c.fill_top+c.fill_height<=GRAPH_BOTTOM);
    }
    assert(!graph_column(h,-1).valid && !graph_column(h,GRAPH_WIDTH).valid);
    assert(graph_y(1000)==GRAPH_TOP && graph_y(0)==GRAPH_ZERO && graph_y(-500)==GRAPH_BOTTOM);
    for(int i=0;i<HISTORY;++i)h[i]=0;
    assert(graph_average(h)==0);
    for(int x=0;x<GRAPH_WIDTH;++x)assert(graph_column(h,x).fill_height==0);
}
