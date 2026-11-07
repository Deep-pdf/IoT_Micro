import serial
import time

s = serial.Serial()
s.port = "COM5"
s.baudrate = 115200
s.dtr = False
s.rts = False
s.timeout = 2
s.open()

# Read 500 raw bytes
data = s.read(500)
print(f"Read {len(data)} bytes:")
print(data)
s.close()

