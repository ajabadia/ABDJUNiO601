
import io
import sys

def extract_errors():
    try:
        # Open build_log.txt as UTF-16
        with open('build_log.txt', 'r', encoding='utf-16') as f_in:
            # Open output file as UTF-8
            with open('final_errors_utf8.txt', 'w', encoding='utf-8') as f_out:
                for line in f_in:
                    # Filter for typical error/warning patterns
                    l_lower = line.lower()
                    if 'error' in l_lower or 'fatal' in l_lower or 'failed' in l_lower:
                        f_out.write(line)
        print("Successfully extracted errors to final_errors_utf8.txt")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    extract_errors()
