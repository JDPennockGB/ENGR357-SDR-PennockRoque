#include "lcd_driver.h"
#include "hardware/gpio.h"

#define RS_PIN 14
#define E_PIN  15

// Pins DB0 to DB7
static const uint DB_PINS[] = {3, 4, 5, 6, 7, 8, 9, 10};

static void lcd_pulse_e() {
    gpio_put(E_PIN, 1);
    sleep_us(1); // Enable pulse width >= 300ns
    gpio_put(E_PIN, 0);
    sleep_us(100);
}

static void lcd_write_bus(uint8_t val) {
    for (int i = 0; i < 8; i++) {
        gpio_put(DB_PINS[i], (val >> i) & 1);
    }
}

void lcd_command(uint8_t cmd) {
    gpio_put(RS_PIN, 0); // Command Mode
    lcd_write_bus(cmd);
    lcd_pulse_e();
}

void lcd_init() {
    gpio_init(RS_PIN); gpio_set_dir(RS_PIN, GPIO_OUT);
    gpio_init(E_PIN);  gpio_set_dir(E_PIN, GPIO_OUT);
    for(int i=0; i<8; i++) {
        gpio_init(DB_PINS[i]);
        gpio_set_dir(DB_PINS[i], GPIO_OUT);
    }

    sleep_ms(100);       // Wait >40msec [cite: 259]
    lcd_command(0x30);   // Wake up
    sleep_ms(30);
    lcd_command(0x30);   // Wake up #2
    sleep_ms(10);
    lcd_command(0x30);   // Wake up #3
    
    lcd_command(0x38);   // Function set: 8-bit, 1-line [cite: 259]
    lcd_command(0x0C);   // Display ON, Cursor OFF
    lcd_clear();
}

void lcd_clear() {
    lcd_command(0x01);
    sleep_ms(2);
}

void lcd_print(const char* str) {
    gpio_put(RS_PIN, 1); // Data Mode
    while (*str && (str - str < 8)) { // 8 chars max
        lcd_write_bus(*str++);
        lcd_pulse_e();
    }
}