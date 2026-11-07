import serial
import time

s = serial.Serial()
s.port = "COM5"
s.baudrate = 115200
s.dtr = False
s.rts = False
s.timeout = 0.1
s.open()

# Read until empty
total_drained = 0
while True:
    waiting = s.in_waiting
    if waiting > 0:
        chunk = s.read(waiting)
        total_drained += len(chunk)
    else:
        time.sleep(0.05)
        if s.in_waiting == 0:
            break

print(f"Drained backlog: {total_drained} bytes.")
print("Now measuring incoming rate over 3 seconds...")
t0 = time.time()
received = bytearray()
while time.time() - t0 < 3.0:
    waiting = s.in_waiting
    if waiting > 0:
        received.extend(s.read(waiting))
    time.sleep(0.01)

print(f"Received {len(received)} bytes in 3 seconds.")
print("Incoming sample repr:", repr(bytes(received[:200])))
s.close()

