import codecs
import os

target_dir = r"d:\desarrollos\ABDJUNiO601\Source"
extensions = ('.h', '.cpp', '.hpp', '.c')

print(f"Recursively normalizing files in {target_dir}...")

count = 0
for root, dirs, files in os.walk(target_dir):
    for filename in files:
        if filename.endswith(extensions):
            path = os.path.join(root, filename)
            try:
                # Detect current encoding (check for UTF-16 BOM)
                with open(path, 'rb') as f:
                    header = f.read(2)
                
                encoding = 'utf-8' # Default
                if header == b'\xff\xfe' or header == b'\xfe\xff':
                    encoding = 'utf-16'
                    # print(f"Detected UTF-16 for {filename}")
                
                # Read and re-write as UTF-8 with BOM
                with codecs.open(path, 'r', encoding, errors='replace') as f:
                    content = f.read()
                
                # Check if it's already utf-8-sig to avoid unnecessary writes
                # (Reading with utf-8-sig codec already strips the BOM if present)
                
                with codecs.open(path, 'w', 'utf-8-sig') as f:
                    f.write(content)
                
                count += 1
                
            except Exception as e:
                print(f"Failed to process {path}: {e}")

print(f"Normalization complete. Processed {count} files.")
