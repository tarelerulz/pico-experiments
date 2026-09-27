// trainedled.c -- uses what setuplearn.c already discovered (pin 25, mux
// function 5 = SIO, output-enable on) instead of re-learning it at every
// boot. That trial-and-reward search only had to happen once; this is the
// trained result, applied directly. Then it takes single-key commands over
// USB serial: 'a' turns the LED on, 's' turns it off.
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/io_bank0.h"

// -- trained knowledge, discovered once by setuplearn.c, not re-learned here --
#define LED_PIN   25
#define LED_FUNC  5
#define LED_OE    1

int main(void){
    stdio_init_all();
    sleep_ms(1500);

    // apply the already-trained setup once -- no search, no trial-and-error
    io_bank0_hw->io[LED_PIN].ctrl = LED_FUNC;
    if (LED_OE) sio_hw->gpio_oe_set = 1u << LED_PIN;
    sio_hw->gpio_clr = 1u << LED_PIN;   // start off

    printf("trainedled ready -- press 'a' to turn the LED on, 's' to turn it off\n");

    while (true) {
        int c = getchar_timeout_us(100000);
        if (c == 'a') {
            sio_hw->gpio_set = 1u << LED_PIN;
            printf("LED on\n");
        } else if (c == 's') {
            sio_hw->gpio_clr = 1u << LED_PIN;
            printf("LED off\n");
        }
    }
}
