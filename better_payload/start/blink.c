#include "rpi.h"

void notmain(void) {
    int led = 20;
    gpio_set_output(led);
    for(int i = 0; i < 10; i++) {
        gpio_set_on(led);
        delay_cycles(1000000);
        gpio_set_off(led);
        delay_cycles(1000000);
    }

    // last iter = leave the LED on so know to power-cycle
    gpio_set_on(led);
}