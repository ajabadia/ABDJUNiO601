import sys

def parse_hex_file(filepath):
    binary_data = bytearray()
    with open(filepath, 'r') as f:
        for line in f:
            # Format: 0000\t01 02 ... \tASCII
            parts = line.split('\t')
            if len(parts) >= 2:
                hex_part = parts[1].strip().split('  ')[0] # Handle the double space separator before ASCII if present, or just take the middle
                # Actually, the file view showed: "0000\t01 01 ... 0a  ................"
                # It's safer to just take the first 48 chars or split by spaces strictly
                hex_tokens = hex_part.split()
                for token in hex_tokens:
                    try:
                        binary_data.append(int(token, 16))
                    except ValueError:
                        pass
    return binary_data

filepath = r'D:\desarrollos\ABDJUNiO601\JUNO106\original\sysex\hex\original_bank.hex'
data = parse_hex_file(filepath)
print(f"Total binary size: {len(data)} bytes")

# Try to split into 32-byte chunks and print first few
for i in range(0, min(len(data), 160), 32):
    chunk = data[i:i+32]
    print(f"Patch {i//32}: {list(chunk)}")
    # Valid ASCII?
    chars = "".join([chr(b) if 32 <= b <= 127 else '.' for b in chunk])
    print(f"ASCII: {chars}")

# Check for pattern of Names
# Juno-106 names are usually 10 chars?
