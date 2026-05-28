import os
import sys

cwd = os.getcwd()
print(f"CWD: {cwd}")
path = os.path.join(cwd, 'WebUI', 'src', 'App.jsx')
print(f"Checking Path: {path}")
print(f"Exists: {os.path.exists(path)}")

if not os.path.exists(path):
    print("Listing WebUI:")
    if os.path.exists('WebUI'):
        print(os.listdir('WebUI'))
        src_path = os.path.join('WebUI', 'src')
        if os.path.exists(src_path):
             print("Listing WebUI/src:")
             print(os.listdir(src_path))
    else:
        print("WebUI folder not found!")

try:
    with open(path, 'r', encoding='utf-8') as f:
        print("--- CONTENT START ---")
        print(f.read())
        print("--- CONTENT END ---")
except Exception as e:
    print(f"Error: {e}")
