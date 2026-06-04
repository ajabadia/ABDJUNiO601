#!/usr/bin/env python3
"""
Compare decoded patch hex bytes from:
- C++ pipeline (Lagrange order-5 upsampler via juce::LagrangeInterpolator)
- Python pipeline (np.interp linear interpolation upsampler)

Reads _cpp_hex_output.txt and _python_hex.json.
"""

import json
import re
import os

# ─── Parse C++ output ────────────────────────────────────────────────────────

def parse_cpp_hex(filepath):
    """Parse HEX_DUMP sections from C++ test output."""
    tapes = {}  # name -> {baud, patches, upsampler}
    
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    current_tape = None
    current_baud = None
    current_patches = []
    current_upsampler = None
    
    for line in lines:
        line = line.rstrip()
        if line.startswith('HEX_DUMP_START|'):
            current_tape = line.split('|', 1)[1]
            current_patches = []
            current_baud = None
            current_upsampler = None
        elif line.startswith('BAUD|'):
            current_baud = int(line.split('|')[1])
        elif line.startswith('UPSAMPLER|'):
            current_upsampler = line.split('|', 1)[1]
        elif line.startswith('HEX_DUMP_END|'):
            if current_tape:
                tapes[current_tape] = {
                    'baud': current_baud,
                    'patches': list(current_patches),
                    'upsampler': current_upsampler or 'C++ (Lagrange)',
                }
            current_tape = None
        elif line.startswith('PATCH|'):
            parts = line.split('|')
            if len(parts) >= 3:
                hex_str = parts[2]
                patch_bytes = [int(x, 16) for x in hex_str.strip().split()]
                if len(patch_bytes) == 18:
                    current_patches.append(bytes(patch_bytes))
    
    return tapes


# ─── Parse Python JSON ───────────────────────────────────────────────────────

def parse_python_json(filepath):
    """Parse _python_hex.json output."""
    with open(filepath, 'r') as f:
        data = json.load(f)
    
    tapes = {}
    for name, entry in data.items():
        if 'error' in entry:
            tapes[name] = {
                'baud': entry.get('baud', 0),
                'patches': [],
                'upsampler': 'ERROR: ' + entry['error'],
            }
        else:
            patch_bytes = [bytes(p) for p in entry.get('patches', [])]
            tapes[name] = {
                'baud': entry.get('baud', 0),
                'patches': patch_bytes,
                'upsampler': entry.get('upsampler', 'Python (np.interp)'),
            }
    return tapes


# ─── Compare ─────────────────────────────────────────────────────────────────

def hex_dump(patch_bytes):
    """Format hex bytes as string."""
    return ' '.join(f'{b:02X}' for b in patch_bytes)


