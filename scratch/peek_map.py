with open(".pio/build/esp32dev/firmware.map", "r", encoding="utf-8", errors="ignore") as f:
    lines = []
    found = False
    for line in f:
        if ".flash.rodata" in line:
            found = True
        if found:
            lines.append(line)
            if len(lines) > 40:
                break

for l in lines:
    print(repr(l))

