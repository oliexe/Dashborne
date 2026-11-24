#include "graph.h"
int graph_y(int value) {
    if(value>1000)value=1000;
    if(value<-500)value=-500;
    return GRAPH_ZERO-value*120/1000;
}
int graph_average(const int history[HISTORY]) {
    int total=0,count=0;
    for(int i=0;i<HISTORY;++i)if(history[i]!=UNKNOWN) {total+=history[i];++count;}
    return count?total/count:UNKNOWN;
}
struct graph_column graph_column(const int history[HISTORY],int x) {
    struct graph_column c={0};
    if(x<0 || x>=GRAPH_WIDTH)return c;
    int i=x/7,t=x%7,a=history[i];
    if(a==UNKNOWN)return c;
    int b=i+1<HISTORY?history[i+1]:a;
    if(b==UNKNOWN && t)return c;
    if(b==UNKNOWN)b=a;
    /* Clip samples before interpolation so the trace remains inside the plot. */
    int ya=graph_y(a),yb=graph_y(b);
    int y=ya+(yb-ya)*t/7,next=ya+(yb-ya)*(t<6?t+1:7)/7;
    if(i==HISTORY-1)next=y;
    c.valid=1;c.negative=y>GRAPH_ZERO;
    c.top=(y<next?y:next)-1;c.bottom=(y>next?y:next)+1;
    if(c.top<GRAPH_TOP)c.top=GRAPH_TOP;
    if(c.bottom>GRAPH_BOTTOM)c.bottom=GRAPH_BOTTOM;
    c.fill_top=y<GRAPH_ZERO?y:GRAPH_ZERO;
    c.fill_height=y<GRAPH_ZERO?GRAPH_ZERO-y:y-GRAPH_ZERO;
    return c;
}
