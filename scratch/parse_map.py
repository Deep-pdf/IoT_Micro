import re
import os
import sys

def parse_map(map_path):
    print(f"Parsing {map_path}...")
    
    # regex for section mapping lines
    # e.g.:
    #  .flash.rodata    0x000000003f400020   0x1afa24 .pio/build/esp32dev/src/lost_crown/Level1.cpp.o
    #  .flash.text      0x00000000400d0020     0x1234 .pio/build/esp32dev/src/main.cpp.o
    
    symbol_pattern = re.compile(r'^\s*([.][a-zA-Z0-9_.]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(.*)$')
    sub_symbol_pattern = re.compile(r'^\s+(0x[0-9a-fA-F]+)\s+([a-zA-Z0-9_]+)$')
    
    # We want to group by:
    # 1. Module / component (e.g. Lost Crown, Retro Arcade (NES), Retro Arcade (GB), Pixel Wars, Spotify, AI, Hackk, Core/UI/Drivers, Arduino/IDF Framework, Libraries)
    # 2. Object file
    # 3. Memory section:
    #    - flash_rodata (.flash.rodata*)
    #    - flash_text (.flash.text*)
    #    - iram (.iram0.*)
    #    - dram_data (.dram0.data*)
    #    - dram_bss (.dram0.bss*)
    #    - rtc (.rtc*)
    
    objects = {} # obj_name -> {section_type: bytes}
    symbols = [] # (sym_name, size, section_type, obj_name)
    
    current_sec = None
    current_addr = 0
    current_size = 0
    current_obj = None
    
    with open(map_path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            m = symbol_pattern.match(line)
            if m:
                sec_name = m.group(1)
                addr_str = m.group(2)
                size_str = m.group(3)
                obj_str = m.group(4).strip()
                
                size = int(size_str, 16)
                if size == 0 or not obj_str:
                    continue
                
                # Classify section
                sec_type = "other"
                if sec_name.startswith(".flash.rodata"):
                    sec_type = "flash_rodata"
                elif sec_name.startswith(".flash.text"):
                    sec_type = "flash_text"
                elif sec_name.startswith(".iram0"):
                    sec_type = "iram"
                elif sec_name.startswith(".dram0.data"):
                    sec_type = "dram_data"
                elif sec_name.startswith(".dram0.bss"):
                    sec_type = "dram_bss"
                elif sec_name.startswith(".rtc"):
                    sec_type = "rtc"
                
                if sec_type == "other":
                    continue
                
                # Simplify obj_str
                obj_clean = obj_str.replace('\\', '/')
                if obj_clean.startswith('.pio/build/esp32dev/'):
                    obj_clean = obj_clean[len('.pio/build/esp32dev/'):]
                
                if obj_clean not in objects:
                    objects[obj_clean] = {"flash_rodata": 0, "flash_text": 0, "iram": 0, "dram_data": 0, "dram_bss": 0, "rtc": 0}
                objects[obj_clean][sec_type] += size
                
                current_sec = sec_type
                current_addr = int(addr_str, 16)
                current_size = size
                current_obj = obj_clean
            else:
                m_sub = sub_symbol_pattern.match(line)
                if m_sub and current_sec:
                    sym_name = m_sub.group(2)
                    symbols.append((sym_name, current_size, current_sec, current_obj))
                    current_sec = None # record only first symbol or distinct ones
    
    return objects, symbols

def classify_module(obj_name):
    obj = obj_name.lower()
    if 'lost_crown' in obj:
        return "Lost Crown Game"
    elif 'pixel_wars' in obj:
        return "Pixel Wars Game"
    elif 'retro/nes' in obj or 'nestest' in obj:
        return "Retro Arcade (NES)"
    elif 'retro/gb' in obj:
        return "Retro Arcade (Game Boy)"
    elif 'retro/' in obj:
        return "Retro Arcade (Launcher/Common)"
    elif 'ai/' in obj:
        return "AI Assistant App"
    elif 'spotify' in obj:
        return "Spotify Remote App"
    elif 'hackk' in obj or 'deauth' in obj:
        return "Hackk Security Suite"
    elif 'home_screen' in obj or 'joystick' in obj or 'button' in obj or 'status_led' in obj or 'main' in obj:
        return "System / OS Core & UI"
    elif 'adafruit' in obj:
        return "Adafruit GFX / ST7735 Library"
    elif 'arduinojson' in obj:
        return "ArduinoJson Library"
    elif 'frameworkarduino' in obj or 'cores/esp32' in obj:
        return "Arduino Core (ESP32)"
    elif 'libwifi' in obj or 'wpa_supplicant' in obj or 'net80211' in obj or 'pp.a' in obj:
        return "Wi-Fi & Networking Stack (IDF)"
    elif 'freertos' in obj:
        return "FreeRTOS Kernel (IDF)"
    elif 'newlib' in obj or 'libc' in obj or 'libm' in obj or 'gcc' in obj:
        return "C/C++ Standard Runtime (LibC/LibM/LibGCC)"
    elif 'esp32' in obj or 'soc' in obj or 'driver' in obj or 'vfs' in obj or 'spi_flash' in obj or 'hal' in obj or 'heap' in obj:
        return "ESP-IDF System & Drivers"
    else:
        return "Other Libraries / IDF Components"

def main():
    map_path = r".pio\build\esp32dev\firmware.map"
    objects, symbols = parse_map(map_path)
    
    # Aggregate by Module
    modules = {}
    for obj, sec_dict in objects.items():
        mod = classify_module(obj)
        if mod not in modules:
            modules[mod] = {"flash_rodata": 0, "flash_text": 0, "iram": 0, "dram_data": 0, "dram_bss": 0, "rtc": 0, "objects": []}
        modules[mod]["objects"].append(obj)
        for k in ["flash_rodata", "flash_text", "iram", "dram_data", "dram_bss", "rtc"]:
            modules[mod][k] += sec_dict[k]
    
    print("\n" + "="*115)
    print(f"{'Module / Subsystem':35s} | {'Flash Text':>11s} | {'Flash Rodata':>12s} | {'Total Flash':>12s} | {'Static DRAM':>11s} | {'IRAM':>8s}")
    print("="*115)
    
    sorted_mods = sorted(modules.items(), key=lambda x: (x[1]["flash_text"] + x[1]["flash_rodata"]), reverse=True)
    total_flash_all = 0
    total_dram_all = 0
    total_iram_all = 0
    
    for mod, d in sorted_mods:
        tot_flash = d["flash_text"] + d["flash_rodata"]
        tot_dram = d["dram_data"] + d["dram_bss"]
        total_flash_all += tot_flash
        total_dram_all += tot_dram
        total_iram_all += d["iram"]
        print(f"{mod:35s} | {d['flash_text']:>9,d} B | {d['flash_rodata']:>10,d} B | {tot_flash:>10,d} B | {tot_dram:>9,d} B | {d['iram']:>6,d} B")
    
    print("="*115)
    print(f"{'TOTAL IDENTIFIED':35s} | {sum(d['flash_text'] for _, d in sorted_mods):>9,d} B | {sum(d['flash_rodata'] for _, d in sorted_mods):>10,d} B | {total_flash_all:>10,d} B | {total_dram_all:>9,d} B | {total_iram_all:>6,d} B")
    print("="*115)

    print("\n=== TOP 25 LARGEST OBJECT FILES BY TOTAL FLASH (Text + Rodata) ===")
    sorted_objs = sorted(objects.items(), key=lambda x: (x[1]["flash_text"] + x[1]["flash_rodata"]), reverse=True)
    for obj, d in sorted_objs[:25]:
        tot = d["flash_text"] + d["flash_rodata"]
        dram = d["dram_data"] + d["dram_bss"]
        print(f"{obj:65s} | Flash: {tot:>9,d} B (rodata: {d['flash_rodata']:>9,d} B) | DRAM: {dram:>7,d} B")

    print("\n=== TOP 25 LARGEST OBJECT FILES BY STATIC DRAM (Data + BSS) ===")
    sorted_dram_objs = sorted(objects.items(), key=lambda x: (x[1]["dram_data"] + x[1]["dram_bss"]), reverse=True)
    for obj, d in sorted_dram_objs[:25]:
        tot_dram = d["dram_data"] + d["dram_bss"]
        print(f"{obj:65s} | DRAM: {tot_dram:>7,d} B (data: {d['dram_data']:>6,d} B, bss: {d['dram_bss']:>6,d} B)")

if __name__ == "__main__":
    main()

