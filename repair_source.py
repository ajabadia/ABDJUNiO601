import os
import shutil

source_dir = r"d:\desarrollos\ABDJUNiO601\Source"
backup_dir = r"d:\desarrollos\ABDJUNiO601\Backup_Sprint8_v95\Source"

def is_corrupted(path):
    try:
        with open(path, 'r', encoding='utf-8-sig') as f:
            content = f.read(500)
            # If the file contains a lot of CJK characters but it's a C++ file, it's corrupted.
            cjk_count = sum(1 for c in content if ord(c) > 0x4E00 and ord(c) < 0x9FFF)
            if cjk_count > 10: # Threshold
                return True
    except:
        return True # Can't read as utf-8? Might be corrupted.
    return False

print(f"Repairing Source from Backup...")

repaired_count = 0
for root, dirs, files in os.walk(source_dir):
    for filename in files:
        if filename.endswith(('.h', '.cpp', '.hpp', '.c')):
            path = os.path.join(root, filename)
            if is_corrupted(path):
                # Try to find in backup
                relative_path = os.path.relpath(path, source_dir)
                backup_path = os.path.join(backup_dir, relative_path)
                
                if os.path.exists(backup_path):
                    print(f"Restoring {relative_path} from backup...")
                    shutil.copy2(backup_path, path)
                    repaired_count += 1
                else:
                    print(f"WARNING: Corrupted file {relative_path} not found in backup!")

print(f"Repair complete. Restored {repaired_count} files.")
