// ternary.c — XOR as a TERNARY network: weights only +1 / -1, integer thresholds, NO floating point.
// Same light-switch behavior as the float version, but the whole engine is add/subtract/compare.
#include <stdio.h>
#include "pico/stdlib.h"

int main(void) {
    stdio_init_all();
    const uint LED = PICO_DEFAULT_LED_PIN;
    gpio_init(LED); gpio_set_dir(LED, GPIO_OUT);
    sleep_ms(1500);

    printf("\n=== TERNARY XOR on the Pico (no floating point) ===\n");
    printf("'a' = toggle A   's' = toggle B   'd' = auto-demo\n");
    printf("all weights are +1; activation is just 'is the sum >= N?'\n\n");

    int A = 0, B = 0, prevA = -1, prevB = -1;
    bool manual = false;
    absolute_time_t next = make_timeout_time_ms(1500);
    int combo = 0;

    while (true) {
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == 'a' || c == 'A') { A ^= 1; manual = true; }
            else if (c == 's' || c == 'S') { B ^= 1; manual = true; }
            else if (c == 'd' || c == 'D') { manual = false; }
        }
        if (!manual && absolute_time_diff_us(get_absolute_time(), next) < 0) {
            combo = (combo + 1) & 3;
            A = (combo >> 1) & 1; B = combo & 1;
            next = make_timeout_time_ms(1500);
        }

        // ---- the ternary network: pure integer math, no sigmoid, no multiply-by-decimals ----
        int sum   = (+1) * A + (+1) * B;            // hidden weights: +1, +1  (just add the buttons)
        int h_or  = (sum >= 1);                     // "at least one" detector   (OR)
        int h_and = (sum >= 2);                     // "both" detector           (AND)
        int led   = ((+1) * h_or + (-1) * h_and) >= 1;  // OR and NOT AND  =  XOR
        gpio_put(LED, led);

        if (A != prevA || B != prevB) {
            printf("A=%d B=%d  ->  sum=%d  (or=%d and=%d)  ->  LED %s   %s\n",
                   A, B, sum, h_or, h_and, led ? "ON " : "off",
                   manual ? "[keyboard]" : "[demo]");
            prevA = A; prevB = B;
        }
        sleep_ms(20);
    }
}
