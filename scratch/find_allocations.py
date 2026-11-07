import os
import re

def analyze_src_allocations():
    src_dir = "src"
    include_dir = "include"
    
    # regex for allocations
    re_malloc = re.compile(r'\b(malloc|calloc|realloc|strdup|ps_malloc|heap_caps_malloc)\s*\(([^)]*)\)')
    re_new = re.compile(r'\bnew\s+([A-Za-z0-9_:]+)(\s*\[([^\]]+)\])?')
    re_free = re.compile(r'\b(free|delete|delete\s*\[\s*\]|heap_caps_free)\s*(\([^)]*\)|[A-Za-z0-9_]+)')
    
    allocations = []
    
    for r, dirs, files in os.walk(src_dir):
        for f in files:
            if f.endswith(('.cpp', '.c', '.h')):
                p = os.path.join(r, f)
                rel = os.path.relpath(p, ".")
                with open(p, 'r', encoding='utf-8', errors='ignore') as fp:
                    lines = fp.readlines()
                
                for idx, line in enumerate(lines):
                    line_str = line.strip()
                    if line_str.startswith("//") or line_str.startswith("/*"):
                        continue
                    
                    # check malloc
                    for m in re_malloc.finditer(line_str):
                        allocations.append((rel, idx + 1, "C-Alloc", m.group(1), m.group(2).strip(), line_str))
                    # check new
                    for m in re_new.finditer(line_str):
                        t_name = m.group(1)
                        if t_name in ('int', 'char', 'uint8_t', 'uint16_t', 'uint32_t', 'float', 'double', 'byte') or m.group(2):
                            size_expr = m.group(3) if m.group(3) else "1 object"
                            allocations.append((rel, idx + 1, "C++ New", f"new {t_name}", size_expr.strip(), line_str))
                        else:
                            allocations.append((rel, idx + 1, "C++ New Object", f"new {t_name}", "object", line_str))

    return allocations

def main():
    allocs = analyze_src_allocations()
    print(f"Total dynamic heap allocation calls found in src: {len(allocs)}\n")
    
    # Group by file/subsystem
    by_file = {}
    for rel, line_no, a_type, func, expr, line_str in allocs:
        if rel not in by_file:
            by_file[rel] = []
        by_file[rel].append((line_no, a_type, func, expr, line_str))
    
    for rel, items in sorted(by_file.items()):
        print(f"=== {rel} ({len(items)} allocation sites) ===")
        for line_no, a_type, func, expr, line_str in items:
            print(f"  Line {line_no:4d} | {a_type:14s} | {func:20s} | Expr: {expr:25s} | {line_str[:55]}")
        print()

if __name__ == "__main__":
    main()

