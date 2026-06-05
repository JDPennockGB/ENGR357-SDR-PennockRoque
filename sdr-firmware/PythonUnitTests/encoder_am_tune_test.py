import machine
import time

# --- Hardware Pins ---
ENC_CLK_PIN = 20  # Enc_A
ENC_DT_PIN  = 21  # Enc_B
ENC_SW_PIN  = 22  # Enc_Sw (Push Button)
I2C_SDA_PIN = 12
I2C_SCL_PIN = 13

# --- AM Band Constants ---
FREQ_MIN = 530000   # 530 kHz
FREQ_MAX = 1700000  # 1700 kHz
STEP_HZ  = 10000    # 10 kHz AM Channel Spacing

# --- State Variables ---
MODE_TUNE = 0
MODE_PLAY = 1

current_mode = MODE_TUNE   # Start in TUNE mode
current_freq_hz = 1000000  # Start at 1000 kHz
freq_changed = True        
mode_changed = True        # Force initial UI update
last_enc_time = 0
last_btn_time = 0

# --- SI5351 Constants ---
SI5351_ADDR = 0x60
SI5351_REG_DEV_STATUS = 0
SI5351_REG_OEB = 3
SI5351_REG_CLK0_CTRL = 16
SI5351_REG_PLLA_BASE = 26
SI5351_REG_MS0_BASE = 42
SI5351_REG_PLL_RESET = 177
SI5351_REG_XTAL_LOAD = 183
SI5351_XTAL_FREQ = 24576000
SI5351_XTAL_LOAD_10PF = (3 << 6)

# Initialize I2C
i2c = machine.I2C(0, scl=machine.Pin(I2C_SCL_PIN), sda=machine.Pin(I2C_SDA_PIN), freq=400000)

# --- SI5351 Driver Functions ---
def si5351_write_reg(reg, val):
    i2c.writeto_mem(SI5351_ADDR, reg, bytes([val]))

def si5351_read_reg(reg):
    return i2c.readfrom_mem(SI5351_ADDR, reg, 1)[0]

def si5351_init():
    print("Initializing SI5351...")
    si5351_write_reg(SI5351_REG_OEB, 0xFF) # Start muted
    si5351_write_reg(SI5351_REG_XTAL_LOAD, SI5351_XTAL_LOAD_10PF)
    timeout = 100
    while timeout > 0:
        if (si5351_read_reg(SI5351_REG_DEV_STATUS) & 0x80) == 0:
            return True
        time.sleep(0.01)
        timeout -= 1
    return False

def set_si5351_freq(freq_hz):
    best_error = float('inf')
    best_n = 0
    best_m = 0
    
    for n in range(25, 37):
        vco_freq = n * SI5351_XTAL_FREQ
        m = vco_freq // freq_hz
        if m % 2 == 1: m += 1
        actual_freq = vco_freq // m
        error = abs(actual_freq - freq_hz)
        
        if error < best_error:
            best_error = error
            best_n = n
            best_m = m

    # Program PLLA
    p1 = 128 * best_n - 512
    si5351_write_reg(SI5351_REG_PLLA_BASE + 0, 0)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 1, 1)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 2, (p1 >> 16) & 0x03)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 3, (p1 >> 8) & 0xFF)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 4, p1 & 0xFF)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 5, 0)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 6, 0)
    si5351_write_reg(SI5351_REG_PLLA_BASE + 7, 0)
    
    # Program MS0
    p1 = 128 * best_m - 512
    si5351_write_reg(SI5351_REG_MS0_BASE + 0, 0)
    si5351_write_reg(SI5351_REG_MS0_BASE + 1, 1)
    si5351_write_reg(SI5351_REG_MS0_BASE + 2, (p1 >> 16) & 0x03)
    si5351_write_reg(SI5351_REG_MS0_BASE + 3, (p1 >> 8) & 0xFF)
    si5351_write_reg(SI5351_REG_MS0_BASE + 4, p1 & 0xFF)
    si5351_write_reg(SI5351_REG_MS0_BASE + 5, 0)
    si5351_write_reg(SI5351_REG_MS0_BASE + 6, 0)
    si5351_write_reg(SI5351_REG_MS0_BASE + 7, 0)
    
    si5351_write_reg(SI5351_REG_CLK0_CTRL, 0x4F) 
    si5351_write_reg(SI5351_REG_PLL_RESET, 0xA0) 

