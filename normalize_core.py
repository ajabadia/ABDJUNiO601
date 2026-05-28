import codecs
import os

target_dir = r"d:\desarrollos\ABDJUNiO601\Source\Core"
files = [f for f in os.listdir(target_dir) if f.endswith(('.h', '.cpp'))]

print(f"Normalizing {len(files)} files in {target_dir}...")

for filename in files:
    path = os.path.join(target_dir, filename)
    try:
        # Detect current encoding (check for UTF-16 BOM)
        with open(path, 'rb') as f:
            header = f.read(2)
        
        encoding = 'utf-8' # Default
        if header == b'\xff\xfe' or header == b'\xfe\xff':
            encoding = 'utf-16'
            print(f"Detected UTF-16 for {filename}")
        
        # Read and re-write as UTF-8 with BOM
        with codecs.open(path, 'r', encoding, errors='replace') as f:
            content = f.read()
        
        with codecs.open(path, 'w', 'utf-8-sig') as f:
            f.write(content)
        
    except Exception as e:
        print(f"Failed to process {filename}: {e}")

print("Normalization complete.")
