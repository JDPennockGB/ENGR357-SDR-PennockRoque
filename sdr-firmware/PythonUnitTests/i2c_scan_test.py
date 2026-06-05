import machine
import time

# Initialize I2C on GPIO 12 (SDA) and 13 (SCL)
i2c = machine.I2C(0, scl=machine.Pin(13), sda=machine.Pin(12), freq=400000)

print("\n=== I2C Device Scan ===")
print("Scanning I2C bus 0 (SDA=GPIO12, SCL=GPIO13)")
print()

devices = i2c.scan()

if devices:
    print(f"Found {len(devices)} device(s):")
    for addr in devices:
        print(f"  - Address 0x{addr:02X} ({addr})")
else:
    print("No I2C devices found.")

print("\n=== Scan Complete ===\n")


