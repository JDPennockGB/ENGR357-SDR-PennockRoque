#include "si5351.h"
#include <string.h>

// We use 10000us (10ms) timeouts so the Pico NEVER freezes if the I2C wires are loose.
#define I2C_TIMEOUT 10000

bool si5351_init(i2c_inst_t *i2c) {
    uint8_t tx[2];
    tx[0] = 3; tx[1] = 0xFF; // Disable outputs
    i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 2, false, I2C_TIMEOUT);
    
    tx[0] = 183; tx[1] = 0xD2; // 10pF Crystal Load
    i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 2, false, I2C_TIMEOUT);
    return true;
}

void si5351_set_freq_regs(i2c_inst_t *i2c, uint32_t N, uint32_t P1, uint32_t P2, uint32_t P3, bool direct_mode) {
    uint8_t tx[9];
    
    // 1. Program MS0 and MS1 to integer divider N
    uint32_t ms_p1 = 128 * N - 512;
    tx[0] = 42; // MS0 Base
    tx[1] = 0; tx[2] = 1;
    tx[3] = (ms_p1 >> 16) & 0x03; tx[4] = (ms_p1 >> 8) & 0xFF; tx[5] = ms_p1 & 0xFF;
    tx[6] = 0; tx[7] = 0; tx[8] = 0;
    i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 9, false, I2C_TIMEOUT);
    
    tx[0] = 50; // MS1 Base
    i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 9, false, I2C_TIMEOUT);

    // 2. Program PLLA
    tx[0] = 26; // PLLA Base
    tx[1] = (P3 >> 8) & 0xFF;
    tx[2] = P3 & 0xFF;
    tx[3] = ((P3 >> 12) & 0xF0) | ((P1 >> 16) & 0x03);
    tx[4] = (P1 >> 8) & 0xFF;
    tx[5] = P1 & 0xFF;
    tx[6] = ((P3 >> 12) & 0xF0) | ((P2 >> 16) & 0x0F);
    tx[7] = (P2 >> 8) & 0xFF;
    tx[8] = P2 & 0xFF;
    i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 9, false, I2C_TIMEOUT);

    // 3. Configure Clock Control Registers
    tx[0] = 16; tx[1] = 0x4F; // CLK0_CTRL
    i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 2, false, I2C_TIMEOUT);
    tx[0] = 17; tx[1] = 0x4F; // CLK1_CTRL
    i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 2, false, I2C_TIMEOUT);

    // 4. Set Phase Offsets
    if (direct_mode) {
        tx[0] = 165; tx[1] = 0; // CLK0_PHOFF
        i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 2, false, I2C_TIMEOUT);
        tx[0] = 166; tx[1] = N; // CLK1_PHOFF
        i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 2, false, I2C_TIMEOUT);
    }

    // 5. Soft Reset PLLA
    tx[0] = 177; tx[1] = 0xA0;
    i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 2, false, I2C_TIMEOUT);

    // 6. Re-enable Outputs 0 and 1
    uint8_t oe_reg = 3;
    uint8_t oe_val;
    i2c_write_timeout_us(i2c, SI5351_ADDR, &oe_reg, 1, true, I2C_TIMEOUT);
    i2c_read_timeout_us(i2c, SI5351_ADDR, &oe_val, 1, false, I2C_TIMEOUT);
    oe_val &= ~0x03; // Clear bits 0 and 1
    tx[0] = 3; tx[1] = oe_val;
    i2c_write_timeout_us(i2c, SI5351_ADDR, tx, 2, false, I2C_TIMEOUT);
}
