#ifndef SI5351_H
#define SI5351_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

#define SI5351_ADDR                     0x60
#define SI5351_XTAL_HZ                  24576000u
#define SI5351_PLL_VCO_MIN_HZ           600000000u
#define SI5351_PLL_VCO_MAX_HZ           900000000u

#define SI5351_REG_DEVICE_STATUS        0
#define SI5351_REG_OUTPUT_ENABLE_CTRL   3
#define SI5351_REG_CLK0_CTRL            16
#define SI5351_REG_CLK1_CTRL            17
#define SI5351_REG_CLK2_CTRL            18
#define SI5351_REG_PLLA_BASE            26
#define SI5351_REG_PLLB_BASE            34
#define SI5351_REG_MS0_BASE             42
#define SI5351_REG_MS1_BASE             50
#define SI5351_REG_MS2_BASE             58
#define SI5351_REG_CLK0_PHOFF           165
#define SI5351_REG_CLK1_PHOFF           166
#define SI5351_REG_CLK2_PHOFF           167
#define SI5351_REG_PLL_RESET            177
#define SI5351_REG_XTAL_CL              183

#define SI5351_XTAL_LOAD_10PF           0xD2

typedef enum {
	SI5351_CLK0 = 0,
	SI5351_CLK1 = 1,
	SI5351_CLK2 = 2,
	SI5351_CLK_NONE = 255
} si5351_clock_t;

typedef enum {
    SI5351_INTEGER_CLOSEST = 0,
    SI5351_INTEGER_APPROX = 0,
    SI5351_INTEGER_EXACT = 1
} si5351_integer_mode_t;

bool si5351_init(i2c_inst_t *i2c);
void si5351_write_reg(i2c_inst_t *i2c, uint8_t reg, uint8_t val);
uint8_t si5351_read_reg(i2c_inst_t *i2c, uint8_t reg);

// Returns the actual programmed frequency in Hz, or 0 on error.
uint32_t si5351_set_frequency(
	i2c_inst_t *i2c,
	si5351_clock_t clock,
	uint32_t target_hz,
	si5351_integer_mode_t mode,
	si5351_clock_t quadrature_partner
);

// Debug probe/watch variables for SWD inspection.
extern volatile uint32_t si5351_debug_pll_n;
extern volatile uint32_t si5351_debug_ms_m;
extern volatile uint32_t si5351_debug_actual_hz;
extern volatile uint8_t si5351_debug_pll_bytes[8];
extern volatile uint8_t si5351_debug_ms_bytes[8];
extern volatile uint8_t si5351_debug_clk_ctrl;
extern volatile uint8_t si5351_debug_phase_offset;
extern volatile uint8_t si5351_debug_clock;
extern volatile uint8_t si5351_debug_quadrature_partner;
extern volatile bool si5351_debug_pair_mode;
extern volatile bool si5351_debug_use_pllb;
extern volatile bool si5351_debug_programmed;

#endif