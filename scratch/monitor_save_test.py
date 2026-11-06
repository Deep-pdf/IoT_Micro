import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("pyserial is not installed in the active Python environment.")
    sys.exit(1)

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
    print(f"Connecting to ESP32 on {port} @ {BAUD} baud...")
    
    while True:
        try:
            ser = serial.Serial(port, BAUD, timeout=1)
            print(f"Connected to {port}! Monitoring serial logs (Ctrl+C to stop)...")
            break
        except serial.SerialException:
            print(f"Waiting for {port} to connect...")
            time.sleep(2)

    try:
        while True:
            line = ser.readline()
            if line:
                try:
                    text = line.decode('utf-8', errors='replace').rstrip()
                    print(f"[{time.strftime('%H:%M:%S')}] {text}")
                except Exception as e:
                    print(f"[RAW] {line}")
    except KeyboardInterrupt:
        print("\nStopping monitor.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()

