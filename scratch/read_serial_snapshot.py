import sys
import time
import serial

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PORT = "COM5"
BAUD = 115200

try:
    ser = serial.Serial()
    ser.port = PORT
    ser.baudrate = BAUD
    ser.timeout = 0.5
    ser.dtr = False
    ser.rts = False
    ser.open()
    print(f"Connected to {PORT}. Listening for 3 seconds...")
    start = time.time()
    lines_received = 0
    while time.time() - start < 3.0:
        line = ser.readline()
        if line:
            text = line.decode('utf-8', errors='replace').rstrip()
            if text:
                print(f"[SERIAL] {text}")
                lines_received += 1
    if lines_received == 0:
        print("No serial lines received in 3 seconds (device is idle).")
    ser.close()
except Exception as e:
    print(f"Error accessing {PORT}: {e}")

