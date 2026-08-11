
import sys
import io

# Set encoding for stdout to utf-8
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

def read_utf16(file_path):
    try:
        with open(file_path, 'r', encoding='utf-16') as f:
            lines = f.readlines()
            for line in lines:
                sys.stdout.write(line)
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        read_utf16(sys.argv[1])
    else:
        print("Usage: python read_utf16.py <file_path>")
