#!/usr/bin/env python3
"""
Fix bridge files:
1. Remove KEEP_INLINE functions (menuAction, serviceAction, setBrowserData, uiReady, exportBank, importBank) from BridgeActions.cpp
2. Update BridgeActions.h declarations to match actual .cpp signatures
3. Update BridgeImport.h declarations to match actual .cpp signatures
4. Fix [this] captures in internal lambdas of remaining extracted functions

KEEP_INLINE functions stay in WebViewEditor.cpp and are NOT in the bridge files.
"""

import re
import os

PROJECT_ROOT = "D:/desarrollos/ABDSynths/ABDJUNiO601"

KEEP_INLINE = {"serviceAction", "menuAction", "uiReady", "exportBank", "importBank", "setBrowserData"}

# ===== STEP 1: Remove KEEP_INLINE functions from BridgeActions.cpp =====

def remove_inline_functions_from_cpp():
    path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeActions.cpp")
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find and remove each KEEP_INLINE function
    for name in KEEP_INLINE:
        # Find the function start
        pattern = f'// {name}'
        fn_start = content.find(f'\n// {name}')
        if fn_start == -1:
            # Try without newline
            fn_start = content.find(f'// {name}')
        if fn_start == -1:
            print(f"  WARNING: Could not find start of '{name}' in BridgeActions.cpp")
            continue
        
        # Find the function end by scanning for the next comment separator or end of namespace
        search_from = fn_start + len(f'// {name}')
        end_pos = search_from
        
        # Find the next function start (// =====) or end of namespace
        next_fn = content.find('\n// =', end_pos)
        if next_fn == -1:
            # This might be the last function - find end of namespace
            ns_end = content.find('} // namespace BridgeActions', end_pos)
            if ns_end != -1:
                end_pos = ns_end
            else:
                end_pos = len(content)
        else:
            end_pos = next_fn
        
        # Remove from fn_start to end_pos
        removed = content[fn_start:end_pos]
        content = content[:fn_start] + content[end_pos:]
        print(f"  Removed '{name}' ({len(removed)} chars)")
    
    # Also remove empty lines at the end of the namespace before }
    content = re.sub(r'\n{3,}', '\n\n', content)
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"\nBridgeActions.cpp: KEEP_INLINE functions removed")


# ===== STEP 2: Extract function signatures from .cpp =====

def extract_cpp_signatures(content):
    """Extract function signatures from .cpp file content."""
    # Find all function definitions in the namespace
    # Pattern: void name(\n    params,\n    ...\n    completion)\n{
    
    func_pattern = re.compile(
        r'void\s+(\w+)\s*\('
        r'([^}]*?)'  # params (non-greedy, up to the closing paren before {)
        r'\)\s*\{',
        re.DOTALL
    )
    
    signatures = {}
    pos = 0
    while True:
        match = func_pattern.search(content, pos)
        if not match:
            break
        
        name = match.group(1)
        params_str = match.group(2).strip()
        
        # Clean up params - remove extra whitespace and format nicely
        param_lines = []
        for line in params_str.split('\n'):
            stripped = line.strip()
            if stripped:
                # Skip [this] capture if present (it's in the lambda, not the function)
                param_lines.append(stripped)
        
        signatures[name] = param_lines
        pos = match.end()
    
    return signatures


def generate_header_declaration(name, params):
    """Generate a header declaration for a function."""
    lines = [f'void {name}(']
    for i, param in enumerate(params):
        prefix = '               ' if i > 0 else '                 '
        comma = ',' if i < len(params) - 1 else ');'
        lines.append(f'{prefix}{param}{comma}')
    return '\n'.join(lines)


# ===== STEP 3: Update BridgeActions.h =====

