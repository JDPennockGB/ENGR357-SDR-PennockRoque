import machine
import time

# SI5351 I2C address and register definitions
SI5351_ADDR = 0x60
SI5351_REG_DEV_STATUS = 0
SI5351_REG_OEB = 3
SI5351_REG_CLK0_CTRL = 16
SI5351_REG_PLLA_BASE = 26
SI5351_REG_MS0_BASE = 42
SI5351_REG_CLK0_PHOFF = 165
SI5351_REG_PLL_RESET = 177
SI5351_REG_XTAL_LOAD = 183

# 2026 board crystal frequency: 24.576 MHz
SI5351_XTAL_FREQ = 24576000
SI5351_XTAL_LOAD_10PF = (3 << 6)

# Initialize I2C
i2c = machine.I2C(0, scl=machine.Pin(13), sda=machine.Pin(12), freq=400000)

def si5351_write_reg(reg, val):
    """Write a single register to SI5351"""
    i2c.writeto_mem(SI5351_ADDR, reg, bytes([val]))

def si5351_read_reg(reg):
    """Read a single register from SI5351"""
    return i2c.readfrom_mem(SI5351_ADDR, reg, 1)[0]

def si5351_init():
    """Initialize the SI5351"""
    print("Initializing SI5351...")
    
    # Disable all outputs
    si5351_write_reg(SI5351_REG_OEB, 0xFF)
    
    # Set crystal load capacitance to 10pF
    si5351_write_reg(SI5351_REG_XTAL_LOAD, SI5351_XTAL_LOAD_10PF)
    
    # Wait for SYS_INIT to clear (device ready)
    timeout = 100
    while timeout > 0:
        status = si5351_read_reg(SI5351_REG_DEV_STATUS)
        if (status & 0x80) == 0:  # SYS_INIT bit
            print("SI5351 ready!")
            return True
        time.sleep(0.01)
        timeout -= 1
    
    print("SI5351 initialization timeout!")
    return False

def set_frequency(freq_hz):
    """
    Configure SI5351 for a specific output frequency.
    Uses integer PLL mode (N) and multisynth divider (M).
    Simple approach: finds N (25-36) and M that gives close to target frequency.
    """
    print(f"Setting frequency to {freq_hz} Hz ({freq_hz/1e6:.3f} MHz)...")
    
    # Try different PLL multipliers (N = 25-36)
    best_error = float('inf')
    best_n = 0
    best_m = 0
    best_freq = 0
    
    for n in range(25, 37):
        vco_freq = n * SI5351_XTAL_FREQ
        
        # Calculate output divider M (must be even)
        m = vco_freq // freq_hz
        if m % 2 == 1:  # Force even
            m += 1
        
        actual_freq = vco_freq // m
        error = abs(actual_freq - freq_hz)
        
        if error < best_error:
            best_error = error
            best_n = n
            best_m = m
            best_freq = actual_freq
    
    print(f"Best match: N={best_n}, M={best_m}, actual freq = {best_freq} Hz")
    
    # Program PLLA: N is the multiplier
    # For integer mode, P1 = 128*N - 512, P2 = 0, P3 = 1
    n = best_n
    p1 = 128 * n - 512
    p2 = 0
    p3 = 1
    
    # Write PLLA multisynth registers (26-33)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 0, (p3 >> 8) & 0xFF)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 1, p3 & 0xFF)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 2, (p1 >> 16) & 0x03)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 3, (p1 >> 8) & 0xFF)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 4, p1 & 0xFF)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 5, ((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0x0F))
    si5351_write_reg(SI5351_REG_PLLA_BASE + 6, (p2 >> 8) & 0xFF)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 7, p2 & 0xFF)
    
    # Program MS0 (CLK0) multisynth: divider M
    m = best_m
    p1 = 128 * m - 512
    p2 = 0
    p3 = 1
    
    # Write MS0 registers (42-49)
    si5351_write_reg(SI5351_REG_MS0_BASE + 0, (p3 >> 8) & 0xFF)
    si5351_write_reg(SI5351_REG_MS0_BASE + 1, p3 & 0xFF)
    si5351_write_reg(SI5351_REG_MS0_BASE + 2, (p1 >> 16) & 0x03)
    si5351_write_reg(SI5351_REG_MS0_BASE + 3, (p1 >> 8) & 0xFF)
    si5351_write_reg(SI5351_REG_MS0_BASE + 4, p1 & 0xFF)
    si5351_write_reg(SI5351_REG_MS0_BASE + 5, ((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0x0F))
    si5351_write_reg(SI5351_REG_MS0_BASE + 6, (p2 >> 8) & 0xFF)
    si5351_write_reg(SI5351_REG_MS0_BASE + 7, p2 & 0xFF)
    
    # Configure CLK0 to use PLLA and MS0
    # Bits: 7=disable, 6:5=source (01=PLLA), 4=invert, 3:0=drive strength
    si5351_write_reg(SI5351_REG_CLK0_CTRL, 0x4F)  # PLLA source, 8mA drive
    
    # Reset PLL
    si5351_write_reg(SI5351_REG_PLL_RESET, 0xA0)  # Reset PLLA and PLLB
    
    # Enable CLK0 output (clear OEB bit 0)
    oeb = si5351_read_reg(SI5351_REG_OEB)
    oeb &= ~0x01  # Clear bit 0 to enable CLK0
    si5351_write_reg(SI5351_REG_OEB, oeb)
    
    print(f"CLK0 enabled at {best_freq} Hz")
    return best_freq

# Main
print("\n=== SI5351 Clock Generator ===\n")

if si5351_init():
    # Set CLK0 to 10 MHz
    set_frequency(10000000)
    print("\nClock signal should now be output on CLK0!")
    print("Use a scope/logic analyzer to verify the signal on the output pin.\n")
else:
    print("Failed to initialize SI5351")