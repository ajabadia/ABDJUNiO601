
import os

path = r'd:\desarrollos\ABDJUNiO601\JUNO106\patches\factory_patches\factory patches (with position).106'
with open(path, 'rb') as f:
    data = f.read()

print(f"File size: {len(data)}")

# Search for A11 payload: 14 31 00 66
pattern = b'\x14\x31\x00\x66'
idx = data.find(pattern)

if idx != -1:
    print(f"Found PATTERN at {idx}")
    print(f"Context: {list(data[idx-10:idx+20])}")
    
    # Check if name "A11" is nearby
    # "A11" is '41 31 31'
    name_pat = b'\x41\x31\x31'
    name_idx = data.find(name_pat)
    print(f"Found NAME 'A11' at {name_idx}")
    
    if name_idx != -1:
        print(f"Distance Name to Data: {idx - name_idx}")
else:
    print("Pattern NOT found.")
