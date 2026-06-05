#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include "pico/stdlib.h"

// Initialize the LCD with your specific pinout
void lcd_init();

// Clear the screen
void lcd_clear();

// Print a string to the LCD
void lcd_print(const char* str);

#endif