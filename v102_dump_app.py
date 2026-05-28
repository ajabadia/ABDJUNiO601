import os

path = os.path.join('WebUI', 'src', 'App.jsx')
try:
    with open(path, 'r', encoding='utf-8') as f:
        print("--- APP START ---")
        print(f.read())
        print("--- APP END ---")
except Exception as e:
    print(f"Error: {e}")
