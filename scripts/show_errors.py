
import sys
import io

def show_errors():
    # Set stdout to UTF-8
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    
    print("--- Searching in build_log.txt (UTF-16) ---")
    try:
        with open('build_log.txt', 'r', encoding='utf-16') as f:
            count = 0
            for line in f:
                if 'error' in line.lower() or 'fatal' in line.lower():
                    print(line.strip())
                    count += 1
                    if count >= 50: break
    except Exception as e:
        print(f"Error reading build_log.txt: {e}")

    print("\n--- Searching in debug_error.txt (UTF-8) ---")
    try:
        with open('debug_error.txt', 'r', encoding='utf-8', errors='replace') as f:
            count = 0
            for line in f:
                if 'error' in line.lower() or 'fatal' in line.lower():
                    print(line.strip())
                    count += 1
                    if count >= 50: break
    except Exception as e:
        print(f"Error reading debug_error.txt: {e}")

if __name__ == "__main__":
    show_errors()
