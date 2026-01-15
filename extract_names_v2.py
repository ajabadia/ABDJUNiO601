import re

def extract_names(filepath):
    found_names = []
    
    with open(filepath, 'r') as f:
        # Read line by line
        for line_num, line in enumerate(f):
            # Format: "index: ... [hex bytes]"
            match = re.search(r'\[(.*?)\]', line)
            if match:
                hex_str = match.group(1).replace(' ', '')
                if len(hex_str) % 2 != 0: continue
                try:
                    data = bytes.fromhex(hex_str)
                    # Decode with replace to handle non-ascii
                    text = data.decode('utf-8', errors='replace')
                    
                    # Look for pattern "A11 Name" or "B88 Name"
                    # Juno names are usually 10-14 chars?
                    # Regex: [AB][1-8][1-8] followed by space and characters
                    name_match = re.search(r'([AB][1-8][1-8]\s+[\w\s\.\-\&\*\(\)\/]+)', text)
                    if name_match:
                        # Clean up the name
                        raw_name = name_match.group(1).strip()
                        # Ensure reasonable length
                        if len(raw_name) > 3:
                            found_names.append(raw_name)
                except:
                    pass

    # Sort or Dedup?
    # names.txt seems to have sequential entries.
    # But let's just return list and see count.
    return found_names

names = extract_names(r'd:\desarrollos\ABDJUNiO601\names.txt')
print(f"Found {len(names)} names.")
# Print first and last few to verify range
if names:
    print("First 5:", names[:5])
    print("Last 5:", names[-5:])

# Check for duplicates or missing
# We expect A11..A18, A21..A88, B11..B88
expected_count = 128
if len(names) != 128:
    print(f"Warning: Expected 128 names, found {len(names)}")
