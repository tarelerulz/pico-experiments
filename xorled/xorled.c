// xorled.c — the trained XOR neural net running ON the Pico.
// Two "buttons" come from your keyboard over USB serial ('a' and 's').
// The onboard LED is the output. The 9 learned weights (weights.h) decide it.
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "weights.h"   // W1[2][2], b1[2], W2[2], b2  (from train_xor.py)

static float sigmoidf(float z) { return 1.0f / (1.0f + expf(-z)); }

// forward pass: the exact same math as the story model, with just 9 numbers
static float xor_net(int a, int b) {
    float h[2];
    for (int j = 0; j < 2; j++)
        h[j] = sigmoidf(a * W1[0][j] + b * W1[1][j] + b1[j]);   // hidden layer
    return sigmoidf(h[0] * W2[0] + h[1] * W2[1] + b2);          // output neuron
}

int main(void) {
    stdio_init_all();
    const uint LED = PICO_DEFAULT_LED_PIN;   // onboard LED (GP25)
    gpio_init(LED); gpio_set_dir(LED, GPIO_OUT);
    sleep_ms(1500);

    printf("\n=== XOR neural net on the Pico ===\n");
    printf("Keys over USB serial:  'a' = toggle button A   's' = toggle button B\n");
    printf("                       'd' = auto-demo mode\n");
    printf("The onboard LED is the model's output.\n\n");

    int A = 0, B = 0, prevA = -1, prevB = -1;
    bool manual = false;
    absolute_time_t next = make_timeout_time_ms(1500);
    int combo = 0;

    while (true) {
        int c = getchar_timeout_us(0);            // non-blocking: a key waiting?
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == 'a' || c == 'A') { A ^= 1; manual = true; }
            else if (c == 's' || c == 'S') { B ^= 1; manual = true; }
            else if (c == 'd' || c == 'D') { manual = false; }
        }

        // auto-demo: walk the 4 combos so the LED shows XOR by itself
        if (!manual && absolute_time_diff_us(get_absolute_time(), next) < 0) {
            combo = (combo + 1) & 3;
            A = (combo >> 1) & 1; B = combo & 1;
            next = make_timeout_time_ms(1500);
        }

        float out = xor_net(A, B);
        int led = out > 0.5f;
        gpio_put(LED, led);

        if (A != prevA || B != prevB) {           // print only when something changes
            printf("A=%d  B=%d   ->   net=%.3f   ->   LED %s%s\n",
                   A, B, out, led ? "ON " : "off",
                   manual ? "   [keyboard]" : "   [demo]");
            prevA = A; prevB = B;
        }
        sleep_ms(20);
    }
}
