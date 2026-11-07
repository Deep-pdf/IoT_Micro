import os
import subprocess
import glob

def run_cmd(cmd):
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, shell=True)
    return res.stdout

def main():
    elf_path = r".pio\build\esp32dev\firmware.elf"
    map_path = r".pio\build\esp32dev\firmware.map"
    bin_path = r".pio\build\esp32dev\firmware.bin"
    size_tool = r"C:\Users\Deep\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-size.exe"
    readelf_tool = r"C:\Users\Deep\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-readelf.exe"

    print("=== FIRMWARE SIZE (xtensa-esp32-elf-size -A) ===")
    out = run_cmd(f'"{size_tool}" -A "{elf_path}"')
    print(out)

    print("\n=== FIRMWARE SIZE (Berkeley format) ===")
    out2 = run_cmd(f'"{size_tool}" "{elf_path}"')
    print(out2)

    print(f"\nPhysical bin size: {os.path.getsize(bin_path):,d} bytes")

    print("\n=== COMPILED OBJECT FILES IN .pio/build/esp32dev/src ===")
    obj_files = []
    for r, dirs, files in os.walk(r".pio\build\esp32dev\src"):
        for f in files:
            if f.endswith(".o"):
                p = os.path.join(r, f)
                rel = os.path.relpath(p, r".pio\build\esp32dev\src")
                sz = os.path.getsize(p)
                obj_files.append((rel, sz, p))
    obj_files.sort(key=lambda x: x[1], reverse=True)
    print(f"Total .o files in src: {len(obj_files)}")
    for rel, sz, p in obj_files[:30]:
        print(f"{rel:50s} {sz:>10,d} B")

if __name__ == "__main__":
    main()

