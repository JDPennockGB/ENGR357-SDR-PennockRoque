#ifndef SI5351_H
#define SI5351_H

#include "hardware/i2c.h"
#include <stdint.h>
#include <stdbool.h>

#define SI5351_ADDR 0x60

// Initialize the Si5351
bool si5351_init(i2c_inst_t *i2c);

// Program the Si5351a directly using parameters calculated by the host (Quisk).
void si5351_set_freq_regs(i2c_inst_t *i2c, uint32_t N, uint32_t P1, uint32_t P2, uint32_t P3, bool direct_mode);

#endif
