import sys
import time
import serial
import serial.tools.list_ports

# Force utf-8 output so non-ASCII characters like 'é' in Pokémon print cleanly
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PORT = "COM5"
BAUD = 115200

def find_esp32_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if "CP210" in p.description or "CH340" in p.description or "USB to UART" in p.description:
            return p.device
    return PORT

def main():
    port = find_esp32_port()
    print(f"=== Serial Monitor started for {port} @ {BAUD} baud ===", flush=True)
    
    ser = None
    while True:
        try:
            if ser is None or not ser.is_open:
                ser = serial.Serial()
                ser.port = port
                ser.baudrate = BAUD
                ser.timeout = 0.5
                ser.dtr = False
                ser.rts = False
                ser.open()
                print(f"[{time.strftime('%H:%M:%S')}] Connected to {port} without reset!", flush=True)
            
            line = ser.readline()
            if line:
                try:
                    text = line.decode('utf-8', errors='replace').rstrip()
                    if text:
                        print(f"[{time.strftime('%H:%M:%S')}] {text}", flush=True)
                except Exception as e:
                    print(f"[{time.strftime('%H:%M:%S')}] [RAW] {line}", flush=True)
        except serial.SerialException:
            if ser:
                try: ser.close()
                except: pass
                ser = None
            time.sleep(1)
        except KeyboardInterrupt:
            break

    if ser:
        ser.close()
    print("=== Serial Monitor stopped ===", flush=True)

if __name__ == "__main__":
    main()
