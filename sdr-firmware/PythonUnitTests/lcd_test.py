class LCD8Bit:
    def __init__(self, rs, e, db_pins):
        self.rs = machine.Pin(rs, machine.Pin.OUT)
        self.e = machine.Pin(e, machine.Pin.OUT)
        self.db = [machine.Pin(p, machine.Pin.OUT) for p in db_pins]
        
        # 8-bit Initialization Sequence 
        time.sleep_ms(100) # Wait >40msec after power
        self.command(0x30) # Wake up
        time.sleep_ms(30)
        self.command(0x30) # Wake up #2
        time.sleep_ms(10)
        self.command(0x30) # Wake up #3
        
        # Function set: 8-bit interface, 1-line (or 2 if applicable), 5x8 font
        self.command(0x38) 
        self.command(0x0C) # Display ON, Cursor OFF
        self.command(0x06) # Entry mode set
        self.clear()

    def pulse_enable(self):
        self.e.value(1)
        time.sleep_us(1) 
        self.e.value(0)
        time.sleep_us(100)

    def command(self, cmd):
        self.rs.value(0) # Instruction mode
        self.write_bus(cmd)
        self.pulse_enable()

    def write_bus(self, val):
        for i in range(8):
            self.db[i].value((val >> i) & 1)

    def clear(self):
        self.command(0x01)
        time.sleep_ms(2)

    def print(self, text):
        for char in text:
            self.rs.value(1) # Data mode
            self.write_bus(ord(char))
            self.pulse_enable()

# Mapping your pins: DB0=3, DB1=4, DB2=5, DB3=6, DB4=7, DB5=8, DB6=9, DB7=10
db_pins = [3, 4, 5, 6, 7, 8, 9, 10]
lcd = LCD8Bit(rs=14, e=15, db_pins=db_pins)

lcd.print("Hello!")
