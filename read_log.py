import sys

filename = r"d:\desarrollos\ABDSimpleJuno106\my_build_log_clean.txt"
try:
    with open(filename, 'r', encoding='utf-16') as f:
        for line in f:
            if "error" in line.lower():
                print(line.strip())
except Exception as e:
    print(f"Error reading file: {e}")
