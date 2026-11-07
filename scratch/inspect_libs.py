import os
import glob
import json

def main():
    with open("scratch/parsed_map_data.json", "r") as f:
        data = json.load(f)
        
    objects = data["objects"]
    
    # Categorize libraries by archive or path
    lib_categories = {}
    for obj, sec in objects.items():
        # obj looks like "lib701/libWiFi.a(WiFiGeneric.cpp.o)" or "C:/Users/.../libmbedtls.a(...)"
        archive = "src"
        if obj.startswith("src/"):
            archive = "src (Application Code)"
        elif "(" in obj:
            archive = obj.split("(")[0]
        else:
            archive = obj
            
        if archive not in lib_categories:
            lib_categories[archive] = {"flash_text": 0, "flash_rodata": 0, "iram": 0, "dram_data": 0, "dram_bss": 0, "count": 0}
            
        for k in ["flash_text", "flash_rodata", "iram", "dram_data", "dram_bss"]:
            lib_categories[archive][k] += sec[k]
        lib_categories[archive]["count"] += 1

    print("\n" + "="*110)
    print(f"{'Archive / Component':60s} | {'Flash Total':>11s} | {'Static DRAM':>11s} | {'IRAM':>8s} | Objs")
    print("="*110)
    
    sorted_libs = sorted(lib_categories.items(), key=lambda x: (x[1]["flash_text"] + x[1]["flash_rodata"]), reverse=True)
    for lib, d in sorted_libs:
        tot_flash = d["flash_text"] + d["flash_rodata"]
        tot_dram = d["dram_data"] + d["dram_bss"]
        # shorten long path
        short_lib = lib
        if "tools/sdk/esp32/lib" in short_lib:
            short_lib = "IDF: " + short_lib.split("tools/sdk/esp32/lib/")[1]
        elif "tools/sdk/esp32/ld" in short_lib:
            short_lib = "IDF: " + short_lib.split("tools/sdk/esp32/ld/")[1]
        elif "tools/sdk/esp32/dio_qspi" in short_lib:
            short_lib = "IDF: " + short_lib.split("tools/sdk/esp32/dio_qspi/")[1]
        elif "toolchain-xtensa-esp32" in short_lib:
            short_lib = "GCC: " + os.path.basename(short_lib)
            
        print(f"{short_lib:60s} | {tot_flash:>9,d} B | {tot_dram:>9,d} B | {d['iram']:>6,d} B | {d['count']:>3d}")

if __name__ == "__main__":
    main()

