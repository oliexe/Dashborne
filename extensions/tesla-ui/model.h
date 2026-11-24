#ifndef TESLA_UI_MODEL_H
#define TESLA_UI_MODEL_H
#define HISTORY 60
#define UNKNOWN (-10000)
enum cluster_mode { WAITING, PARKED, DRIVING, CHARGING };
/* Values are already in the stock user's units, except power (always kW). */
struct readings {
    int connected, demo, speed, limit, battery, range, temperature, gear, charging;
    int mph, miles, fahrenheit;
    float power_kw, nav_distance, nav_minutes, nav_battery;
    int power_valid, nav_valid, left, right;
    char clock[16];
};
struct cluster {
    enum cluster_mode mode;
    struct readings data;
    int history[HISTORY], count;
    unsigned int last_second;
    int sampled;
};
void cluster_reset(struct cluster *);
void cluster_update(struct cluster *,const struct readings *,unsigned int seconds);
#endif
