
import os

input_path = 'factory_presets.syx'
output_path = 'extracted_syx_patches.txt'

with open(input_path, 'rb') as f:
    data = f.read()

chunks = [data[i:i+24] for i in range(0, len(data), 24)]

with open(output_path, 'w', encoding='utf-8') as f:
    for i, chunk in enumerate(chunks):
        if len(chunk) < 24:
            continue
        # Standard 106 Data is 18 bytes at offset 4 or 5?
        # F0 41 30 ch [18 bytes] ... F7?
        # Or F0 41 30 ch [Format] [18 bytes] ... F7?
        # My previous check showed F0 41 30 00 at start.
        # Chunks are 24 bytes.
        # F0, 41, 30, 00 (4 bytes)
        # 00 00 00 00 07 4C 69 62 72 61 72 79 00 00 00 03 00 00 00 01 F7
        # Wait, that's not 18 bytes.
        
        # Let's just dump the whole hex.
        hex_data = chunk.hex(' ')
        try:
            ascii_data = "".join([chr(b) if 32 <= b <= 126 else "." for b in chunk])
        except:
            ascii_data = ""
        f.write(f"{i:03d}: {hex_data}  | {ascii_data}\n")

print(f"Extracted {len(chunks)} chunks to {output_path}")
