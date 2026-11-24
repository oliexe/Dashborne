#include <assert.h>
#include "../graph.h"
static void check_runs(const int h[HISTORY]) {
    struct graph_runs runs;graph_runs(h,&runs);
    assert(runs.fills<=GRAPH_WIDTH && runs.traces<=GRAPH_WIDTH);
    for(int kind=0;kind<2;++kind) {
        const struct graph_run *r=kind?runs.trace:runs.fill;
        int count=kind?runs.traces:runs.fills,seen[GRAPH_WIDTH]={0};
        for(int i=0;i<count;++i) {
            assert(r[i].width>0 && r[i].height>0 && r[i].x>=0 && r[i].x+r[i].width<=GRAPH_WIDTH);
            if(i)assert(r[i-1].x+r[i-1].width<=r[i].x);
            for(int x=r[i].x;x<r[i].x+r[i].width;++x) {
                struct graph_column c=graph_column(h,x);assert(!seen[x]++ && c.valid);
                assert(r[i].negative==c.negative);
                assert(r[i].top==(kind?c.top:c.fill_top));
                assert(r[i].height==(kind?c.bottom-c.top+1:c.fill_height));
            }
        }
        for(int x=0;x<GRAPH_WIDTH;++x) {
            struct graph_column c=graph_column(h,x);
            assert(seen[x]==(c.valid && (kind || c.fill_height)));
        }
    }
}
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
    struct graph_runs runs;graph_runs(h,&runs);assert(!runs.fills && runs.traces==1);
    for(int i=0;i<HISTORY;++i)h[i]=340;
    graph_runs(h,&runs);assert(runs.fills==1 && runs.traces==1);
    check_runs(h);
    /* Merging must preserve every pixel, including missing intervals and
       steep clipped transitions. Keep the sequence deterministic. */
    unsigned int seed=7;
    for(int sample=0;sample<1000;++sample) {
        for(int i=0;i<HISTORY;++i) {
            seed=seed*1664525u+1013904223u;
            h[i]=seed%5==0?UNKNOWN:(int)(seed%20001)-5000;
        }
        check_runs(h);
    }
}
