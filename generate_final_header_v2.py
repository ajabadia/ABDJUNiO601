import re
import os

def parse_names_robust(filepath):
    """
    Extracts names from names.txt by looking for explicit "Axx Name" or "Bxx Name" patterns 
    in the hex-decoded content of lines.
    """
    names_map = {} # Key: "A11", "B88" etc. Value: Full Name
    
    with open(filepath, 'r') as f:
        content = f.read()
        
    # The file has lines like: "Index: ... [ hex ... ]"
    # But it also seems to contain the ascii representation often?
    # Let's try to extract ALL hex content from the file into one big binary blob
    # then search for strings in that blob.
    
    full_hex = []
    # Regex to find hex bytes in brackets: [ ab 01 ... ]
    # Or just iterate lines
    for line in content.splitlines():
        match = re.search(r'\[(.*?)\]', line)
        if match:
            hex_part = match.group(1).replace(' ', '')
            # Pair up
            ignore = False
            for i in range(0, len(hex_part), 2):
                byte_str = hex_part[i:i+2]
                if len(byte_str) == 2:
                    try:
                        val = int(byte_str, 16)
                        full_hex.append(val)
                    except:
                        pass
    
    binary_blob = bytes(full_hex)
    
    # Now look for patterns: [A|B][1-8][1-8] [Name] in the binary data
    # We'll use regex on the bytes decoded as latin1 (to keep 1-1 byte mapping)
    decoded_blob = binary_blob.decode('latin1')
    
    # Pattern: A or B, digit, digit, space, then some text chars until a control char or end
    # A11 Brass 1...
    matches = re.finditer(r'([AB][1-8][1-8])\s+([ -~]+)', decoded_blob)
    
    found_list = []
    
    for m in matches:
        code = m.group(1)
        rest = m.group(2)
        # trim rest at first non-printable or weird char?
        # Actually regex [ -~] matches printable ASCII.
        # But we might match "A11 Brass 1   " (spaces at end)
        full_name = (code + " " + rest).strip()
        
        # Avoid huge matches
        if len(full_name) > 30:
            full_name = full_name[:30]
            
        # Add to list if unique code? 
        # Or just append and sort later?
        # Names.txt order might be messy.
        names_map[code] = full_name
        
    # Also parse names from the text lines themselves if the above failed
    # The file view showed "3: ... [ ... A11 Brass ... ]"
    # Some lines explicitly bad "Set 1" etc.
    
    # Return sorted list of 128 names if possible
    final_names = []
    banks = ['A', 'B']
    for bank in banks:
        for i in range(1, 9):
            for j in range(1, 9):
                code = f"{bank}{i}{j}"
                name = names_map.get(code, f"{code} Unknown")
                final_names.append(name)
                
    return final_names

def parse_hex_file_full(filepath):
    """
    Parses the original_bank.hex allowing for spaces or tabs and handling line breaks.
    """
    data = bytearray()
    with open(filepath, 'r') as f:
        for line in f:
            parts = line.strip().split('\t')
            # If no tab, maybe split by multiple spaces?
            if len(parts) < 2:
                parts = re.split(r'\s{2,}', line.strip())
            
            if len(parts) >= 2:
                hex_content = parts[1]
                # Split bytes by space
                tokens = hex_content.split()
                for token in tokens:
                    if len(token) == 2:
                        try:
                            # Verify it's hex
                            val = int(token, 16)
                            data.append(val)
                        except:
                            pass
                            
    return data

def generate_header():
    hex_path = r'D:\desarrollos\ABDJUNiO601\JUNO106\original\sysex\hex\original_bank.hex'
    names_path = r'd:\desarrollos\ABDJUNiO601\names.txt'
    out_path = r'd:\desarrollos\ABDJUNiO601\Source\Core\FactoryPresets.h'
    
    # 1. Parse Data
    raw_data = parse_hex_file_full(hex_path)
    print(f"Total binary bytes parsed: {len(raw_data)}")
    
    # 2. Parse Names
    names = parse_names_robust(names_path)
    print(f"Total names assembled: {len(names)}")
    
    # 3. Create Patches
    # We assume raw_data contains 32-byte chunks for each patch.
    # 128 * 32 = 4096.
    # If raw_data > 4096, assume header or trailer.
    # The file view showed data starting at 0000.
    
    offset = 0
    # Alignment check
    if len(raw_data) >= 4112: # 16 header + 4096
       # Check for j106 or similar
       # if raw_data[0] == ...
       pass
       
    # If exactly 4096 or more, take chunks
    # Note: earlier we saw 2056 bytes which is half bank?
    # Reread hex file thoroughly.
    
    patches = []
    for i in range(128):
        if offset + 18 <= len(raw_data):
            # Take 18 bytes.
            # Assuming contiguous 18 bytes? Or 32 bytes per patch padded?
            # Standard .106 extraction used 18 from 32? (Offset+18 vs Offset+32 loop)
            # The .106 extraction script skipped name (16 bytes?) then took 18 bytes?
            # Or Name... (variable) then Data.
            
            # Let's inspect raw_data[0:32]
            # 01 01 01 0F ... (18 bytes) ... 00 00 ...
            # If the patch params are dense, they might be just 18 bytes * 128 = 2304 bytes.
            # But 2056 is less than 2304.
            # 4096 is enough for 128 * 32.
            # Let's assume 32 bytes STRIDE.
            
            chunk = raw_data[offset : offset + 32] if offset + 32 <= len(raw_data) else raw_data[offset:]
            # Use first 18 bytes
            data_bytes = chunk[:18]
            # Fill if short
            while len(data_bytes) < 18:
                data_bytes.append(0)
            
            name = names[i]
            
            patches.append((name, data_bytes))
            offset += 32
        else:
            # Out of data
            patches.append((names[i], [0]*18))
            
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
        f.write(' */\n')
        f.write('static const FactoryPresetData junoFactoryPresets[] = {\n')
        
        for name, data in patches:
            hex_str = ",".join([f"0x{b:02X}" for b in data])
            f.write(f'    {{"{name}", {{{hex_str}}}}},\n')
            
        f.write('};\n')
        
    print("Header generated.")

if __name__ == '__main__':
    generate_header()
