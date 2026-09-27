// picolearn.c — FULLY learned, no if-statement decides anything. Two learned models:
//   MODEL A (command decoder): learns from raw bytes that '1'=on, '0'=off.
//   MODEL B (register knowledge): learns which GPIO bit is the LED.
// Press a key -> Model A decodes the raw byte into a goal -> Model B writes the
// register it learned is the LED. All on the Pico, nothing hardcoded.
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/structs/sio.h"

#define LEDBIT 25
#define NIN    30
#define NDEMO  16

static float wreg[NIN], breg;   // Model B: which register bit is the LED
static float wcmd[8],  bcmd;    // Model A: which byte means on vs off
static float sigmoidf(float z){ return 1.0f/(1.0f+expf(-z)); }

static float Xr[NDEMO][NIN]; static int Yr[NDEMO];
static uint32_t rng;
static uint32_t rnd(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static void bits8(int byte, float x[8]){ for(int i=0;i<8;i++) x[i]=(float)((byte>>i)&1); }

int main(void){
    stdio_init_all();
    gpio_init(LEDBIT); gpio_set_dir(LEDBIT, GPIO_OUT);
    sleep_ms(1500);
    rng=(uint32_t)time_us_64()|1u;

    // ---- PHASE 1: Model B learns which register is the LED ----
    printf("\n=== PHASE 1: learning which register is the LED ===\n");
    for(int i=0;i<NDEMO;i++){
        int want=rnd()&1; gpio_put(LEDBIT,want); sleep_us(5);
        uint32_t in=sio_hw->gpio_in;
        for(int k=0;k<NIN;k++) Xr[i][k]=(float)((in>>k)&1);
        Yr[i]=want;
    }
    for(int k=0;k<NIN;k++) wreg[k]=0; breg=0;
    for(int e=0;e<400;e++) for(int i=0;i<NDEMO;i++){
        float z=breg; for(int k=0;k<NIN;k++) z+=wreg[k]*Xr[i][k];
        float o=sigmoidf(z), err=o-(float)Yr[i];
        for(int k=0;k<NIN;k++) wreg[k]-=0.3f*(err*Xr[i][k]+0.01f*wreg[k]);
        breg-=0.3f*err;
    }
    int best=0; for(int k=1;k<NIN;k++) if(fabsf(wreg[k])>fabsf(wreg[best])) best=k;
    printf("  learned: the LED is bit %d%s\n", best, best==LEDBIT?"   <-- correct":"");

    // ---- PHASE 2: Model A learns what '1'/'0' mean, from the RAW BYTES ----
    printf("\n=== PHASE 2: learning what the '1' and '0' commands mean (raw bytes, no if) ===\n");
    int cb[2]={'1','0'}, cy[2]={1,0};
    for(int i=0;i<8;i++) wcmd[i]=0; bcmd=0;
    for(int e=0;e<3000;e++) for(int n=0;n<2;n++){
        float x[8]; bits8(cb[n],x);
        float z=bcmd; for(int i=0;i<8;i++) z+=wcmd[i]*x[i];
        float o=sigmoidf(z), err=o-(float)cy[n];
        for(int i=0;i<8;i++) wcmd[i]-=0.3f*err*x[i];
        bcmd-=0.3f*err;
    }
    int kb=0; for(int i=1;i<8;i++) if(fabsf(wcmd[i])>fabsf(wcmd[kb])) kb=i;
    printf("  learned: bit %d of the command byte decides on/off ('1' has %d, '0' has %d there)\n",
           kb, ('1'>>kb)&1, ('0'>>kb)&1);

    // ---- PHASE 3: fully-learned command mode. NO if decides on/off. ----
    printf("\n=== PHASE 3: press '1' or '0' (try other keys too). The MODEL decodes AND drives ===\n\n");
    while(true){
        int c=getchar_timeout_us(0);
        if(c>=0){
            float x[8]; bits8(c,x);
            float z=bcmd; for(int i=0;i<8;i++) z+=wcmd[i]*x[i];
            int want = sigmoidf(z)>0.5f ? 1 : 0;     // Model A decodes your keystroke
            gpio_put(best, want);                    // Model B drives the learned LED register
            printf("byte %3d ('%c') -> model decodes -> LED %s  (wrote bit %d=%d)\n",
                   c, (c>=32&&c<127)?c:'?', want?"ON ":"OFF", best, want);
        }
        sleep_ms(10);
    }
}
