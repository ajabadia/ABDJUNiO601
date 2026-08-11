import os

def check_namespaces(root_dir):
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file.endswith(('.h', '.cpp', '.hpp')):
                path = os.path.join(root, file)
                try:
                    with open(path, 'r', encoding='utf-8-sig') as f:
                        lines = f.readlines()
                    
                    # Count only namespace ABD blocks
                    in_abd = False
                    bracket_balance = 0
                    leak = False
                    
                    content = "".join(lines)
                    if "namespace ABD" in content:
                        # Simple check: total braces in file
                        # (This is naive but handles most simple leak cases)
                        total_open = content.count('{')
                        total_close = content.count('}')
                        if total_open != total_close:
                            print(f"Brace mismatch in {path}: {total_open} opens, {total_close} closes")
                        
                        # More specific check for namespace ABD suffix
                        last_lines = "".join(lines[-10:])
                        if "namespace ABD" in content and "} // namespace ABD" not in last_lines and "} //namespace ABD" not in last_lines:
                             # This might be a false positive if they don't use comments
                             pass

                except Exception as e:
                    print(f"Error reading {path}: {e}")

check_namespaces(r"d:\desarrollos\ABDJUNiO601\Source")
