// setupdrive.c -- like regdrive, but goes further: the Pico does NOT configure
// any candidate pin ahead of time. Function-select (mux) and output-enable
// are part of what gets learned now too, not just which pin/value. Only
// stdio (USB) is fixed -- that has to work before any learning can be
// reported at all.
//
// Protocol (text lines over USB):
//   "R"                            -> wipe every candidate pin back to
//                                      unconfigured (blank slate); replies "led=x"
//   "A <pin> <funcsel> <oe> <val>" -> mux `pin` to function `funcsel`, set its
//                                      output-enable to `oe`, set its output
//                                      value to `val` -- all in one action;
//                                      replies "led=x"
// "led=x" is always the REAL electrical read-back at pin 25 (the actual
// onboard LED), not just whatever was last written. It only matches what you
// asked for if you picked the right pin AND the right funcsel AND OE.
// Candidate pins: 20..29 (this range includes 25, the real LED).
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/io_bank0.h"

#define LED 25
#define PIN_LO 20
#define PIN_HI 29
#define FUNCSEL_NULL 0x1fu

static void reset_all(void){
    for(int p=PIN_LO;p<=PIN_HI;p++){
        io_bank0_hw->io[p].ctrl = FUNCSEL_NULL;
        sio_hw->gpio_oe_clr = 1u << p;
        sio_hw->gpio_clr = 1u << p;
    }
}

static int led_level(void){ return (sio_hw->gpio_in >> LED) & 1; }

int main(void){
    stdio_init_all();
    sleep_ms(1500);
    reset_all();
    printf("setupdrive ready\n");

    char line[48]; int li=0;
    while(true){
        int c=getchar_timeout_us(2000);
        if(c==PICO_ERROR_TIMEOUT || c<0) continue;
        if(c=='\r') continue;
        if(c=='\n'){
            line[li]=0;
            if(li>0){
                if(line[0]=='R'){
                    reset_all();
                } else if(line[0]=='A'){
                    int pin=-1, fn=0, oe=0, val=0;
                    if(sscanf(line+1,"%d %d %d %d",&pin,&fn,&oe,&val)==4
                       && pin>=PIN_LO && pin<=PIN_HI){
                        io_bank0_hw->io[pin].ctrl = fn & 0x1f;
                        if(oe & 1) sio_hw->gpio_oe_set = 1u<<pin; else sio_hw->gpio_oe_clr = 1u<<pin;
                        if(val & 1) sio_hw->gpio_set = 1u<<pin; else sio_hw->gpio_clr = 1u<<pin;
                    }
                }
                printf("led=%d\n", led_level());
            }
            li=0;
        } else if(li < (int)sizeof(line)-1){
            line[li++]=(char)c;
        }
    }
}
