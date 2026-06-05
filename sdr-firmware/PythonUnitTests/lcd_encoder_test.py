import machine
import time

# --- Hardware Pins ---
ENC_A_PIN    = 20
ENC_B_PIN    = 21
ENC_SW_PIN   = 22
RS_PIN       = 14
E_PIN        = 15
DB_PINS      = [3, 4, 5, 6, 7, 8, 9, 10]

# --- State ---
current_freq = 1000
freq_changed = True

# --- LCD Driver (8-Bit Mode) ---
class LCD8Bit:
    def __init__(self, rs, e, db_pins):
        self.rs = machine.Pin(rs, machine.Pin.OUT)
        self.e = machine.Pin(e, machine.Pin.OUT)
        self.db = [machine.Pin(p, machine.Pin.OUT) for p in db_pins]
        time.sleep_ms(100)
        for _ in range(3):
            self.command(0x30)
            time.sleep_ms(5)
        self.command(0x38) # 8-bit mode
        self.command(0x0C) # Display ON, Cursor OFF
        self.command(0x06) # Entry mode
        self.clear()

    def pulse_enable(self):
        self.e.value(1)
        time.sleep_us(1)
        self.e.value(0)
        time.sleep_us(100)

    def command(self, cmd):
        self.rs.value(0)
        for i in range(8):
            self.db[i].value((cmd >> i) & 1)
        self.pulse_enable()

    def clear(self):
        self.command(0x01)
        time.sleep_ms(2)

    def print(self, text):
        # Clear screen first to avoid ghosting
        self.clear()
        self.rs.value(1)
        for char in text[:8]: # 8 chars max
            for i in range(8):
                self.db[i].value((ord(char) >> i) & 1)
            self.pulse_enable()

# --- Encoder Logic ---
def encoder_isr(pin):
    global current_freq, freq_changed
    # Simple direction check
    if pin == enc_a:
        if enc_b.value() != pin.value():
            current_freq += 10
        else:
            if current_freq > 10: current_freq -= 10
        freq_changed = True

# Initialize
enc_a = machine.Pin(ENC_A_PIN, machine.Pin.IN, machine.Pin.PULL_UP)
enc_b = machine.Pin(ENC_B_PIN, machine.Pin.IN, machine.Pin.PULL_UP)
enc_a.irq(trigger=machine.Pin.IRQ_FALLING, handler=encoder_isr)

lcd = LCD8Bit(RS_PIN, E_PIN, DB_PINS)

# --- Main Loop ---
while True:
    if freq_changed:
        freq_changed = False
        
        # This formats the frequency to 4 chars wide, followed by " kHz"
        # 1000 -> "1000 kHz" (8 chars)
        #  990 -> " 990 kHz" (8 chars)
        display_str = f"{current_freq:>4} kHz"
        
        lcd.clear()
        lcd.print(display_str)
        print(display_str) # Debug in Serial Monitor
        
    time.sleep(0.01)
