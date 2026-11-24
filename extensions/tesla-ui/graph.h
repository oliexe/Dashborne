#ifndef TESLA_UI_GRAPH_H
#define TESLA_UI_GRAPH_H
#include "model.h"
#define GRAPH_WIDTH 414
#define GRAPH_TOP 142
#define GRAPH_ZERO 262
#define GRAPH_BOTTOM 322
#define GRAPH_LEFT 185
#define GRAPH_FILL_ID 600000
#define GRAPH_TRACE_ID 601000
struct graph_column {int valid,top,bottom,fill_top,fill_height,negative;};
/* Linear interpolation only between adjacent valid samples; values are tenths of kW. */
struct graph_column graph_column(const int history[HISTORY],int x);
int graph_average(const int history[HISTORY]);
int graph_y(int value);
#endif
