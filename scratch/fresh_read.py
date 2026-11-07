import serial
import time

s = serial.Serial()
s.port = "COM5"
s.baudrate = 115200
s.dtr = False
s.rts = False
s.timeout = 1
s.open()

# Flush any old backlog in the OS driver buffer
s.reset_input_buffer()
print("Input buffer flushed. Listening for fresh incoming data for 4 seconds...")

start = time.time()
while time.time() - start < 4.0:
    line = s.readline()
    if line:
        print(repr(line.decode('utf-8', errors='replace')))

s.close()

