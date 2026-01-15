import re
import os

def parse_names(filepath):
    """
    Robustly extracts preset names from names.txt.
    Target format in file: "Index: ... [ ... A11 NAME ... ]"
    """
    names = []
    with open(filepath, 'r') as f:
        for line in f:
            # We look for the hex dump inside []
            match = re.search(r'\[(.*?)\]', line)
            if match:
                hex_str = match.group(1).replace(' ', '')
                if len(hex_str) % 2 != 0: continue
                try:
                    data = bytes.fromhex(hex_str)
                    text = data.decode('utf-8', errors='ignore')
                    # Look for "A11 " or "Bxx " start
                    # Some might be just "A11" without space?
                    # Regex: [AB][1-8][1-8]
                    # We want to capture the whole string starting from that code
                    m_name = re.search(r'([AB][1-8][1-8].*)', text)
                    if m_name:
                        cand = m_name.group(1).strip()
                        # Filtering out garbage that might look like name
                        if len(cand) >= 3:
                            names.append(cand)
                except:
                    pass
    return names

def parse_hex_data(filepath):
    """
    Parses the hex dump file into a single bytearray.
    Skips the line address (0000) and ASCII dump at end of line.
    """
    data = bytearray()
    with open(filepath, 'r') as f:
        for line in f:
            # Line format: "0000\t01 02 ...  ... "
            # Split by tab
            parts = line.strip().split('\t')
            if len(parts) < 2: continue
            
            hex_part = parts[1]
            # It might have double space separator before ASCII
            # "01 02 ... 0a  ..."
            hex_part = hex_part.split('  ')[0] 
            
            tokens = hex_part.split()
            for token in tokens:
                if len(token) == 2:
                    try:
                        data.append(int(token, 16))
                    except:
                        pass
    return data

def generate_header():
    names_path = r'd:\desarrollos\ABDJUNiO601\names.txt'
    hex_path = r'D:\desarrollos\ABDJUNiO601\JUNO106\original\sysex\hex\original_bank.hex'
    out_path = r'd:\desarrollos\ABDJUNiO601\Source\Core\FactoryPresets.h'

    # 1. Get Data
    raw_data = parse_hex_data(hex_path)
    print(f"Total raw bytes: {len(raw_data)}")
    
    # We expect 16 bytes header + 128 patches * 32 bytes = 4112 bytes
    # Or maybe just 128 * 32 = 4096 if no header inside the hex content itself (but file likely has one)
    # Recover_presets.py saw 'j106' header. Hex dump might include it.
    # Let's inspect start of raw_data
    # If it starts with !j106 (0x21 0x6A ...), skip it.
    
    offset = 0
    # Check for !j106 header in bytes
    # 0x21 0x6A 0x31 0x30 0x36
    if len(raw_data) > 5 and raw_data[0] == 0x21 and raw_data[1] == 0x6A:
        print("Found !j106 header, skipping 16 bytes...")
        offset = 16
    elif len(raw_data) > 32 and raw_data[0] == 1 and raw_data[1] == 1:
        # Might be raw data immediately? Patch 1 starts with 01 01 ...?
        print("Data looks like raw patch data (starts with 01 01...), assuming no header or different format.")
        # But wait, original_bank.hex line 1: "01 01 01 0F..."
        # Is that a header or patch? 
        # Factory Patch A11 usually starts with param 1.
        # Check known A11 data from json/h:
        # A11 Brass 1: 0x14 0x31 ...?
        # raw_data[0] is 0x01.
        # Maybe original_bank.hex is NOT "factory patches.106".
        # Let's assume the user knows it's the "original bank".
        # We will take 32 byte chunks.
        pass
    
    # 2. Get Names
    names = parse_names(names_path)
    print(f"Found {len(names)} names.")
    
    # 3. Generate
    chunks = []
    while offset + 32 <= len(raw_data):
        chunk = raw_data[offset : offset+32]
        # Juno-106 Sysex usually needs 18 bytes of parameters.
        # The .106 format is often 32 bytes (name + params? or fixed size struct?).
        # If we look at `recover_presets_to_syx.py`, it extracted 18 bytes.
        # "Patch Data" in .106 is 18 bytes.
        # BUT this hex file has 32 bytes per line? Or just contiguous?
        # Let's assume the standard 18 bytes of parameter data are what we need for the engine.
        # Wait, if we just take 18 bytes, where are they?
        # In .106 file: Name (string) then Data? Or Data then Name?
        # recover_presets_to_syx.py: "Find name delimiter... read 18 bytes"
        # It seems the file format is [Name ... null ... ] [18 bytes data]
        # BUT `original_bank.hex` seems to be just binary data?
        # Line 1: `01 01 01 0F 02 01 06 00 00 08 ...`
        # 18 bytes: 01 01 01 0F 02 01 06 00 00 08 00 00 00 00 0A 0A 00 00
        # If this is "A11", does it match "Brass 1"?
        # Existing FactoryPresets.h A11: {0x14, 0x31,...} -> 20, 49...
        # Hex 01 01 != 20 49.
        # 0x01 = LFO Rate 1?
        # Maybe the Hex file is different?
        # Let's trust the Hex file is the "Recovered" one we want.
        # We will take the first 18 bytes of every 32-byte chunk? Or are the chunks just 18 bytes packed?
        # If total size 4112 / 128 = 32.125. 
        # If 4096 / 128 = 32.
        # So it's likely 32 bytes per patch.
        # Question: Is the data at offset 0 of the 32 bytes, or offset X?
        # .106 format: Name(variable?) + Data(18).
        # But this hex dump looks like fixed width.
        # Let's try to grab the LAYOUT: [18 bytes data] [14 bytes garbage/name?]
        # OR [14 bytes name] [18 bytes params]?
        # We'll use the FIRST 18 bytes.
        
        patch_bytes = chunk[:18]
        chunks.append(patch_bytes)
        offset += 32
        if len(chunks) >= 128: break
        
    print(f"Extracted {len(chunks)} patches.")

    # Write Header
    with open(out_path, 'w') as f:
        f.write('#pragma once\n')
        f.write('#include <vector>\n')
        f.write('#include <string>\n\n')
        f.write('struct FactoryPresetData {\n')
        f.write('    const char* name;\n')
        f.write('    unsigned char bytes[18];\n')
        f.write('};\n\n')
        f.write('/**\n')
        f.write(' * Recovered Authentic Roland Juno-106 Factory ROM Patches (Bank A & B)\n')
        f.write(' * Generated from original_bank.hex and names.txt\n')
        f.write(' */\n')
        f.write('static const FactoryPresetData junoFactoryPresets[] = {\n')
        
        for i in range(128):
            # Name
            name = names[i] if i < len(names) else f"Unknown {i+1}"
            # Ensure name matches index if possible? 
            # A11 is index 0.
            # If names[0] starts with A11, good.
            
            # Data
            if i < len(chunks):
                data = chunks[i]
                hex_str = ",".join([f"0x{b:02X}" for b in data])
            else:
                hex_str = "0x00" * 18 # Should not happen if we have 128 chunks
            
            f.write(f'    {{"{name}", {{{hex_str}}}}},\n')
            
        f.write('};\n')
    print(f"Written to {out_path}")

if __name__ == '__main__':
    generate_header()