def set_si5351_mute(mute):
    oeb = si5351_read_reg(SI5351_REG_OEB)
    if mute:
        si5351_write_reg(SI5351_REG_OEB, oeb | 0x01)  # Set bit 0 to mute
    else:
        si5351_write_reg(SI5351_REG_OEB, oeb & ~0x01) # Clear bit 0 to play

# --- Interrupt Handlers ---
def button_isr(pin):
    global current_mode, mode_changed, last_btn_time
    
    # 200ms debounce to prevent double-clicks
    current_time = time.ticks_ms()
    if time.ticks_diff(current_time, last_btn_time) < 200:
        return
    last_btn_time = current_time

    # Toggle the mode
    if current_mode == MODE_TUNE:
        current_mode = MODE_PLAY
    else:
        current_mode = MODE_TUNE
        
    mode_changed = True


# --- Interrupt Handlers ---
def encoder_isr(pin):
    global current_freq_hz, freq_changed, last_enc_time
    
    # If we are locked in PLAY mode, ignore the rotation
    if current_mode == MODE_PLAY:
        return
    
    # 5ms debounce
    current_time = time.ticks_ms()
    if time.ticks_diff(current_time, last_enc_time) < 5:
        return
    last_enc_time = current_time

    # FIXED DIRECTION: Changed != to ==
    if enc_dt.value() == enc_clk.value():
        current_freq_hz += STEP_HZ  # Turn Right -> Tune UP
    else:
        current_freq_hz -= STEP_HZ  # Turn Left -> Tune DOWN

    # Constrain to AM band bounds
    if current_freq_hz > FREQ_MAX:
        current_freq_hz = FREQ_MAX
    elif current_freq_hz < FREQ_MIN:
        current_freq_hz = FREQ_MIN

    freq_changed = True

# ... (Keep your button_isr and pin setups the same) ...

# --- Setup Encoder Pins ---
enc_clk = machine.Pin(ENC_CLK_PIN, machine.Pin.IN, machine.Pin.PULL_UP)
enc_dt  = machine.Pin(ENC_DT_PIN, machine.Pin.IN, machine.Pin.PULL_UP)
enc_sw  = machine.Pin(ENC_SW_PIN, machine.Pin.IN, machine.Pin.PULL_UP)

# FIXED RESOLUTION: Trigger on BOTH falling and rising edges to catch every single click!
enc_clk.irq(trigger=machine.Pin.IRQ_FALLING | machine.Pin.IRQ_RISING, handler=encoder_isr)
enc_sw.irq(trigger=machine.Pin.IRQ_FALLING, handler=button_isr)
# --- Main Loop ---
print("\n=== AM SDR Controller ===")

if si5351_init():
    print("System Ready. Starting in [TUNE] mode.")
    
    while True:
        # Handle Mode Switches (Play/Mute)
        if mode_changed:
            mode_changed = False
            if current_mode == MODE_TUNE:
                set_si5351_mute(True)  # Mute audio
                print(f"\n[TUNE MODE] Audio Muted. Encoder Unlocked.")
                print(f"-> Frequency: {current_freq_hz / 1000:,.0f} kHz")
            elif current_mode == MODE_PLAY:
                set_si5351_mute(False) # Unmute audio
                print(f"\n[PLAY MODE] Listening at {current_freq_hz / 1000:,.0f} kHz. Encoder Locked.")
        
        # Handle Tuning Updates
        if freq_changed:
            freq_changed = False 
            
            pll_freq = current_freq_hz * 4
            set_si5351_freq(pll_freq)
            
            print(f"-> Tuned to: {current_freq_hz / 1000:,.0f} kHz")
        
        time.sleep(0.01)
else:
    print("Failed to find Si5351.")