/* Electrical power follows the fingerprinted stock getStat_CurrentPower path,
   before user-unit conversion. No speed/accelerator estimate is substituted. */
static int power_kw(const volatile unsigned char *quick,const volatile unsigned char *medium,const volatile unsigned char *slow) {
    int charging=*(const volatile int *)(medium+0x2c);
    float a,b;
    if(charging>0) {
        a=(float)*(const volatile int *)(slow+0xa0)*0.073f;
        b=(float)*(const volatile int *)(slow+0x9c)*0.073f;
    } else {
        a=(float)*(const volatile int *)(quick+4)*0.01f;
        b=(float)*(const volatile int *)(quick+8)*-0.1f;
    }
    float power=a*b/100.0f;
    if(power < -2000.0f || power > 2000.0f)return 0;
    return (int)power;
}
static int power_width(int kw) {
    int width=kw<0?-kw*165/100:kw*165/400;
    return width>165?165:width;
}
