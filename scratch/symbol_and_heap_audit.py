import os
import subprocess
import re
import json

def run_cmd(cmd):
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, shell=True)
    return res.stdout

def main():
    elf = r".pio\build\esp32dev\firmware.elf"
    nm = r"C:\Users\Deep\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-nm.exe"
    cxxfilt = r"C:\Users\Deep\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-c++filt.exe"

    print("Running nm -S --size-sort on firmware.elf...")
    nm_out = run_cmd(f'"{nm}" -S --size-sort "{elf}"')
    
    symbols_by_type = {
        'rodata': [], # 'r', 'R'
        'text': [],   # 't', 'T'
        'data': [],   # 'd', 'D'
        'bss': [],    # 'b', 'B'
        'iram': [],   # located in iram address range 0x40080000 - 0x400a0000
    }
    
    for line in nm_out.splitlines():
        parts = line.strip().split()
        if len(parts) >= 4:
            addr_str, size_str, sym_type, name = parts[0], parts[1], parts[2], parts[3]
            addr = int(addr_str, 16)
            size = int(size_str, 16)
            
            # Check IRAM address
            if 0x40080000 <= addr < 0x400a0000 and sym_type in ('t', 'T'):
                symbols_by_type['iram'].append((name, size, addr, sym_type))
            elif sym_type in ('r', 'R'):
                symbols_by_type['rodata'].append((name, size, addr, sym_type))
            elif sym_type in ('t', 'T'):
                symbols_by_type['text'].append((name, size, addr, sym_type))
            elif sym_type in ('d', 'D'):
                symbols_by_type['data'].append((name, size, addr, sym_type))
            elif sym_type in ('b', 'B'):
                symbols_by_type['bss'].append((name, size, addr, sym_type))

    # Demangle top symbols
    def demangle(name):
        return run_cmd(f'"{cxxfilt}" "{name}"').strip()

    print("\n" + "="*80)
    print("TOP 30 LARGEST CONSTANTS / ASSETS (.flash.rodata)")
    print("="*80)
    symbols_by_type['rodata'].sort(key=lambda x: x[1], reverse=True)
    for name, sz, addr, t in symbols_by_type['rodata'][:30]:
        dem = demangle(name)
        print(f"0x{addr:08x} | {sz:>10,d} B | {dem[:65]}")

    print("\n" + "="*80)
    print("TOP 30 LARGEST STATIC DRAM VARIABLES (.dram0.bss + .dram0.data)")
    print("="*80)
    dram_syms = symbols_by_type['bss'] + symbols_by_type['data']
    dram_syms.sort(key=lambda x: x[1], reverse=True)
    for name, sz, addr, t in dram_syms[:30]:
        dem = demangle(name)
        print(f"0x{addr:08x} | {sz:>10,d} B [{t}] | {dem[:65]}")

    print("\n" + "="*80)
    print("TOP 25 LARGEST CODE FUNCTIONS (.flash.text)")
    print("="*80)
    symbols_by_type['text'].sort(key=lambda x: x[1], reverse=True)
    for name, sz, addr, t in symbols_by_type['text'][:25]:
        dem = demangle(name)
        print(f"0x{addr:08x} | {sz:>10,d} B | {dem[:65]}")

    print("\n" + "="*80)
    print("TOP 20 LARGEST IRAM RESIDENT FUNCTIONS (.iram0.text)")
    print("="*80)
    symbols_by_type['iram'].sort(key=lambda x: x[1], reverse=True)
    for name, sz, addr, t in symbols_by_type['iram'][:20]:
        dem = demangle(name)
        print(f"0x{addr:08x} | {sz:>10,d} B | {dem[:65]}")

    # Save symbol summary to scratch/symbols_summary.json
    out_data = {
        'top_rodata': [{'name': demangle(x[0]), 'raw': x[0], 'size': x[1], 'addr': hex(x[2])} for x in symbols_by_type['rodata'][:100]],
        'top_dram': [{'name': demangle(x[0]), 'raw': x[0], 'size': x[1], 'addr': hex(x[2]), 'type': x[3]} for x in dram_syms[:100]],
        'top_text': [{'name': demangle(x[0]), 'raw': x[0], 'size': x[1], 'addr': hex(x[2])} for x in symbols_by_type['text'][:100]],
        'top_iram': [{'name': demangle(x[0]), 'raw': x[0], 'size': x[1], 'addr': hex(x[2])} for x in symbols_by_type['iram'][:50]],
    }
    with open("scratch/symbols_summary.json", "w") as f:
        json.dump(out_data, f, indent=2)

if __name__ == "__main__":
    main()

