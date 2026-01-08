import os, sys, re

path = r'd:\\desarrollos\\ABDJUNiO601\\JUNO106\\patches\\factory_patches\\factory patches.106'
with open(path, 'rb') as f:
    data = f.read()
header = data[:6]
print('Header:', header)
if header != b'!j106\\':
    print('Unexpected header')
offset = 6
entries = []
while offset < len(data):
    # find null terminator within next 32 bytes for name
    end = data.find(b'\x00', offset, offset+32)
    if end == -1:
        name_bytes = data[offset:offset+16]
        name = name_bytes.rstrip(b'\x00').decode('utf-8', errors='ignore')
        offset += 16
    else:
        name = data[offset:end].decode('utf-8', errors='ignore')
        offset = end + 1
    if offset + 18 > len(data):
        break
    patch = data[offset:offset+18]
    entries.append((name, patch))
    offset += 18
print('Total entries:', len(entries))
for i, (name, patch) in enumerate(entries[:10]):
    print(i, name, patch.hex(' '))
