// setuplearn.c -- FULLY on-chip. No host script and no Pi decides anything.
// The Pico itself runs the trial-and-reward loop that discovers which pin,
// which mux (function-select) value, and whether output-enable needs to be
// on, to control the real LED -- then uses what it found to blink it,
// forever, on its own. Only bringing up USB is fixed; everything about pin
// configuration is learned here, on the chip, at boot. USB is only used to
// print a log of what it's doing -- nothing reads that log to control
// anything back.
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/io_bank0.h"

#define LED 25
#define PIN_LO 20
#define PIN_HI 29
#define NPIN (PIN_HI - PIN_LO + 1)   // 10
#define NFUNC 9                     // real RP2040 function numbers: 0..8
#define NOE 2
#define NVAL 2
#define NACT (NPIN * NFUNC * NOE * NVAL)   // 360

typedef struct { int pin, fn, oe, val; } action_t;

static action_t ACTIONS[NACT];
static float Qon[NACT], Qoff[NACT];
static int   Non[NACT], Noff[NACT];
static uint32_t rng;
static uint32_t rnd(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }

static void reset_all(void){
    for(int p=PIN_LO;p<=PIN_HI;p++){
        io_bank0_hw->io[p].ctrl = 0x1f;   // unmux (NULL function)
        sio_hw->gpio_oe_clr = 1u << p;    // input, not driving
        sio_hw->gpio_clr = 1u << p;       // value = 0
    }
}
static int led_level(void){ return (sio_hw->gpio_in >> LED) & 1; }

static void apply(action_t a){
    io_bank0_hw->io[a.pin].ctrl = a.fn & 0x1f;
    if(a.oe)  sio_hw->gpio_oe_set = 1u << a.pin; else sio_hw->gpio_oe_clr = 1u << a.pin;
    if(a.val) sio_hw->gpio_set   = 1u << a.pin; else sio_hw->gpio_clr   = 1u << a.pin;
}

int main(void){
    stdio_init_all();
    sleep_ms(1500);
    reset_all();
    rng = (uint32_t)time_us_64() | 1u;

    int idx = 0;
    for(int p=PIN_LO;p<=PIN_HI;p++)
        for(int f=0;f<NFUNC;f++)
            for(int oe=0;oe<NOE;oe++)
                for(int v=0;v<NVAL;v++)
                    ACTIONS[idx++] = (action_t){p, f, oe, v};

    for(int i=0;i<NACT;i++){ Qon[i]=0; Qoff[i]=0; Non[i]=0; Noff[i]=0; }

    printf("=== learning the whole setup + on/off, entirely on-chip ===\n");
    const int TRIALS = 6000;
    for(int t=0;t<TRIALS;t++){
        int goal_on = rnd() & 1;
        reset_all();

        int ai;
        float eps = (float)(rnd() & 0xFFFF) / 65536.0f;
        if(eps < 0.3f){
            ai = rnd() % NACT;
        } else {
            float *Q = goal_on ? Qon : Qoff;
            ai = 0;
            for(int i=1;i<NACT;i++) if(Q[i] > Q[ai]) ai = i;
        }

        apply(ACTIONS[ai]);
        int led = led_level();
        float reward = (led == goal_on) ? 1.0f : 0.0f;
        if(goal_on){ Non[ai]++;  Qon[ai]  += (reward - Qon[ai])  / Non[ai]; }
        else       { Noff[ai]++; Qoff[ai] += (reward - Qoff[ai]) / Noff[ai]; }

        if(t % 1000 == 0) printf("  trial %d...\n", t);
    }

    int best_on = 0, best_off = 0;
    for(int i=1;i<NACT;i++){
        if(Qon[i]  > Qon[best_on])   best_on  = i;
        if(Qoff[i] > Qoff[best_off]) best_off = i;
    }
    action_t A = ACTIONS[best_on], B = ACTIONS[best_off];
    printf("\nlearned ON:  pin=%d funcsel=%d oe=%d val=%d (confidence %.2f)\n",
           A.pin, A.fn, A.oe, A.val, Qon[best_on]);
    printf("learned OFF: pin=%d funcsel=%d oe=%d val=%d (confidence %.2f)\n",
           B.pin, B.fn, B.oe, B.val, Qoff[best_off]);

    printf("\n=== now blinking using ONLY what it learned, forever ===\n");
    while(true){
        reset_all(); apply(A); printf("ON  -> led=%d\n", led_level()); sleep_ms(400);
        reset_all(); apply(B); printf("OFF -> led=%d\n", led_level()); sleep_ms(400);
    }
}
