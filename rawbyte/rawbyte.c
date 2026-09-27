// rawbyte.c — the model reads the RAW USB BYTE (all 8 bits) and learns, by itself,
// which byte means "LED on" and which means "LED off". NO if-statement decoding:
// the byte's bits go straight into the model. It trains on-chip on two examples:
//     'a' (byte 97)  -> LED ON        's' (byte 115) -> LED OFF
// then reads live bytes and lets the LEARNED model decide.
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "pico/stdlib.h"

#define LED 25

// training examples: (byte, desired LED)
static const int   TB[2] = { 'a', 's' };   // 97, 115
static const float TY[2] = {  1,   0  };    // on, off

// model: a single neuron over the 8 bits of the byte (8 weights + bias)
static float w[8], b;

static float sigmoidf(float z){ return 1.0f/(1.0f+expf(-z)); }
static uint32_t rng;
static float frand(){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return ((rng>>8)&0xffffff)/16777216.0f; }

static void bits_of(int byte, float x[8]){ for(int i=0;i<8;i++) x[i]=(float)((byte>>i)&1); }
static float forward(const float x[8]){ float z=b; for(int i=0;i<8;i++) z+=w[i]*x[i]; return sigmoidf(z); }

static void train(int epochs, float lr){
    for(int i=0;i<8;i++) w[i]=frand()*2-1; b=frand()*2-1;
    for(int e=0;e<epochs;e++){
        for(int n=0;n<2;n++){
            float x[8]; bits_of(TB[n], x);
            float o=forward(x), err=(o-TY[n]);
            for(int i=0;i<8;i++) w[i]-=lr*err*x[i];
            b-=lr*err;
        }
    }
}

int main(void){
    stdio_init_all();
    gpio_init(LED); gpio_set_dir(LED, GPIO_OUT);
    sleep_ms(1500);
    rng=(uint32_t)time_us_64()|1u;

    printf("\n=== Pico learning to read RAW BYTES ===\n");
    printf("training:  'a'(97) -> ON    's'(115) -> OFF   (only the raw bits, no decoding)\n\n");
    train(4000, 0.3f);

    printf("weights it put on each bit of the byte:\n");
    for(int i=0;i<8;i++){
        int ba=('a'>>i)&1, bs=('s'>>i)&1;
        const char* note = (ba!=bs) ? "  <-- this bit differs between a and s" : "";
        printf("  bit %d (value %3d):  w=%+6.2f%s\n", i, 1<<i, w[i], note);
    }
    printf("\ncheck on the two it trained on:\n");
    for(int n=0;n<2;n++){ float x[8]; bits_of(TB[n],x); float o=forward(x);
        printf("  byte %d ('%c') -> %.2f -> LED %s\n", TB[n], TB[n], o, o>0.5f?"ON":"off"); }

    printf("\nNow press keys. 'a'=on, 's'=off  — and try OTHER keys to see what it does\n");
    printf("with bytes it never trained on (its guess from the raw bits).\n\n");

    int last=-1;
    while(true){
        int c=getchar_timeout_us(0);
        if(c!=PICO_ERROR_TIMEOUT && c>=0){
            float x[8]; bits_of(c, x);
            float o=forward(x);
            int led=o>0.5f; gpio_put(LED, led);
            if(c!=last){
                printf("byte %3d ('%c')  bits=%d%d%d%d%d%d%d%d -> model=%.2f -> LED %s\n",
                    c, isprint(c)?c:'?',
                    (c>>7)&1,(c>>6)&1,(c>>5)&1,(c>>4)&1,(c>>3)&1,(c>>2)&1,(c>>1)&1,c&1,
                    o, led?"ON":"off");
                last=c;
            }
        }
        sleep_ms(10);
    }
}
