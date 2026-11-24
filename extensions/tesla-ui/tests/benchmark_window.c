/* Measures our C update path with fake native setters. This deliberately does
   not stand in for PNG decoding, native paint, display scanout or device FPS. */
#define main window_regressions
#include "test_window.c"
#undef main
#include <time.h>
#define ITERATIONS 10000
static unsigned long long timings[ITERATIONS];
static unsigned long long host_ns(void) {
    struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));
    return (unsigned long long)t.tv_sec*1000000000+(unsigned long)t.tv_nsec;
}
static int compare(const void *a,const void *b) {
    unsigned long long x=*(const unsigned long long *)a,y=*(const unsigned long long *)b;
    return (x>y)-(x<y);
}
static void benchmark(const char *name,int scenario) {
    struct dashborne_host host={0};api=&host;
    fixture=(struct readings){.connected=1,.gear=4,.speed=72,.limit=80,.battery=68,.range=312,
        .temperature=21,.body_known=BODY_ALL,.power_valid=1,.power_kw=34};
    seconds=100;subsecond=0;control_count=0;memset(controls,0,sizeof(controls));
    void *s=create();created(s);
    for(int i=0;i<HISTORY;++i)state.history[i]=340;
    state.count=HISTORY;history_drawn=0;render(source_microseconds(),1);
    int initial_writes=writes,initial_lookups=lookups,initial_pictures=pictures;
    for(int i=0;i<ITERATIONS;++i) {
        subsecond+=16000;
        if(scenario==1) {
            fixture.power_kw=(float)((i/7)%500-100);
            fixture.speed=(i/7)%130;
        } else if(scenario==2) {
            /* Force a dense, completely changed history on each iteration,
               far more work than the production once-per-second update. */
            for(int n=0;n<HISTORY;++n)state.history[n]=(n+i)%2?1000:-500;
            history_drawn=0;next_sample=source_microseconds();
        }
        unsigned long long begin=host_ns();timer(s,TIMER);timings[i]=host_ns()-begin;
    }
    qsort(timings,ITERATIONS,sizeof(timings[0]),compare);
    printf("%s: p50=%.3f us p95=%.3f us p99=%.3f us max=%.3f us; setters=%d lookups=%d images=%d\n",
        name,timings[ITERATIONS/2]/1000.0,timings[ITERATIONS*95/100]/1000.0,
        timings[ITERATIONS*99/100]/1000.0,timings[ITERATIONS-1]/1000.0,
        writes-initial_writes,lookups-initial_lookups,pictures-initial_pictures);
    destroy(s);assert(!cluster_window_cleanup());
}
int main(void) {
    char *args[]={"window"};window_regressions(1,args);
    puts("Host CPU timings with fake telemetry/native UI; not device frame times.");
    benchmark("steady",0);benchmark("changing speed/power",1);benchmark("dense graph rebuild",2);
}
