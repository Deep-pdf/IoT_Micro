import json

def main():
    with open("scratch/parsed_map_data.json", "r") as f:
        data = json.load(f)
    
    objects = data["objects"]
    linked_src_objs = set()
    for obj in objects:
        if obj.startswith("src/"):
            linked_src_objs.add(obj)
            
    print(f"Total src/ objects linked in firmware.map: {len(linked_src_objs)} / 69")
    
    # Check if any .o in .pio/build/esp32dev/src is not in linked_src_objs
    import os
    all_src_objs = set()
    for r, dirs, files in os.walk(r".pio\build\esp32dev\src"):
        for fn in files:
            if fn.endswith(".o"):
                rel = os.path.relpath(os.path.join(r, fn), r".pio\build\esp32dev").replace('\\', '/')
                all_src_objs.add(rel)
                
    unlinked = all_src_objs - linked_src_objs
    print(f"Unlinked src objects: {len(unlinked)}")
    for u in unlinked:
        print(f"  Unlinked: {u}")

if __name__ == "__main__":
    main()

