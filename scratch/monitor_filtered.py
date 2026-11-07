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
    print(f"Connected to {PORT}. Monitoring for non-FPS logs for 10 seconds...", flush=True)
    start = time.time()
    last_fps_time = 0
    while time.time() - start < 10.0:
        line = ser.readline()
        if line:
            text = line.decode('utf-8', errors='replace').rstrip()
            if not text:
                continue
            if "[RetroApp] Game: Pokémon Red" in text:
                now = time.time()
                if now - last_fps_time > 2.0:
                    print(f"[HEARTBEAT] {text}", flush=True)
                    last_fps_time = now
            else:
                print(f"[EVENT] {text}", flush=True)
    ser.close()
    print("Monitoring ended.", flush=True)
except Exception as e:
    print(f"Error accessing {PORT}: {e}", flush=True)

