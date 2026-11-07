import serial
import time

s = serial.Serial()
s.port = "COM5"
s.baudrate = 115200
s.dtr = False
s.rts = False
s.timeout = 1
s.open()
time.sleep(0.1)

# read 5 lines and print exact repr
for _ in range(5):
    line = s.readline()
    print(repr(line))

s.close()

