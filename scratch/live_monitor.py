import sys
import time
import serial

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PORT = "COM5"
BAUD = 115200

def main():
    print(f"=== Starting Live Monitor on {PORT} @ {BAUD} ===", flush=True)
    ser = serial.Serial()
    ser.port = PORT
    ser.baudrate = BAUD
    ser.dtr = False
    ser.rts = False
    ser.timeout = 0.5
    ser.open()

    # Drain any driver backlog
    drained = 0
    while ser.in_waiting > 0:
        chunk = ser.read(ser.in_waiting)
        drained += len(chunk)
        time.sleep(0.02)
    if drained > 0:
        print(f"Drained {drained} bytes from driver buffer.", flush=True)

    log_file = open("scratch/serial_session.log", "a", encoding="utf-8")
    log_file.write(f"\n--- Session started at {time.strftime('%Y-%m-%d %H:%M:%S')} ---\n")
    log_file.flush()

    last_heartbeat = 0
    try:
        while True:
            line = ser.readline()
            if line:
                text = line.decode('utf-8', errors='replace').rstrip()
                if not text:
                    continue
                log_file.write(f"[{time.strftime('%H:%M:%S')}] {text}\n")
                log_file.flush()

                if "[RetroApp] Game: Pokémon Red" in text:
                    now = time.time()
                    if now - last_heartbeat >= 6.0:
                        print(f"[{time.strftime('%H:%M:%S')}] [GAME RUNNING] {text}", flush=True)
                        last_heartbeat = now
                else:
                    print(f"[{time.strftime('%H:%M:%S')}] >>> {text} <<<", flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        log_file.close()
        print("=== Live Monitor stopped ===", flush=True)

if __name__ == "__main__":
    main()

