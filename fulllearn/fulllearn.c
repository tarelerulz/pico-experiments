// fulllearn.c -- everything combined, nothing hardcoded except USB bring-up:
//   PHASE 1: same search as setuplearn.c -- discovers which pin, which mux
//            (function-select), and whether output-enable needs to be on,
//            to control the real LED, via trial and reward.
//   PHASE 2: YOU press whatever two keys you want -- it asks "press the key
//            for ON", then "press the key for OFF". No key is hardcoded in
//            the source; it's whatever bytes you actually send it.
//   PHASE 3: trains a tiny model (8 weights, one per bit of the key's byte)
//            to recognize those two specific keys again later.
//   PHASE 4: live forever -- reads whatever key you press, runs it through
//            the trained decoder to get on/off, then drives the LED using
//            what Phase 1 learned. Any other key is just ignored.
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/io_bank0.h"

#define LED 25
#define PIN_LO 20
#define PIN_HI 29
#define NPIN (PIN_HI - PIN_LO + 1)
#define NFUNC 9
#define NOE 2
#define NVAL 2
#define NACT (NPIN * NFUNC * NOE * NVAL)

typedef struct { int pin, fn, oe, val; } action_t;

static action_t ACTIONS[NACT];
static float Qon[NACT], Qoff[NACT];
static int   Non[NACT], Noff[NACT];
static uint32_t rng;
static uint32_t rnd(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }

static void reset_all(void){
    for(int p=PIN_LO;p<=PIN_HI;p++){
        io_bank0_hw->io[p].ctrl = 0x1f;
        sio_hw->gpio_oe_clr = 1u << p;
        sio_hw->gpio_clr = 1u << p;
    }
}
static int led_level(void){ return (sio_hw->gpio_in >> LED) & 1; }
static void apply(action_t a){
    io_bank0_hw->io[a.pin].ctrl = a.fn & 0x1f;
    if(a.oe)  sio_hw->gpio_oe_set = 1u << a.pin; else sio_hw->gpio_oe_clr = 1u << a.pin;
    if(a.val) sio_hw->gpio_set   = 1u << a.pin; else sio_hw->gpio_clr   = 1u << a.pin;
}

static int wait_key(void){
    int c;
    do { c = getchar_timeout_us(100000); } while(c < 0);
    return c;
}

static float sigmoidf(float z){ return 1.0f / (1.0f + expf(-z)); }
static void bits8(int byte, float x[8]){ for(int i=0;i<8;i++) x[i] = (float)((byte>>i)&1); }

int main(void){
    stdio_init_all();
    sleep_ms(1500);
    reset_all();
    rng = (uint32_t)time_us_64() | 1u;

    // ---- PHASE 1: learn which pin/mux/OE/value control the real LED ----
    int idx = 0;
    for(int p=PIN_LO;p<=PIN_HI;p++)
        for(int f=0;f<NFUNC;f++)
            for(int oe=0;oe<NOE;oe++)
                for(int v=0;v<NVAL;v++)
                    ACTIONS[idx++] = (action_t){p, f, oe, v};
    for(int i=0;i<NACT;i++){ Qon[i]=0; Qoff[i]=0; Non[i]=0; Noff[i]=0; }

    printf("=== PHASE 1: learning the whole setup + on/off ===\n");
    const int TRIALS = 6000;
    for(int t=0;t<TRIALS;t++){
        int goal_on = rnd() & 1;
        reset_all();
        int ai;
        if(((float)(rnd() & 0xFFFF) / 65536.0f) < 0.3f){
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
    }
    int best_on = 0, best_off = 0;
    for(int i=1;i<NACT;i++){
        if(Qon[i]  > Qon[best_on])   best_on  = i;
        if(Qoff[i] > Qoff[best_off]) best_off = i;
    }
    action_t ON = ACTIONS[best_on], OFF = ACTIONS[best_off];
    printf("  learned ON:  pin=%d funcsel=%d oe=%d val=%d\n", ON.pin, ON.fn, ON.oe, ON.val);
    printf("  learned OFF: pin=%d funcsel=%d oe=%d val=%d\n", OFF.pin, OFF.fn, OFF.oe, OFF.val);

    // ---- PHASE 2: you pick the two keys, live, right now ----
    printf("\n=== PHASE 2: press the key that should mean ON ===\n");
    int key_on = wait_key();
    printf("  got byte %d ('%c')\n", key_on, (key_on>=32&&key_on<127)?key_on:'?');
    printf("=== press the key that should mean OFF ===\n");
    int key_off = wait_key();
    printf("  got byte %d ('%c')\n", key_off, (key_off>=32&&key_off<127)?key_off:'?');

    // ---- PHASE 3: train an 8-weight decoder on just those two keys ----
    printf("\n=== PHASE 3: learning to recognize those two keys ===\n");
    float w[8] = {0,0,0,0,0,0,0,0}, b = 0;
    int cb[2] = {key_on, key_off}, cy[2] = {1, 0};
    for(int e=0;e<3000;e++) for(int n=0;n<2;n++){
        float x[8]; bits8(cb[n], x);
        float z=b; for(int i=0;i<8;i++) z += w[i]*x[i];
        float o = sigmoidf(z), err = o - (float)cy[n];
        for(int i=0;i<8;i++) w[i] -= 0.3f*err*x[i];
        b -= 0.3f*err;
    }

    // ---- PHASE 4: live -- any key, decoded and driven by what was learned ----
    printf("\n=== PHASE 4: press your ON/OFF keys (or anything else) ===\n\n");
    while(true){
        int c = getchar_timeout_us(0);
        if(c >= 0){
            float x[8]; bits8(c, x);
            float z=b; for(int i=0;i<8;i++) z += w[i]*x[i];
            float conf = sigmoidf(z);
            if(conf > 0.5f){
                reset_all(); apply(ON);
                printf("byte %3d ('%c') -> ON  (confidence %.2f) -> led=%d\n",
                       c, (c>=32&&c<127)?c:'?', conf, led_level());
            } else if(conf < 0.5f){
                reset_all(); apply(OFF);
                printf("byte %3d ('%c') -> OFF (confidence %.2f) -> led=%d\n",
                       c, (c>=32&&c<127)?c:'?', 1.0f-conf, led_level());
            }
        }
        sleep_ms(10);
    }
}
