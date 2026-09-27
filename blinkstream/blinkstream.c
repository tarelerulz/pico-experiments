// blinkstream.c — blink the onboard LED (GPIO 25) AND stream the GPIO register
// over USB each step. The stream is the dataset: a 32-bit value where ONE bit
// (bit 25) toggles with the LED. A model on the Pi can learn "bit 25 = the LED".
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/structs/sio.h"   // gives direct access to the GPIO registers

#define LED 25   // onboard LED on the original Pico

int main(void) {
    stdio_init_all();               // USB serial
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    sleep_ms(1500);                 // let USB connect
    printf("# blinkstream: columns = out_reg(hex)  in_reg(hex)  bit25\n");

    bool on = false;
    while (true) {
        on = !on;
        gpio_put(LED, on);          // the "command" — flip the LED

        // read the actual hardware registers straight from the SIO block
        uint32_t out_reg = sio_hw->gpio_out;   // what pins are commanded on
        uint32_t in_reg  = sio_hw->gpio_in;    // actual level read back at the pins
        int bit25 = (out_reg >> LED) & 1;      // the one bit that IS the LED

        // stream one line per step: the whole register, plus the isolated bit
        printf("%08x %08x %d\n", out_reg, in_reg, bit25);

        sleep_ms(500);              // blink rate / sample rate
    }
}
