// button.c — hold the Pico's BOOTSEL button, the onboard LED turns on.
// Pure digital I/O: read one pin (the button), drive another (the LED). No "model".
#include "pico/stdlib.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"

// The BOOTSEL button shares the flash chip-select pin, so to read it we must briefly
// stop driving that pin, sample it, then restore it — with flash interrupts paused.
// (This function must live in RAM, hence __no_inline_not_in_flash_func.)
bool __no_inline_not_in_flash_func(get_bootsel_button)(void) {
    const uint CS_PIN_INDEX = 1;
    uint32_t flags = save_and_disable_interrupts();

    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    for (volatile int i = 0; i < 1000; ++i);           // let the pin settle

    // BOOTSEL reads LOW when pressed, so invert to get "pressed = true"
    bool pressed = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);
    return pressed;
}

int main(void) {
    const uint LED = PICO_DEFAULT_LED_PIN;   // onboard LED = GP25 on the original Pico
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    while (true) {
        gpio_put(LED, get_bootsel_button());  // LED on WHILE the button is held
        sleep_ms(10);
    }
}
