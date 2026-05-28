import os

def sanitize_recursively(start_node):
    for root, dirs, files in os.walk(start_node, topdown=False):
        for name in files:
            if '\r' in name or '\n' in name:
                old_path = os.path.join(root, name)
                new_name = name.replace('\r', '').replace('\n', '')
                new_path = os.path.join(root, new_name)
                try:
                    os.rename(old_path, new_path)
                    print(f"File: {repr(name)} -> {repr(new_name)}")
                except Exception as e:
                    print(f"Error file {name}: {e}")
        
        for name in dirs:
            if '\r' in name or '\n' in name:
                old_path = os.path.join(root, name)
                new_name = name.replace('\r', '').replace('\n', '')
                new_path = os.path.join(root, new_name)
                try:
                    os.rename(old_path, new_path)
                    print(f"Dir: {repr(name)} -> {repr(new_name)}")
                except Exception as e:
                    print(f"Error dir {name}: {e}")

# Apply to root to be sure
sanitize_recursively('.')