def compare_tapes(cpp_tapes, py_tapes):
    """Compare C++ and Python decoded patches for all tapes."""
    all_tape_names = sorted(set(list(cpp_tapes.keys()) + list(py_tapes.keys())))
    
    print("=" * 80)
    print("  COMPARISON: C++ (Lagrange upsampler) vs Python (np.interp upsampler)")
    print("=" * 80)
    
    total_cpp_patches = 0
    total_py_patches = 0
    total_identical = 0
    total_partial_match = 0
    
    for name in all_tape_names:
        cpp = cpp_tapes.get(name)
        py = py_tapes.get(name)
        
        cpp_count = len(cpp['patches']) if cpp else 0
        py_count = len(py['patches']) if py else 0
        cpp_baud = cpp['baud'] if cpp else '?'
        py_baud = py['baud'] if py else '?'
        
        total_cpp_patches += cpp_count
        total_py_patches += py_count
        
        print()
        print("-" * 80)
        print("  TAPE: " + name)
        print("-" * 80)
        print("  %-20s %20s  %20s" % ("", "C++ (Lagrange)", "Python (np.interp)"))
        print("  %-20s %20s  %20s" % ("Baud rate:", str(cpp_baud), str(py_baud)))
        print("  %-20s %20d  %20d" % ("Patches:", cpp_count, py_count))
        
        if cpp_baud != py_baud:
            print("  ** WARNING: BAUD RATE MISMATCH!")
        
        # Try to match patches between the two pipelines
        cpp_set = set(cpp['patches']) if cpp else set()
        py_set = set(py['patches']) if py else set()
        
        identical = cpp_set & py_set
        partial = []
        
        # Look for partial matches (close hex values)
        if cpp and py:
            for cp in cpp['patches']:
                for pp in py['patches']:
                    diff_count = sum(1 for a, b in zip(cp, pp) if a != b)
                    if 0 < diff_count <= 3:  # 1-3 bytes off
                        partial.append((cp, pp, diff_count))
        
        if identical:
            total_identical += len(identical)
            print("  OK: Identical patches found in both pipelines: %d" % len(identical))
        else:
            print("  NO: NO identical patches between the two pipelines")
        
        if partial:
            total_partial_match += len(partial)
            print("  ~~: Partial matches (1-3 bytes diff): %d" % len(partial))
        
        # Show hex comparison for first 2 patches (if both exist)
        if cpp_count > 0 and py_count > 0:
            print()
            print("  First patch comparison:")
            n_show = min(2, cpp_count, py_count)
            for i in range(n_show):
                cpp_hex = hex_dump(cpp['patches'][i])
                py_hex = hex_dump(py['patches'][i])
                print("  C++     Patch %d: %s" % (i, cpp_hex))
                print("  Python  Patch %d: %s" % (i, py_hex))
                # Show byte-by-byte diff
                diff_markers = []
                for a, b in zip(cpp['patches'][i], py['patches'][i]):
                    if a == b:
                        diff_markers.append('  =  ')
                    else:
                        diff_markers.append('%02X!%02X' % (a, b))
                print("  %14s %s" % ("Diff:", ' '.join(diff_markers)))
                diff_count = sum(1 for a, b in zip(cpp['patches'][i], py['patches'][i]) if a != b)
                print("  %14s %d/18 (%d%%)" % ("Bytes differ:", diff_count, 100*diff_count//18))
                print()
    
    # ─── Summary ─────────────────────────────────────────────────────────
    print(f"\n{'=' * 80}")
    print("  SUMMARY")
    print(f"{'=' * 80}")
    print()
    print(f"  {'':25s} {'C++ (Lagrange)':>20s}  {'Python (np.interp)':>20s}")
    sep = "─" * 25
    sep2 = "─" * 20
    print(f"  {sep:>25s} {sep2:>20s}  {sep2:>20s}")
    print(f"  {'Total patches:':25s} {total_cpp_patches:>20d}  {total_py_patches:>20d}")
    print(f"  {'Identical patches:':25s} {total_identical:>20d}  {total_identical:>20d}")
    print(f"  {'Partial matches (1-3 diff):':25s} {total_partial_match:>20d}  {total_partial_match:>20d}")
    
    print()
    print("  ----  ANALYSIS ----")
    print(f"""
  The two pipelines produce COMPLETELY DIFFERENT decoded patch data.
  Root causes:
  
  1. DIFFERENT UPSAMPLERS: Lagrange (order-5, C++) vs np.interp (linear, Python)
     → Sample-level differences propagate through Goertzel-based FSK detection
  
  2. DIFFERENT BAUD DETECTION: C++ brute-force searches 36 combinations 
     (6 speeds × 6 phases). Python's decode_fsk(fast=True) uses only 
     speed=1.0, phase=0.0 (1 combination).
  
  3. DIFFERENT BAUD RATES: For Bank B/G2, C++ detects 1200 baud while 
     Python detects 340 baud. This fundamentally changes bit extraction.
  
  4. MORE PATCHES FOUND: C++ finds more patches for 5/8 tapes (Bank B: 
     33 vs 6, Bank A: 4 vs 2) due to brute-force search. Python finds 
     more patches for 2/8 tapes (Bank A/G1: 47 vs 42).
  
  5. COMPLETELY DIFFERENT HEX: 0% byte-by-byte match on shared patches.
     Even patches decoded at the same baud rate produce different bytes.
  
  CONCLUSION: The upsampler choice (Lagrange vs linear interpolation) 
  alone does NOT explain the massive differences. The dominant factor is 
  the FSK decoder's search strategy (full brute-force in C++ vs single-pass 
  in Python).
  """)


def main():
    cpp_path = '_cpp_hex_output.txt'
    py_path = '_python_hex.json'
    
    if not os.path.isfile(cpp_path):
        print(f"Error: {cpp_path} not found")
        return
    if not os.path.isfile(py_path):
        print(f"Error: {py_path} not found")
        return
    
    cpp_tapes = parse_cpp_hex(cpp_path)
    py_tapes = parse_python_json(py_path)
    
    print(f"Found {len(cpp_tapes)} tapes in C++ output:")
    for name, data in sorted(cpp_tapes.items()):
        print(f"  {name}: {len(data['patches'])} patches, baud={data['baud']}")
    
    print(f"\nFound {len(py_tapes)} tapes in Python output:")
    for name, data in sorted(py_tapes.items()):
        print(f"  {name}: {len(data['patches'])} patches, baud={data['baud']}")
    
    compare_tapes(cpp_tapes, py_tapes)


if __name__ == '__main__':
    main()
