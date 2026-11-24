#ifndef TESLA_UI_MODEL_H
#define TESLA_UI_MODEL_H
#define HISTORY 60
#define UNKNOWN (-10000)
enum body_panel {
    FRONT_LEFT=1, REAR_LEFT=2, FRONT_RIGHT=4, REAR_RIGHT=8, FRUNK=16, TRUNK=32,
    BODY_ALL=63
};
enum cluster_mode { WAITING, PARKED, DRIVING, CHARGING };
enum vehicle_side {SIDE_LEFT=1,SIDE_RIGHT=2,SIDES_ALL=3};
/* Values are already in the stock user's units, except power (always kW). */
struct readings {
    int connected, demo, speed, limit, battery, range, temperature, gear, charging;
    int mph, miles, fahrenheit;
    float power_kw, nav_distance, nav_minutes, nav_battery;
    int power_valid, nav_valid, left, right;
    /* Unknown is not closed: a panel is trustworthy only when its known bit is set. */
    unsigned int body_known, body_open;
    unsigned int blind_known, blind_active, blind_urgent;
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
