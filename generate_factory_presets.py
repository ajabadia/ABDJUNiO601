import os, sys

# Path to the .106 file containing factory patches
path = r'd:\\desarrollos\\ABDJUNiO601\\JUNO106\\patches\\factory_patches\\factory patches.106'

with open(path, 'rb') as f:
    data = f.read()

header = data[:6]
if header != b'!j106\\':
    print('Unexpected header:', header)
    sys.exit(1)

offset = 6
entries = []
while offset < len(data):
    # Find null terminator for name within next 32 bytes
    end = data.find(b'\x00', offset, offset + 32)
    if end == -1:
        name_bytes = data[offset:offset + 16]
        name = name_bytes.rstrip(b'\x00').decode('utf-8', errors='ignore')
        offset += 16
    else:
        name = data[offset:end].decode('utf-8', errors='ignore')
        offset = end + 1
    if offset + 18 > len(data):
        break
    patch = data[offset:offset + 18]
    offset += 18
    # Keep only entries whose name starts with 'A' (factory presets)
    if name.startswith('A'):
        entries.append((name, patch))
    # Stop when we have 128 factory presets
    if len(entries) >= 128:
        break

# Generate C++ header content
lines = []
lines.append('#pragma once')
lines.append('#include <vector>')
lines.append('#include <cstddef>')
lines.append('')
lines.append('/**')
lines.append(' * Factory presets extracted from the original Juno-106 ROM.')
lines.append(' * Generated automatically from "factory patches.106".')
lines.append(' */')
lines.append('struct FactoryPresetData {')
lines.append('    const char* name;')
lines.append('    unsigned char bytes[18];')
lines.append('};')
lines.append('')
lines.append('static const FactoryPresetData junoFactoryPresets[] = {')
for name, patch in entries:
    hex_bytes = ', '.join(f'0x{b:02X}' for b in patch)
    lines.append(f'    {{"{name}", {{{hex_bytes}}}}},')
lines.append('};')

output_path = r'd:\\desarrollos\\ABDJUNiO601\\Source\\Core\\FactoryPresets.h'
with open(output_path, 'w', encoding='utf-8') as out:
    out.write('\n'.join(lines))
print('FactoryPresets.h written with', len(entries), 'entries')
