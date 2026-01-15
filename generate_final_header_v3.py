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
    
    # Process 128 patches
    # 32 nibbles per patch (16 params * 2)
    # Total bytes read should be 4096 (128*32)
    
    stride = 32
    
    for i in range(128):
        offset = i * stride
        if offset + 32 <= len(raw_data):
            nibbles = raw_data[offset : offset + 32]
            
            # Recombine nibbles: High << 4 | Low
            # Assuming byte order in hex file: HighNibble, LowNibble for each param
            combined_params = []
            for j in range(16):
                high = nibbles[2*j]
                low = nibbles[2*j+1]
                val = (high << 4) | (low & 0x0F)
                combined_params.append(val)
            
            # Append Switch Bytes (Defaults)
            # SW1: 0x32 (Saw ON, Range 8', Chorus OFF)
            # SW2: 0x08 (HPF 2, VCA Env)
            combined_params.append(0x32)
            combined_params.append(0x08)
            
            patches.append((names[i], combined_params))
        else:
            # Fallback
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
        f.write(' * Reconstructed from nibbilized hex dump.\n')
        f.write(' */\n')
        f.write('static const FactoryPresetData junoFactoryPresets[] = {\n')
        
        for name, data in patches:
            hex_str = ",".join([f"0x{b:02X}" for b in data])
            f.write(f'    {{"{name}", {{{hex_str}}}}},\n')
            
        f.write('};\n')
        
    print(f"Generated header with {len(patches)} patches.")

if __name__ == '__main__':
    generate_header()
