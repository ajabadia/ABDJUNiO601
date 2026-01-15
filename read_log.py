import sys

filename = r"build_debug.txt"
try:
    with open(filename, 'r', encoding='utf-16') as f:
        for line in f:
             # Print lines with error/warning or "declaraci"
            if "error" in line.lower() or "declaraci" in line.lower() or "fallado" in line.lower():
                print(line.strip())
except Exception as e:
    print(f"Error reading file: {e}")
