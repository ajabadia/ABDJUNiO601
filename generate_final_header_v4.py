import re
import os

def parse_names_robust(filepath):
    names_map = {} 
    with open(filepath, 'r') as f:
        content = f.read()
    
    full_hex = []
    for line in content.splitlines():
        match = re.search(r'\[(.*?)\]', line)
        if match:
            hex_part = match.group(1).replace(' ', '')
            for i in range(0, len(hex_part), 2):
                byte_str = hex_part[i:i+2]
                if len(byte_str) == 2:
                    try:
                        val = int(byte_str, 16)
                        full_hex.append(val)
                    except:
                        pass
    
    binary_blob = bytes(full_hex)
    decoded_blob = binary_blob.decode('latin1')
    matches = re.finditer(r'([AB][1-8][1-8])\s+([ -~]+)', decoded_blob)
    
    for m in matches:
        code = m.group(1)
        rest = m.group(2)
        full_name = (code + " " + rest).strip()
        if len(full_name) > 30: full_name = full_name[:30]
        names_map[code] = full_name
        
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
    data = bytearray()
    with open(filepath, 'r') as f:
        for line in f:
            parts = line.strip().split('\t')
            if len(parts) < 2: parts = re.split(r'\s{2,}', line.strip())
            if len(parts) >= 2:
                hex_content = parts[1]
                tokens = hex_content.split()
                for token in tokens:
                    if len(token) == 2:
                        try:
                            val = int(token, 16)
                            data.append(val)
                        except:
                            pass
    return data

def generate_header():
    hex_path = r'D:\desarrollos\ABDJUNiO601\JUNO106\original\sysex\hex\original_bank.hex'
    names_path = r'd:\desarrollos\ABDJUNiO601\names.txt'
    out_path = r'd:\desarrollos\ABDJUNiO601\Source\Core\FactoryPresets.h'
    
    raw_data = parse_hex_file_full(hex_path)
    names = parse_names_robust(names_path)
    
    patches = []
    stride = 32
    
    for i in range(128):
        offset = i * stride
        if offset + 32 <= len(raw_data):
            nibbles = raw_data[offset : offset + 32]
            
            combined_params = []
            for j in range(16):
                high = nibbles[2*j]
                low = nibbles[2*j+1]
                # Combine and Mask to 7-bit
                val = ((high << 4) | (low & 0x0F)) & 0x7F
                combined_params.append(val)
            
            # Switch Defaults: 
            # Saw ON (0x10), Range 8' (0x02) = 0x12?
            # Previous logic: 0011 0010 = 0x32 (Bit 5 set if Chorus OFF? No bit 5 is Chorus ON (0) in logic, wait.)
            # Logic: bool chorusOn = (sw1 & (1 << 5)) == 0;
            # So if I want Chorus OFF, bit 5 must be 1.
            # 0x20.
            # Saw On (Bit 4): 0x10.
            # Pulse Off (Bit 3): 0.
            # Range 8' (Bit 1): 0x02.
            # Total SW1: 0x20 | 0x10 | 0x02 = 0x32. Correct.
            
            combined_params.append(0x32)
            combined_params.append(0x08)
            
            patches.append((names[i], combined_params))
        else:
            patches.append((names[i], [0]*18))
            
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
        f.write(' * 32-byte nibble dump -> 16 bytes + defaults.\n')
        f.write(' */\n')
        f.write('static const FactoryPresetData junoFactoryPresets[] = {\n')
        
        for name, data in patches:
            hex_str = ",".join([f"0x{b:02X}" for b in data])
            f.write(f'    {{"{name}", {{{hex_str}}}}},\n')
            
        f.write('};\n')
        
    print(f"Generated header with {len(patches)} valid patches.")

if __name__ == '__main__':
    generate_header()
