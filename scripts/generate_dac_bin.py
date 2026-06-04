#!/usr/bin/env python3
"""
Generate kV4Hz.bin — raw binary dump of kr106::kV4Hz[4096] doubles.

Usage:
    python scripts/generate_dac_bin.py

Output:
    Source/Synth/kV4Hz.bin   (4096 * 8 = 32768 bytes)
"""

import re
import struct
import sys
import os

HEADER_PATH = os.path.join(os.path.dirname(__file__), '..', 'Source', 'Synth', 'J106DACHzTable.h')
OUTPUT_PATH = os.path.join(os.path.dirname(__file__), '..', 'Source', 'Synth', 'kV4Hz.bin')

def main():
    with open(HEADER_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    # Find the kV4Hz array: look for `kV4Hz[kV4HzSize] = {` then extract all float/double literals
    # Pattern: match the opening brace and capture everything until the closing brace
    array_match = re.search(
        r'inline\s+constexpr\s+double\s+kV4Hz\s*\[kV4HzSize\]\s*=\s*\{(.*?)\};',
        content, re.DOTALL
    )
    if not array_match:
        print("ERROR: Could not find kV4Hz array in", HEADER_PATH, file=sys.stderr)
        sys.exit(1)

    values_str = array_match.group(1)
    
    # Extract all numeric literals (including those with trailing 'f' suffixes)
    # Handle both float (5.7339f) and double (5.7339) literals
    num_pattern = re.findall(r'[-+]?\d+\.?\d*(?:[eE][-+]?\d+)?(?:f|F)?', values_str)
    
    values = []
    for v in num_pattern:
        v_clean = v.rstrip('fF')
        try:
            values.append(float(v_clean))
        except ValueError:
            print(f"WARNING: Could not parse '{v}'", file=sys.stderr)
            continue

    if len(values) != 4096:
        print(f"ERROR: Expected 4096 doubles, got {len(values)}", file=sys.stderr)
        sys.exit(1)

    # Write as raw binary (little-endian IEEE 754 doubles)
    with open(OUTPUT_PATH, 'wb') as f:
        f.write(struct.pack(f'<{len(values)}d', *values))

    file_size = os.path.getsize(OUTPUT_PATH)
    print(f"OK: Generated {OUTPUT_PATH}")
    print(f"   {len(values)} doubles, {file_size} bytes ({file_size/1024:.1f} KB)")
    print(f"   First: {values[0]}, Last: {values[-1]}")
    
    # Verify roundtrip
    with open(OUTPUT_PATH, 'rb') as f:
        verify = struct.unpack(f'<{len(values)}d', f.read())
    assert all(abs(a - b) < 1e-10 for a, b in zip(values, verify)), "Roundtrip verification FAILED!"
    print("   OK: Roundtrip verification passed")

if __name__ == '__main__':
    main()
