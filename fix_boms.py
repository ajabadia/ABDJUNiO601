import codecs
import os

target_dir = r"d:\desarrollos\ABDJUNiO601\Source"
extensions = ('.h', '.cpp', '.hpp', '.c')

print(f"Recursively cleaning BOMs in {target_dir}...")

count = 0
for root, dirs, files in os.walk(target_dir):
    for filename in files:
        if filename.endswith(extensions):
            path = os.path.join(root, filename)
            try:
                # Read with utf-8-sig to strip ALL existing BOMs
                # (Actually, utf-8-sig strips one BOM. If there are two, 
                # we might need to read multiple times or check the content)
                
                with open(path, 'rb') as f:
                    data = f.read()
                
                # Manual BOM stripping for double BOMs
                bom = b'\xef\xbb\xbf'
                while data.startswith(bom):
                    data = data[len(bom):]
                
                # Write with single utf-8-sig BOM
                with open(path, 'wb') as f:
                    f.write(bom + data)
                
                count += 1
                
            except Exception as e:
                print(f"Failed to process {path}: {e}")

print(f"BOM cleanup complete. Processed {count} files.")
