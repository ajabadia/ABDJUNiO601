import os, sys, re

# Path to the .106 file containing factory patches
path = r'd:\desarrollos\ABDJUNiO601\JUNO106\patches\factory_patches\factory patches (with position).106'
output_path = r'd:\desarrollos\ABDJUNiO601\Source\Core\FactoryPresets.h'

def clean_name(raw_name):
    # Strip garbage
    clean = "".join(c for c in raw_name if c.isprintable())
    clean = clean.strip()
    return clean

def generate():
    if not os.path.exists(path):
        print(f"Error: Input file not found at {path}")
        return

    with open(path, 'rb') as f:
        data = f.read()

    offset = 6 # Skip !j106\
    entries = []
    
    print(f"Scanning file...")
    
    while offset < len(data):
        # Name
        limit = min(len(data), offset + 64)
        chunk = data[offset:limit]
        null_pos = chunk.find(b'\x00')
        
        if null_pos != -1:
            name_bytes = chunk[:null_pos]
            offset += null_pos + 1
        else:
            name_bytes = chunk[:16] # Fallback
            offset += 16

        # Data
        if offset + 18 > len(data):
            break
        patch_data = data[offset : offset + 18]
        offset += 18
        
        raw_name = name_bytes.decode('utf-8', errors='ignore')
        cname = clean_name(raw_name)
        
        # Filter metadata
        if cname in ["Library", "Factory Patches", "(position)", ""]:
            continue
            
        entries.append((cname, patch_data))
        if len(entries) >= 128:
            break

    print(f"Found {len(entries)} valid presets.")
    
    while len(entries) < 128:
        entries.append(("Empty", bytes([0]*18)))

    lines = []
    lines.append('#pragma once')
    lines.append('#include <JuceHeader.h>')
    lines.append('')
    lines.append('struct JunoPatch {')
    lines.append('    const char* name;')
    lines.append('    uint8_t lfoRate, lfoDelay, lfoToDCO, pwm, noise, vcfFreq, resonance, envAmount, lfoToVCF, kybdTracking, vcaLevel, attack, decay, sustain, release, subOsc, sw1, sw2;')
    lines.append('};')
    lines.append('')
    lines.append('static const JunoPatch junoFactoryPatches[128] = {')

    for name, patch in entries:
        safe_name = name.replace('\\', '\\\\').replace('"', '\\"')
        h = ",".join(f"0x{b:02X}" for b in patch)
        lines.append(f'    {{"{safe_name}", {h}}},')

    lines.append('};')
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f"Generated {output_path}")

if __name__ == "__main__":
    generate()
