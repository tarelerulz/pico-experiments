// learnled.c — the Pico LEARNS on-chip, then you test it live.
// It trains a tiny 2->2->1 neural net (9 weights) on the XOR truth table right on
// the RP2040, prints the weights it discovered over USB, then runs live: press
// 'a'/'s' and the LEARNED model drives the onboard LED.
//   XOR rule it learns:  exactly one of a/s on -> LED ON ;  both on -> LED OFF.
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"

#define LED 25

// the 4 examples the Pico learns from (a, b) -> desired LED
static const float X[4][2] = {{0,0},{0,1},{1,0},{1,1}};
static const float Y[4]    = { 0,   1,   1,   0 };   // XOR

// the model: 9 weights
static float W1[2][2], b1[2], W2[2], b2;

static float sigmoidf(float z){ return 1.0f/(1.0f+expf(-z)); }

// tiny PRNG for random init
static uint32_t rng;
static float frand(){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return ((rng>>8)&0xffffff)/16777216.0f; }
static float rand_w(){ return frand()*2.0f - 1.0f; }   // [-1,1]

static float forward(float a, float b, float h_out[2]){
    for(int j=0;j<2;j++) h_out[j]=sigmoidf(a*W1[0][j]+b*W1[1][j]+b1[j]);
    return sigmoidf(h_out[0]*W2[0]+h_out[1]*W2[1]+b2);
}

// train once from random init; return final error
static float train(int epochs, float lr){
    for(int i=0;i<2;i++){ for(int j=0;j<2;j++) W1[i][j]=rand_w(); }
    b1[0]=rand_w(); b1[1]=rand_w(); W2[0]=rand_w(); W2[1]=rand_w(); b2=rand_w();
    float loss=1;
    for(int e=0;e<epochs;e++){
        loss=0;
        for(int n=0;n<4;n++){
            float a=X[n][0], b=X[n][1], y=Y[n], h[2];
            float out=forward(a,b,h);
            loss += (out-y)*(out-y);
            float d_out=(out-y)*out*(1-out);
            float dW2[2], d_h[2];
            for(int j=0;j<2;j++){ dW2[j]=h[j]*d_out; d_h[j]=d_out*W2[j]*h[j]*(1-h[j]); }
            for(int j=0;j<2;j++){ W2[j]-=lr*dW2[j]; }
            b2-=lr*d_out;
            for(int i=0;i<2;i++) for(int j=0;j<2;j++) W1[i][j]-=lr*X[n][i]*d_h[j];
            for(int j=0;j<2;j++) b1[j]-=lr*d_h[j];
        }
        if(loss<0.001f) break;
    }
    return loss;
}

int main(void){
    stdio_init_all();
    gpio_init(LED); gpio_set_dir(LED, GPIO_OUT);
    sleep_ms(1500);
    rng = (uint32_t)time_us_64() | 1u;

    printf("\n=== Pico is LEARNING (XOR) on-chip... ===\n");
    float loss=1; int tries=0;
    while(loss>0.01f && tries<50){ loss=train(6000, 0.5f); tries++; }   // retry until it learns
    printf("learned after %d attempt(s), error=%.4f\n\n", tries, loss);

    printf("the 9 weights it discovered:\n");
    printf("  W1 = [[%+.2f %+.2f] [%+.2f %+.2f]]  b1 = [%+.2f %+.2f]\n",
           W1[0][0],W1[0][1],W1[1][0],W1[1][1], b1[0], b1[1]);
    printf("  W2 = [%+.2f %+.2f]  b2 = %+.2f\n\n", W2[0],W2[1], b2);

    printf("did it understand? (its answer on all 4 cases)\n");
    for(int n=0;n<4;n++){ float h[2]; float o=forward(X[n][0],X[n][1],h);
        printf("  a=%d s=%d -> %.2f -> LED %s  (want %d)\n",
               (int)X[n][0],(int)X[n][1], o, o>0.5f?"ON ":"off", (int)Y[n]); }

    printf("\nNow TEST it: press 'a' and 's'.  (both pressed -> LED off)\n\n");

    int A=0,B=0,pa=-1,pb=-1;
    while(true){
        int c=getchar_timeout_us(0);
        if(c!=PICO_ERROR_TIMEOUT){
            if(c=='a'||c=='A') A^=1;
            else if(c=='s'||c=='S') B^=1;
        }
        float h[2]; float o=forward((float)A,(float)B,h);
        int led = o>0.5f; gpio_put(LED, led);
        if(A!=pa||B!=pb){ printf("a=%d s=%d -> model=%.2f -> LED %s\n",A,B,o,led?"ON":"off"); pa=A; pb=B; }
        sleep_ms(20);
    }
}
