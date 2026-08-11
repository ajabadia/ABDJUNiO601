import os

def fix_pch_order(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    if len(lines) < 2: return
    
    line1 = lines[0].strip()
    line2 = lines[1].strip()
    
    # If JuceHeader.h is on line 2, and NOT on line 1, swap them
    if "JuceHeader.h" in line2 and "JuceHeader.h" not in line1:
        print(f"Fixing {file_path}")
        lines[0], lines[1] = lines[1], lines[0]
        # Ensure there's a newline at the end if we swapped
        if not lines[0].endswith('\n'): lines[0] += '\n'
        if not lines[1].endswith('\n'): lines[1] += '\n'
        
        with open(file_path, 'w', encoding='utf-8') as f:
            f.writelines(lines)

core_dir = r"Source\Core"
for filename in os.listdir(core_dir):
    if filename.endswith(".cpp"):
        fix_pch_order(os.path.join(core_dir, filename))
