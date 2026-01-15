
import os

path = r'JUNO106\patches\factory_patches\factory patches.106'
if not os.path.exists(path):
    print(f"File not found: {path}")
    exit(1)

with open(path, 'rb') as f:
    data = f.read()

header = data[:6]
print('Header:', header)
if header != b'!j106\\':
    print('Unexpected header')

offset = 6
entries = []
# Bank A and B tags
tags = [f"A{i}{j}" for i in range(1,9) for j in range(1,9)] + [f"B{i}{j}" for i in range(1,9) for j in range(1,9)]

while offset < len(data):
    # find null terminator within next 32 bytes for name
    end = data.find(b'\x00', offset, offset+32)
    if end == -1:
        name = ""
        offset += 16 # Assume fixed name len if no null?
    else:
        name = data[offset:end].decode('utf-8', errors='ignore')
        offset = end + 1
    
    if offset + 18 > len(data):
        break
        
    patch_bytes = data[offset:offset+18]
    entries.append((name, patch_bytes))
    offset += 18

print('Total entries:', len(entries))

with open('Source/Core/FactoryPresets.h', 'w', encoding='utf-8') as f:
    f.write('#pragma once\n')
    f.write('#include "SynthParams.h"\n')
    f.write('#include <JuceHeader.h>\n\n')
    f.write('struct JunoPatch {\n')
    f.write('    const char* name;\n')
    f.write('    uint8_t lfoRate;\n')
    f.write('    uint8_t lfoDelay;\n')
    f.write('    uint8_t lfoToDCO;\n')
    f.write('    uint8_t pwm;\n')
    f.write('    uint8_t noise;\n')
    f.write('    uint8_t vcfFreq;\n')
    f.write('    uint8_t resonance;\n')
    f.write('    uint8_t envAmount;\n')
    f.write('    uint8_t lfoToVCF;\n')
    f.write('    uint8_t kybdTracking;\n')
    f.write('    uint8_t vcaLevel;\n')
    f.write('    uint8_t attack;\n')
    f.write('    uint8_t decay;\n')
    f.write('    uint8_t sustain;\n')
    f.write('    uint8_t release;\n')
    f.write('    uint8_t subOsc;\n')
    f.write('    uint8_t sw1;\n')
    f.write('    uint8_t sw2;\n')
    f.write('};\n\n')
    f.write('// [AUDIT] Authentic Roland Juno-106 Factory ROM Patches (128)\n')
    f.write('inline const JunoPatch junoFactoryPatches[128] = {\n')
    
    for i in range(128):
        if i < len(entries):
            name, p = entries[i]
            # If name doesn't start with bank tag, add it
            tag = tags[i]
            if not name.startswith(tag):
                name = f"{tag} {name}".strip()
            
            p_hex = [f"0x{b:02X}" for b in p]
            f.write(f'    {{"{name}", {", ".join(p_hex)}}},\n')
        else:
            # Fallback
            f.write(f'    {{"Empty", 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},\n')
            
    f.write('};\n')

print(f"Updated FactoryPresets.h with {min(128, len(entries))} patches.")
