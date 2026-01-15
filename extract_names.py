import re

def extract_names(filepath):
    names = []
    # Pattern to find strings like "A11 Brass 1" or "B88 ..."
    # Lines in names.txt look like: "4: 3: [ ... 41 31 31 ... ]" -> ASCII dump at the end?
    # Actually, looking at the file content:
    # "3: ... [ ... 41 31 31 20 42 72 61 73 73 20]" -> A11 Brass 
    # "4: Set 1 [... 65 ...]" -> ASCII at the end.
    # We can just rely on the fact that names.txt has lines with ascii representations.
    # Better: finding lines with "A\d\d " or "B\d\d " in the ASCII part.
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Simple regex to find A11..B88 followed by text
    # A11 Brass 1
    pattern = re.compile(r'([AB][1-8][1-8]\s+[A-Za-z0-9\s\.\-\&]+)')
    
    # We might need to go line by line to be safe about order
    found_names = []
    lines = content.split('\n')
    for line in lines:
        # Extract the content inside [] if possible, or usually the text is shown after ]?
        # In the provided view: "3: ... [ bytes ]"
        # Wait, the view showed "41 31 31 ..." inside the brackets?
        # line 4: `... [ ... 41 31 31 ... ]`
        # 41=A, 31=1, 31=1
        # So the HEX inside [] is the name.
        
        # Let's clean the line
        match = re.search(r'\[(.*?)\]', line)
        if match:
            hex_bytes_str = match.group(1).replace(' ', '')
            try:
                # Convert hex string to bytes
                # The hex bytes are space separated in the file "21 db ..."
                # remove spaces
                hex_str = match.group(1).replace(' ', '')
                # if odd length (sometimes occurs in dumps), ignore or fix?
                if len(hex_str) % 2 != 0: continue
                
                data = bytes.fromhex(hex_str)
                # Decode to ascii, ignore errors
                text = data.decode('utf-8', errors='ignore')
                
                # Check if it looks like a preset name (Start with A11.. or just assume anything 16 chars?)
                # Actually, names.txt seems to have garbage too. "Library", "Factory Patches"
                if re.search(r'^[AB][1-8][1-8]\s', text):
                     found_names.append(text.strip())
            except:
                pass

    return found_names

names = extract_names(r'd:\desarrollos\ABDJUNiO601\names.txt')
print(f"Found {len(names)} names.")
for i, name in enumerate(names[:10]):
    print(f"{i}: {name}")
if len(names) < 128:
    print("Warning: Found fewer than 128 names.")
