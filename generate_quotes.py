import os
import re

def parse_quotes():
    md_path = r'include/maan_ki_baat_quote_library.md'
    if not os.path.exists(md_path):
        print(f"Error: {md_path} not found!")
        return []
    
    quotes = []
    current_category = "General"
    
    with open(md_path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    # Split content by lines
    lines = content.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue
            
        # Check for category header
        if line.startswith('## '):
            category_candidate = line[3:].strip()
            # Stop if we reach instructions section
            if "Adding New Quotes" in category_candidate:
                break
            current_category = category_candidate
            i += 1
            continue
            
        # Check for quote ID header
        if line.startswith('### Q'):
            quote_id = line[3:].strip() # Qxxx
            
            # Read subsequent lines to extract quote text
            i += 1
            quote_text_lines = []
            while i < len(lines):
                next_line = lines[i].strip()
                # Stop if we hit another header
                if next_line.startswith('##') or next_line.startswith('###'):
                    break
                # Ignore separator lines
                if next_line.startswith('---'):
                    i += 1
                    continue
                if next_line:
                    quote_text_lines.append(next_line)
                i += 1
                
            quote_text = " ".join(quote_text_lines).strip()
            if quote_id and quote_text:
                quotes.append({
                    'id': quote_id,
                    'category': current_category,
                    'text': quote_text
                })
            continue
            
        i += 1
        
    return quotes

def generate_header(quotes):
    header_path = r'include/quote_library.h'
    
    # Escape quotes and backslashes for C++ string literals
    def clean_cpp_str(s):
        return s.replace('\\', '\\\\').replace('"', '\\"')
        
    with open(header_path, 'w', encoding='utf-8') as f:
        f.write('// Generated dynamically from maan_ki_baat_quote_library.md. Do not edit directly.\n')
        f.write('#ifndef QUOTE_LIBRARY_H\n')
        f.write('#define QUOTE_LIBRARY_H\n\n')
        f.write('#include <Arduino.h>\n\n')
        
        f.write('struct Quote {\n')
        f.write('  const char* id;\n')
        f.write('  const char* category;\n')
        f.write('  const char* text;\n')
        f.write('};\n\n')
        
        f.write(f'// Total quotes detected: {len(quotes)}\n')
        f.write('const Quote quotes_db[] PROGMEM = {\n')
        for q in quotes:
            clean_id = clean_cpp_str(q['id'])
            clean_cat = clean_cpp_str(q['category'])
            clean_txt = clean_cpp_str(q['text'])
            f.write(f'  {{ "{clean_id}", "{clean_cat}", "{clean_txt}" }},\n')
        f.write('};\n\n')
        
        f.write(f'const size_t quotes_count = {len(quotes)};\n\n')
        f.write('#endif // QUOTE_LIBRARY_H\n')
        
    print(f"Generated {header_path} with {len(quotes)} quotes.")

if __name__ == '__main__':
    # When run under PlatformIO, working directory is project root
    q = parse_quotes()
    generate_header(q)
