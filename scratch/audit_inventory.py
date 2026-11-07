import os
import sys

def main():
    root = os.path.abspath(".")
    print(f"Project root: {root}")
    
    # 1. Scan src
    print("\n=== SCANNING SRC ===")
    src_files = []
    for r, dirs, files in os.walk(os.path.join(root, "src")):
        for f in files:
            p = os.path.join(r, f)
            rel = os.path.relpath(p, root)
            sz = os.path.getsize(p)
            src_files.append((rel, sz))
    
    src_files.sort()
    for rel, sz in src_files:
        print(f"{rel:60s} {sz:>10,d} B")
    print(f"Total src files: {len(src_files)}, Total size: {sum(s for _, s in src_files):,d} B")

    # 2. Scan include
    print("\n=== SCANNING INCLUDE ===")
    inc_files = []
    for r, dirs, files in os.walk(os.path.join(root, "include")):
        for f in files:
            p = os.path.join(r, f)
            rel = os.path.relpath(p, root)
            sz = os.path.getsize(p)
            inc_files.append((rel, sz))
    inc_files.sort()
    for rel, sz in inc_files:
        print(f"{rel:60s} {sz:>10,d} B")
    print(f"Total include files: {len(inc_files)}, Total size: {sum(s for _, s in inc_files):,d} B")

    # 3. Check other root directories
    print("\n=== TOP LEVEL DIRECTORIES ===")
    for item in os.listdir(root):
        p = os.path.join(root, item)
        if os.path.isdir(p):
            total_sz = sum(os.path.getsize(os.path.join(r, f)) for r, _, files in os.walk(p) for f in files)
            file_count = sum(len(files) for r, _, files in os.walk(p))
            print(f"{item:30s} {file_count:>5d} files, {total_sz:>12,d} B")

if __name__ == "__main__":
    main()

