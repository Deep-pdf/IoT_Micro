import os

def main():
    src_dir = "src"
    build_dir = r".pio\build\esp32dev\src"
    
    src_files = []
    for r, dirs, files in os.walk(src_dir):
        for f in files:
            if f.endswith(('.cpp', '.c', '.S')):
                p = os.path.join(r, f)
                rel = os.path.relpath(p, src_dir)
                src_files.append((rel, p))
    
    built_objs = []
    for r, dirs, files in os.walk(build_dir):
        for f in files:
            if f.endswith(('.o')):
                p = os.path.join(r, f)
                rel = os.path.relpath(p, build_dir)
                built_objs.append((rel, p))
    
    print(f"Total compilable source files in src: {len(src_files)}")
    print(f"Total compiled object files in .pio/build: {len(built_objs)}")
    
    # Check matching
    built_rel_set = set(b[0].replace('\\', '/') for b in built_objs)
    
    uncompiled = []
    compiled = []
    for rel, p in src_files:
        obj_name = rel.replace('\\', '/') + ".o"
        if obj_name in built_rel_set:
            compiled.append((rel, p))
        else:
            uncompiled.append((rel, p))
            
    print(f"Compiled source files: {len(compiled)}")
    print(f"Uncompiled source files: {len(uncompiled)}")
    if uncompiled:
        print("\nUncompiled files in src/:")
        for u, _ in uncompiled:
            print(f"  {u}")

if __name__ == "__main__":
    main()

