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
    
    start = time.time()
    events = []
    fps_count = 0
    last_fps_line = ""
    while time.time() - start < 4.0:
        line = ser.readline()
        if line:
            text = line.decode('utf-8', errors='replace').rstrip()
            if not text:
                continue
            if "[RetroApp] Game: Pokémon Red" in text:
                fps_count += 1
                last_fps_line = text
            else:
                events.append(text)
    ser.close()
    
    print(f"FPS samples received: {fps_count}")
    if last_fps_line:
        print(f"Latest FPS line: {last_fps_line}")
    if events:
        print("Captured Non-FPS Events:")
        for ev in events:
            print(f"  {ev}")
    else:
        print("No non-FPS events captured during sample.")
except Exception as e:
    print(f"Error accessing {PORT}: {e}")

