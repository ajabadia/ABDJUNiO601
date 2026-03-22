
import sys

def filter_errors(file_path):
    try:
        with open(file_path, 'r', encoding='utf-16') as f:
            lines = f.readlines()
            for line in lines:
                l = line.strip()
                if 'error' in l.lower() or 'failed' in l.lower() or 'C2039' in l or 'C2065' in l:
                    print(l)
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        filter_errors(sys.argv[1])
    else:
        print("Usage: python find_errors.py <file_path>")
