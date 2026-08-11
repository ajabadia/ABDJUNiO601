import os

def get_encoding_info(path):
    with open(path, 'rb') as f:
        head = f.read(4)
    if head.startswith(b'\xef\xbb\xbf'): return "UTF-8 SIG"
    if head.startswith(b'\xff\xfe'): return "UTF-16 LE"
    if head.startswith(b'\xfe\xff'): return "UTF-16 BE"
    return "Unknown/ASCII/UTF-8"

current = r"d:\desarrollos\ABDJUNiO601\Source\BinaryData.cpp"
backup = r"d:\desarrollos\ABDJUNiO601\Backup_Sprint8_v101\Source\Core_Backup\BinaryData.cpp"

if os.path.exists(current):
    print(f"Current: {get_encoding_info(current)} (Size: {os.path.getsize(current)})")
else:
    print("Current NOT FOUND")

if os.path.exists(backup):
    print(f"Backup: {get_encoding_info(backup)} (Size: {os.path.getsize(backup)})")
else:
    print("Backup NOT FOUND")
