import os

def recursive_fix_names(target_dir):
    # Walk bottom-up to rename files before parents
    for root, dirs, files in os.walk(target_dir, topdown=False):
        for name in files:
            if '\r' in name:
                old_path = os.path.join(root, name)
                new_name = name.replace('\r', '')
                new_path = os.path.join(root, new_name)
                print(f"Renaming file: {repr(name)} -> {repr(new_name)}")
                os.rename(old_path, new_path)
        
        for name in dirs:
            if '\r' in name:
                old_path = os.path.join(root, name)
                new_name = name.replace('\r', '')
                new_path = os.path.join(root, new_name)
                print(f"Renaming dir: {repr(name)} -> {repr(new_name)}")
                os.rename(old_path, new_path)

recursive_fix_names('WebUI')
