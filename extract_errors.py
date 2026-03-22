
import sys
import io

# Set encoding for stdout to utf-8
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

def extract_errors(file_path):
    try:
        with open(file_path, 'r', encoding='utf-16') as f:
            lines = f.readlines()
            # Find lines containing ": error" (this is the typical MSVC error format)
            for i, line in enumerate(lines):
                if ": error" in line or "fatal error" in line:
                    # Print context: 2 lines before, current line, 7 lines after
                    start = max(0, i - 2)
                    end = min(len(lines), i + 8)
                    print("-" * 40)
                    for k in range(start, end):
                        sys.stdout.write(lines[k])
                    print("-" * 40)
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        extract_errors(sys.argv[1])
    else:
        print("Usage: python extract_errors.py <file_path>")