def update_bridge_actions_h():
    cpp_path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeActions.cpp")
    h_path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeActions.h")
    
    with open(cpp_path, 'r', encoding='utf-8') as f:
        cpp_content = f.read()
    
    with open(h_path, 'r', encoding='utf-8') as f:
        h_content = f.read()
    
    # Extract signatures from .cpp
    cpp_sigs = extract_cpp_signatures(cpp_content)
    
    print(f"\nExtracted {len(cpp_sigs)} signatures from BridgeActions.cpp:")
    for name in sorted(cpp_sigs.keys()):
        print(f"  {name}: {len(cpp_sigs[name])} params")
    
    # Build new header content
    # Keep the header guard and includes, replace function declarations
    
    # Find the start of namespace block
    ns_start = h_content.find('namespace BridgeActions {')
    if ns_start == -1:
        print("ERROR: Could not find namespace BridgeActions in .h")
        return
    
    # Find end of namespace
    ns_end = h_content.find('} // namespace BridgeActions', ns_start)
    if ns_end == -1:
        print("ERROR: Could not find end of namespace")
        return
    
    # Generate new declarations from .cpp signatures (exclude KEEP_INLINE)
    declarations = []
    for name in sorted(cpp_sigs.keys()):
        if name in KEEP_INLINE:
            continue
        params = cpp_sigs[name]
        declarations.append(generate_header_declaration(name, params))
    
    # Build new namespace content
    new_ns_content = 'namespace BridgeActions {\n\n'
    new_ns_content += '\n\n'.join(declarations)
    new_ns_content += '\n\n} // namespace BridgeActions'
    
    # Replace
    new_h = h_content[:ns_start] + new_ns_content + h_content[ns_end + len('} // namespace BridgeActions'):]
    
    with open(h_path, 'w', encoding='utf-8') as f:
        f.write(new_h)
    print(f"\nBridgeActions.h updated with {len(declarations)} declarations")


# ===== STEP 4: Update BridgeImport.h =====

def update_bridge_import_h():
    cpp_path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeImport.cpp")
    h_path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeImport.h")
    
    with open(cpp_path, 'r', encoding='utf-8') as f:
        cpp_content = f.read()
    
    with open(h_path, 'r', encoding='utf-8') as f:
        h_content = f.read()
    
    # Extract signatures
    cpp_sigs = extract_cpp_signatures(cpp_content)
    
    print(f"\nExtracted {len(cpp_sigs)} signatures from BridgeImport.cpp:")
    for name in sorted(cpp_sigs.keys()):
        print(f"  {name}: {len(cpp_sigs[name])} params")
    
    # Build new header content
    ns_start = h_content.find('namespace BridgeImport {')
    if ns_start == -1:
        print("ERROR: Could not find namespace BridgeImport in .h")
        return
    
    ns_end = h_content.find('} // namespace BridgeImport', ns_start)
    if ns_end == -1:
        print("ERROR: Could not find end of namespace")
        return
    
    declarations = []
    for name in sorted(cpp_sigs.keys()):
        params = cpp_sigs[name]
        declarations.append(generate_header_declaration(name, params))
    
    new_ns_content = 'namespace BridgeImport {\n\n'
    new_ns_content += '\n\n'.join(declarations)
    new_ns_content += '\n\n} // namespace BridgeImport'
    
    new_h = h_content[:ns_start] + new_ns_content + h_content[ns_end + len('} // namespace BridgeImport'):]
    
    with open(h_path, 'w', encoding='utf-8') as f:
        f.write(new_h)
    print(f"\nBridgeImport.h updated with {len(declarations)} declarations")


# ===== STEP 5: Fix [this] captures in internal lambdas =====

def fix_internal_this_captures():
    """Remove [this] captures from extracted functions."""
    path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeActions.cpp")
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    count = content.count('[this]')
    print(f"Found {count} [this] captures in BridgeActions.cpp")
    
    content = content.replace('[this]', '[&]')
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)


if __name__ == "__main__":
    print("=" * 60)
    print("STEP 1: Remove KEEP_INLINE functions from BridgeActions.cpp")
    print("=" * 60)
    remove_inline_functions_from_cpp()
    
    print("\n" + "=" * 60)
    print("STEP 2: Update BridgeActions.h to match .cpp")
    print("=" * 60)
    update_bridge_actions_h()
    
    print("\n" + "=" * 60)
    print("STEP 3: Update BridgeImport.h to match .cpp")
    print("=" * 60)
    update_bridge_import_h()
    
    print("\n" + "=" * 60)
    print("STEP 4: Fix [this] captures")
    print("=" * 60)
    fix_internal_this_captures()
    
    print("\nDone!")
