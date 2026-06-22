import sys

with open('src/desktop/main.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

depth = 0
in_comment = False
for i, line in enumerate(lines):
    if i < 1131: continue # skip before RenderUI
    
    clean_line = ""
    j = 0
    while j < len(line):
        if not in_comment and line[j:j+2] == '/*':
            in_comment = True
            j += 2
        elif in_comment and line[j:j+2] == '*/':
            in_comment = False
            j += 2
        elif not in_comment and line[j:j+2] == '//':
            break
        elif not in_comment:
            clean_line += line[j]
        j += 1
        
    for char in clean_line:
        if char == '{': depth += 1
        elif char == '}': depth -= 1
        
    if depth <= 0 and i > 1133:
        print(f"Line {i+1} hits depth {depth}: {line.strip()}")
        break
