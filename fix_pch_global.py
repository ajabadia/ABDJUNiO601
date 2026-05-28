import os

def fix_pch_order(file_path):
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        try:
             with open(file_path, 'r', encoding='latin-1') as f:
                lines = f.readlines()
        except:
            return
    
    if len(lines) < 2: return
    
    # Track if we found JuceHeader.h and if it's NOT at the top
    juce_header_idx = -1
    for i, line in enumerate(lines[:10]): # Only check top 10 lines
        if "JuceHeader.h" in line:
            juce_header_idx = i
            break
            
    if juce_header_idx > 0:
        print(f"Fixing {file_path}")
        juce_line = lines.pop(juce_header_idx)
        lines.insert(0, juce_line)
        
        with open(file_path, 'w', encoding='utf-8') as f:
            f.writelines(lines)

def process_dir(target_dir):
    for root, dirs, files in os.walk(target_dir):
        for filename in files:
            if filename.endswith(".cpp"):
                fix_pch_order(os.path.join(root, filename))

process_dir(r"Source")
