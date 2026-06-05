#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "si5351.h"

#define I2C_PORT i2c0
#define SDA_PIN 12
#define SCL_PIN 13

int main() {
    // Initialize hardware
    stdio_init_all();
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    // Initialize the Si5351
    if (si5351_init(I2C_PORT)) {
        printf("Si5351 Initialized.\n");
    } else {
        printf("Si5351 Init Failed.\n");
        while(1); // Stop here if init fails
    }

    // Set output to 4 MHz (resulting in 1 MHz at Johnson Counter)
    uint32_t target = 4000000;
    uint32_t actual = si5351_set_frequency(
        I2C_PORT, 
        SI5351_CLK0, 
        target, 
        SI5351_INTEGER_CLOSEST, 
        SI5351_CLK_NONE
    );

    if (actual > 0) {
        printf("Si5351 running at %u Hz\n", actual);
    } else {
        printf("Failed to set frequency\n");
    }

    // Keep the core alive
    while (1) {
        sleep_ms(1000);
    }
    return 0;
}