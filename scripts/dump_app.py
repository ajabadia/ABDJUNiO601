import os
import sys

path = os.path.join('WebUI', 'src', 'App.jsx')
try:
    with open(path, 'r', encoding='utf-8') as f:
        print(f.read())
except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
