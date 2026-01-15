import sys

def scan_log():
    try:
        try:
            with open("build_log.txt", "r", encoding="utf-16") as f:
                lines = f.readlines()
        except UnicodeError:
            with open("build_log.txt", "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
        
        print(f"Log has {len(lines)} lines.")
        count = 0
        for line in lines:
            if "error" in line.lower():
                print(line.strip())
                count += 1
                if count > 50: break
                
    except Exception as e:
        print(f"Script Error: {e}")

if __name__ == "__main__":
    scan_log()
