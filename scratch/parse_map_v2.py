import re
import os
import sys

def parse_map(map_path):
    print(f"Parsing {map_path}...")
    
    # Track top-level memory sections
    # Major output sections in ESP32:
    # .flash.rodata
    # .flash.text
    # .iram0.text / .iram0.vectors
    # .dram0.data
    # .dram0.bss
    # .rtc_noinit / .rtc.text
    
    current_major_sec = None
    
    # We map major sections to our categories:
    # "flash_rodata", "flash_text", "iram", "dram_data", "dram_bss", "rtc", "other"
    
    def get_sec_category(name):
        if not name: return "other"
        if name.startswith(".flash.rodata"): return "flash_rodata"
        if name.startswith(".flash.text"): return "flash_text"
        if name.startswith(".iram0"): return "iram"
        if name.startswith(".dram0.data"): return "dram_data"
        if name.startswith(".dram0.bss"): return "dram_bss"
        if name.startswith(".rtc"): return "rtc"
        return "other"

    # Regex patterns
    # Top-level section start:
    # .flash.rodata   0x000000003f400120   0x1afb9c
    re_top_sec = re.compile(r'^([.][a-zA-Z0-9_.]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s*$')
    
    # Section line that wrapped:
    #  .rodata_wlog_error.37
    re_wrapped_sec_name = re.compile(r'^\s+([.][a-zA-Z0-9_.]+)\s*$')
    # Followed by address, size, object:
    #                 0x000000003f400120       0x23 C:/.../libnet80211.a(ieee80211_api.o)
    re_wrapped_sec_val = re.compile(r'^\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(\S.*)$')
    
    # Single-line section:
    #  .rodata        0x000000003f400120       0x23 C:/.../libnet80211.a(ieee80211_api.o)
    re_single_sec = re.compile(r'^\s+([.][a-zA-Z0-9_.]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(\S.*)$')
    
    # Symbol inside section:
    #                 0x000000003f400120                my_symbol
    re_symbol = re.compile(r'^\s+(0x[0-9a-fA-F]+)\s+([a-zA-Z0-9_]+)\s*$')

    objects = {} # obj_name -> {category: bytes}
    sections_detail = [] # list of (sec_name, addr, size, obj_name, category)
    symbols = [] # list of (sym_name, addr, size, obj_name, category)
    
    pending_sec_name = None
    
    with open(map_path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            # Check for top-level section
            m_top = re_top_sec.match(line)
            if m_top:
                sec_name = m_top.group(1)
                current_major_sec = sec_name
                pending_sec_name = None
                continue
            
            # Check for wrapped section name
            m_wrap = re_wrapped_sec_name.match(line)
            if m_wrap:
                pending_sec_name = m_wrap.group(1)
                continue
            
            # Check for wrapped section value
            if pending_sec_name:
                m_val = re_wrapped_sec_val.match(line)
                if m_val:
                    addr = int(m_val.group(1), 16)
                    size = int(m_val.group(2), 16)
                    obj = m_val.group(3).strip()
                    sec_cat = get_sec_category(current_major_sec)
                    
                    if size > 0 and not obj.startswith("*fill*") and not obj.startswith("ABSOLUTE") and not obj.startswith("ALIGN"):
                        obj_clean = obj.replace('\\', '/')
                        if ".pio/build/esp32dev/" in obj_clean:
                            obj_clean = obj_clean.split(".pio/build/esp32dev/")[1]
                        
                        if obj_clean not in objects:
                            objects[obj_clean] = {"flash_rodata": 0, "flash_text": 0, "iram": 0, "dram_data": 0, "dram_bss": 0, "rtc": 0, "other": 0}
                        objects[obj_clean][sec_cat] += size
                        sections_detail.append((pending_sec_name, addr, size, obj_clean, sec_cat))
                    
                    pending_sec_name = None
                    continue
                else:
                    pending_sec_name = None
            
            # Check for single-line section
            m_single = re_single_sec.match(line)
            if m_single:
                s_name = m_single.group(1)
                addr = int(m_single.group(2), 16)
                size = int(m_single.group(3), 16)
                obj = m_single.group(4).strip()
                sec_cat = get_sec_category(current_major_sec)
                
                if size > 0 and not obj.startswith("*fill*") and not obj.startswith("ABSOLUTE") and not obj.startswith("ALIGN"):
                    obj_clean = obj.replace('\\', '/')
                    if ".pio/build/esp32dev/" in obj_clean:
                        obj_clean = obj_clean.split(".pio/build/esp32dev/")[1]
                    
                    if obj_clean not in objects:
                        objects[obj_clean] = {"flash_rodata": 0, "flash_text": 0, "iram": 0, "dram_data": 0, "dram_bss": 0, "rtc": 0, "other": 0}
                    objects[obj_clean][sec_cat] += size
                    sections_detail.append((s_name, addr, size, obj_clean, sec_cat))
                continue
            
            # Check for symbol definition
            m_sym = re_symbol.match(line)
            if m_sym and sections_detail:
                # attribute symbol to the last recorded section if address is close
                sym_addr = int(m_sym.group(1), 16)
                sym_name = m_sym.group(2)
                last_sec = sections_detail[-1]
                if sym_addr >= last_sec[1] and sym_addr < (last_sec[1] + last_sec[2]):
                    symbols.append((sym_name, sym_addr, last_sec[2], last_sec[3], last_sec[4]))
    
    return objects, sections_detail, symbols

def classify_module(obj_name):
    obj = obj_name.lower()
    if 'lost_crown' in obj:
        return "Lost Crown Game"
    elif 'pixel_wars' in obj:
        return "Pixel Wars Game"
    elif 'retro/nes' in obj or 'nestest' in obj or 'romcontra' in obj or 'rommario' in obj:
        return "Retro Arcade (NES)"
    elif 'retro/gb' in obj:
        return "Retro Arcade (Game Boy)"
    elif 'retro/' in obj:
        return "Retro Arcade (Launcher & Common)"
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
    elif 'libwifi' in obj or 'wpa_supplicant' in obj or 'net80211' in obj or 'pp.a' in obj or 'lwip' in obj:
        return "Wi-Fi & Networking Stack (IDF)"
    elif 'freertos' in obj:
        return "FreeRTOS Kernel (IDF)"
    elif 'newlib' in obj or 'libc' in obj or 'libm' in obj or 'gcc' in obj:
        return "C/C++ Standard Runtime (LibC/LibM/LibGCC)"
    elif 'esp32' in obj or 'soc' in obj or 'driver' in obj or 'vfs' in obj or 'spi_flash' in obj or 'hal' in obj or 'heap' in obj or 'nvs' in obj or 'bootloader' in obj or 'esp_hw_support' in obj:
        return "ESP-IDF System & Drivers"
    else:
        return "Other Third-Party / IDF"

def main():
    map_path = r".pio\build\esp32dev\firmware.map"
    objects, sections_detail, symbols = parse_map(map_path)
    
    print(f"Total objects parsed: {len(objects)}")
    print(f"Total section mappings: {len(sections_detail)}")
    print(f"Total symbols attributed: {len(symbols)}")
    
    # Aggregate by Module
    modules = {}
    for obj, sec_dict in objects.items():
        mod = classify_module(obj)
        if mod not in modules:
            modules[mod] = {"flash_rodata": 0, "flash_text": 0, "iram": 0, "dram_data": 0, "dram_bss": 0, "rtc": 0, "objects": []}
        modules[mod]["objects"].append(obj)
        for k in ["flash_rodata", "flash_text", "iram", "dram_data", "dram_bss", "rtc"]:
            modules[mod][k] += sec_dict[k]
    
    print("\n" + "="*120)
    print(f"{'Module / Subsystem':36s} | {'Flash Text':>11s} | {'Flash Rodata':>12s} | {'Total Flash':>12s} | {'Static DRAM':>11s} | {'IRAM':>8s}")
    print("="*120)
    
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
        print(f"{mod:36s} | {d['flash_text']:>9,d} B | {d['flash_rodata']:>10,d} B | {tot_flash:>10,d} B | {tot_dram:>9,d} B | {d['iram']:>6,d} B")
    
    print("="*120)
    print(f"{'TOTAL IDENTIFIED':36s} | {sum(d['flash_text'] for _, d in sorted_mods):>9,d} B | {sum(d['flash_rodata'] for _, d in sorted_mods):>10,d} B | {total_flash_all:>10,d} B | {total_dram_all:>9,d} B | {total_iram_all:>6,d} B")
    print("="*120)

    print("\n=== TOP 30 LARGEST OBJECT FILES BY TOTAL FLASH (Text + Rodata) ===")
    sorted_objs = sorted(objects.items(), key=lambda x: (x[1]["flash_text"] + x[1]["flash_rodata"]), reverse=True)
    for obj, d in sorted_objs[:30]:
        tot = d["flash_text"] + d["flash_rodata"]
        dram = d["dram_data"] + d["dram_bss"]
        print(f"{obj:65s} | Flash: {tot:>9,d} B (rodata: {d['flash_rodata']:>9,d} B, text: {d['flash_text']:>7,d} B) | DRAM: {dram:>7,d} B")

    print("\n=== TOP 25 LARGEST OBJECT FILES BY STATIC DRAM (Data + BSS) ===")
    sorted_dram_objs = sorted(objects.items(), key=lambda x: (x[1]["dram_data"] + x[1]["dram_bss"]), reverse=True)
    for obj, d in sorted_dram_objs[:25]:
        tot_dram = d["dram_data"] + d["dram_bss"]
        print(f"{obj:65s} | DRAM: {tot_dram:>7,d} B (data: {d['dram_data']:>6,d} B, bss: {d['dram_bss']:>6,d} B)")

    # Save detailed data for further inspection
    import json
    with open("scratch/parsed_map_data.json", "w") as jf:
        json.dump({"modules": modules, "objects": objects}, jf, indent=2)

if __name__ == "__main__":
    main()

