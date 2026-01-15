import sys

def convert_hex(input_file, output_file):
    binary_data = bytearray()
    with open(input_file, 'r') as f:
        for line in f:
            # Format: 0000\t01 02 ...
            parts = line.strip().split('\t')
            if len(parts) > 1:
                hex_part = parts[1]
                # Hex part might have extra text at the end (ASCII representation)
                # Usually separated by some space. But in the view: "01 02 ... 0a   ...."
                # We can just take the first 48 chars (16 bytes * 3 chars) or regex
                # Or just split by spaces and take items that are valid hex bytes
                tokens = hex_part.split()
                count = 0
                for token in tokens:
                    if len(token) == 2:
                        try:
                            val = int(token, 16)
                            binary_data.append(val)
                            count += 1
                        except:
                            pass
                    if count >= 16: break
    
    with open(output_file, 'wb') as out:
        out.write(binary_data)
    print(f"Converted {len(binary_data)} bytes to {output_file}")

convert_hex(r'D:\desarrollos\ABDJUNiO601\JUNO106\original\sysex\hex\original_bank.hex', r'd:\desarrollos\ABDJUNiO601\original_bank.bin')
