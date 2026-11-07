import os
import re
import json

def main():
    with open("scratch/symbols_summary.json", "r") as f:
        sym_data = json.load(f)
        
    top_rodata = sym_data["top_rodata"]
    top_dram = sym_data["top_dram"]
    
    print("=== EMBEDDED ASSETS & LARGE DATA ARRAYS IN FIRMWARE ===")
    
    # Scan include/ and src/ for array definitions
    array_defs = [] # (file, name, type, count, bytes_approx, line_no)
    re_array = re.compile(r'(const\s+)?(uint8_t|uint16_t|uint32_t|unsigned char|char|byte|Quote)\s+(PROGMEM\s+)?([A-Za-z0-9_]+)\s*\[([^\]]*)\]\s*(=|\{)')
    
    dirs_to_scan = ["include", "src"]
    for d in dirs_to_scan:
        for r, _, files in os.walk(d):
            for fn in files:
                if fn.endswith(('.h', '.cpp', '.c', '.S')):
                    fp = os.path.join(r, fn)
                    rel = os.path.relpath(fp, ".")
                    with open(fp, 'r', encoding='utf-8', errors='ignore') as f:
                        for l_idx, line in enumerate(f):
                            m = re_array.search(line)
                            if m:
                                is_const = bool(m.group(1) or m.group(3))
                                t_name = m.group(2)
                                arr_name = m.group(4)
                                sz_expr = m.group(5).strip()
                                
                                elem_sz = 1
                                if t_name in ('uint16_t', 'unsigned short'): elem_sz = 2
                                elif t_name in ('uint32_t', 'unsigned int'): elem_sz = 4
                                elif t_name == 'Quote': elem_sz = 8 # struct Quote { const char* text; ... }
                                
                                try:
                                    cnt = int(sz_expr)
                                    b_sz = cnt * elem_sz
                                except:
                                    cnt = -1
                                    b_sz = -1
                                
                                array_defs.append((rel, arr_name, t_name, cnt, b_sz, is_const, l_idx + 1))
    
    print(f"Total array declarations found in headers/sources: {len(array_defs)}")
    
    # Match array definitions with symbols in firmware
    matched_syms = {}
    for item in top_rodata + top_dram:
        raw_name = item['raw']
        matched_syms[raw_name] = item
        clean_name = item['name'].split('::')[-1]
        matched_syms[clean_name] = item

    print("\n" + "="*110)
    print(f"{'Array / Asset Name':35s} | {'Source Location':35s} | {'Type':10s} | {'Firmware Size':>13s} | Target Region")
    print("="*110)
    
    seen = set()
    for rel, arr_name, t_name, cnt, b_sz, is_const, l_no in array_defs:
        if arr_name in seen:
            continue
        seen.add(arr_name)
        
        # Check if in matched_syms
        sym_info = matched_syms.get(arr_name)
        if sym_info:
            sz_str = f"{sym_info['size']:,d} B"
            region = "DRAM (.data/.bss)" if sym_info.get('type') in ('d', 'D', 'b', 'B') else "Flash (.rodata)"
            if sym_info['size'] > 500: # Only list significant assets
                print(f"{arr_name:35s} | {rel + ':' + str(l_no):35s} | {t_name:10s} | {sz_str:>13s} | {region}")
        elif b_sz > 500:
            sz_str = f"~{b_sz:,d} B"
            region = "Flash (RO)" if is_const else "DRAM"
            print(f"{arr_name:35s} | {rel + ':' + str(l_no):35s} | {t_name:10s} | {sz_str:>13s} | {region} (unmatched sym)")

if __name__ == "__main__":
    main()

