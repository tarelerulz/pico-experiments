// regdrive.c — the Pico is a "dumb" register box. It executes register writes it's
// told over USB and reports back the LED state. It does NOT decide anything.
// The LEARNER (on the Pi) is given a goal and must discover, from the LED feedback,
// which register write controls the light.
//
// Protocol (text lines over USB):
//   "R"        -> randomize the LED to a fresh 0/1 (start of a trial); replies "led=x"
//   "<b> <v>"  -> write value v to GPIO bit b (b in 20..29); replies "led=x"
// Only GPIO 25 has the real onboard LED, so only writing bit 25 changes the light —
// but the learner isn't told that. Bits 20..29 are the candidate "levers".
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/io_bank0.h"
#include "hardware/structs/resets.h"

#define LED 25

static uint32_t rng;
static int rnd_bit(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return (rng>>9)&1; }

int main(void){
    stdio_init_all();
    for(int p=20;p<=29;p++){ gpio_init(p); gpio_set_dir(p, GPIO_OUT); gpio_put(p,0); }
    sleep_ms(1500);
    rng=(uint32_t)time_us_64()|1u;

    // One-shot snapshot of the setup this boot did, before any learning starts.
    // reset_done: bit 5 (IO_BANK0) should read 1 -> that block is out of reset.
    // gpio_oe: which pins got configured as outputs (bits 20..29 should be set).
    // funcsel: the mux value IO_BANK0 picked for each candidate pin (5 = SIO).
    // This line only ever prints once per boot, since setup only runs once per
    // boot -- capturing it as training data means power-cycling for each example.
    printf("setup reset_done=%08x gpio_oe=%08x funcsel20-29=", resets_hw->reset_done, sio_hw->gpio_oe);
    for(int p=20;p<=29;p++) printf("%d", io_bank0_hw->io[p].ctrl & 0x1f);
    printf("\n");

    printf("regdrive ready\n");

    char line[32]; int li=0;
    while(true){
        int c=getchar_timeout_us(2000);
        if(c==PICO_ERROR_TIMEOUT || c<0) continue;
        if(c=='\r') continue;
        if(c=='\n'){
            line[li]=0;
            if(li>0){
                if(line[0]=='R'){
                    gpio_put(LED, rnd_bit());          // randomize the light
                } else {
                    int bit=-1, val=0;
                    if(sscanf(line,"%d %d",&bit,&val)==2 && bit>=20 && bit<=29){
                        gpio_put(bit, val&1);          // execute the register write
                    }
                }
            }
            int led = (sio_hw->gpio_out >> LED) & 1;   // report the resulting LED state
            printf("led=%d\n", led);
            li=0;
        } else if(li < (int)sizeof(line)-1){
            line[li++]=(char)c;
        }
    }
}
