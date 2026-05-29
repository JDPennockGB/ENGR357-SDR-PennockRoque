import machine
import time

class Si5351:
    def __init__(self, i2c_bus, addr=0x60, xtal_freq=24576000):
        self.i2c = i2c_bus
        self.addr = addr
        self.xtal = xtal_freq
        
        # Disable all outputs initially
        self.write_reg(3, 0xFF)   
        # Set crystal load to 10pF (Register 183)
        self.write_reg(183, 0xD2) 
        time.sleep(0.1)

    def write_reg(self, reg, val):
        """Write a single byte to a register."""
        self.i2c.writeto_mem(self.addr, reg, bytes([val]))

    def set_clock(self, clk_channel, freq_hz, drive_ma=8):
        """
        Variable function to configure any clock output.
        - clk_channel: 0, 1, or 2
        - freq_hz: Target frequency in Hz
        - drive_ma: Drive strength (2, 4, 6, or 8 mA)
        """
        if clk_channel not in [0, 1, 2]:
            print("Error: Clock channel must be 0, 1, or 2")
            return False

        # Map drive strength to bits (00=2mA, 01=4mA, 10=6mA, 11=8mA)
        drive_bits = {2: 0x00, 4: 0x01, 6: 0x02, 8: 0x03}.get(drive_ma, 0x03)

        # 1. Calculate Integer PLL Parameters
        best_err = float('inf')
        best_N = 0
        best_M = 0
        
        for N in range(25, 37):
            vco = N * self.xtal
            M = vco // freq_hz
            if M % 2 != 0: 
                M += 1 # Force even for 50% duty cycle
            
            err = abs((vco // M) - freq_hz)
            if err < best_err:
                best_err = err
                best_N = N
                best_M = M
                
        if best_N == 0: 
            print("Error: Frequency out of integer range.")
            return False

        # 2. Program PLLA (Registers 26-33)
        # We use PLLA for all outputs in this simple integer test
        p1_pll = 128 * best_N - 512
        pll_regs = [
            0, 1, 
            (p1_pll >> 16) & 0x03, (p1_pll >> 8) & 0xFF, p1_pll & 0xFF, 
            0, 0, 0
        ]
        for i, val in enumerate(pll_regs):
            self.write_reg(26 + i, val)

        # 3. Program Multisynth (MS0, MS1, or MS2)
        # CLK0 is base 42, CLK1 is base 50, CLK2 is base 58
        ms_base = 42 + (clk_channel * 8)
        p1_ms = 128 * best_M - 512
        ms_regs = [
            0, 1, 
            (p1_ms >> 16) & 0x03, (p1_ms >> 8) & 0xFF, p1_ms & 0xFF, 
            0, 0, 0
        ]
        for i, val in enumerate(ms_regs):
            self.write_reg(ms_base + i, val)

        # 4. Configure Clock Control Register
        # Reg 16=CLK0, Reg 17=CLK1, Reg 18=CLK2
        # Bits: PowerDown(0), IntegerMode(1), SourcePLLA(00), Invert(0), Drive(xx)
        clk_ctrl_reg = 16 + clk_channel
        ctrl_val = 0x4C | drive_bits # 0x4C = 0100 1100 (Int mode, PLLA source)
        self.write_reg(clk_ctrl_reg, ctrl_val)

        # 5. Reset PLLA
        self.write_reg(177, 0x20)

        # 6. Enable the specific clock output
        # Read current OEB state, clear the bit for our channel to enable it
        oeb_state = self.i2c.readfrom_mem(self.addr, 3, 1)[0]
        oeb_state &= ~(1 << clk_channel)
        self.write_reg(3, oeb_state)

        actual_freq = (best_N * self.xtal) // best_M
        print(f"CLK{clk_channel} set to {actual_freq} Hz (Drive: {drive_ma}mA)")
        return True

# ==========================================
# TEST SEQUENCE
# ==========================================
SDA_PIN = 12
SCL_PIN = 13

print("\n=== Si5351 Variable Function Test ===")
i2c = machine.I2C(0, sda=machine.Pin(SDA_PIN), scl=machine.Pin(SCL_PIN), freq=400_000)

# Check if Si5351 is on the bus
if 0x60 not in i2c.scan():
    print("CRITICAL ERROR: Si5351 not found at 0x60. Check wiring.")
else:
    clock_gen = Si5351(i2c)
    
    # Define a list of test configurations (Channel, Freq, Drive_mA)
    test_sequence = [
        (0, 7074000, 8),   # CLK0, 7.074 MHz, 8mA (FT8 40m)
        (0, 14074000, 2),  # CLK0, 14.074 MHz, 2mA (FT8 20m, lower drive)
        (1, 10000000, 8),  # CLK1, 10 MHz, 8mA (Standard reference)
        (2, 12288000, 4)   # CLK2, 12.288 MHz, 4mA (Typical I2S Master Clock)
    ]
    
    print("Starting parameter sweep. Connect a scope or frequency counter.")
    print("Press Ctrl+C to stop.\n")
    
    try:
        while True:
            for channel, freq, drive in test_sequence:
                clock_gen.set_clock(clk_channel=channel, freq_hz=freq, drive_ma=drive)
                
                # Hold the frequency for 5 seconds before changing
                time.sleep(5)
                
    except KeyboardInterrupt:
        print("\nTest terminated. Shutting down outputs.")
        clock_gen.write_reg(3, 0xFF) # Disable all outputs