#!/usr/bin/env python3
"""
Test DCB Corrector Impact on Decoded Tape Patches
==================================================
Replicates the C++ `correctDcbFormat()` logic in Python to measure
how many patches become DCB-valid after correction.

Correction rules applied:
1. SW2 reserved bits cleared (Juno-60: bits 2,5,6,7; Juno-106: bits 5,6,7)
2. SW1 bit 7 cleared for Juno-106 (reserved)
3. SW1 range bits (0-2) fixed for Juno-60: if none or multiple -> default to 8'

Usage:
    python scripts/test_dcb_corrector.py
    python scripts/test_dcb_corrector.py --tape "JUNO-60 Bank A"
"""

import sys, os
sys.path.insert(0, 'scripts')
import importlib.util

# Load visualize_tape module
spec = importlib.util.spec_from_file_location('vis', 'scripts/visualize_tape.py')
vis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vis)

# Load validate_dcb module for validation functions
spec2 = importlib.util.spec_from_file_location('vdcb', 'scripts/validate_dcb.py')
vdcb = importlib.util.module_from_spec(spec2)
spec2.loader.exec_module(vdcb)

# ---------------------------------------------------------------
# Replicate C++ correctDcbFormat() logic in Python
# ---------------------------------------------------------------

def correct_dcb_python(patches, baud):
    """Replicate C++ JunoTapeDecoder::correctDcbFormat() exactly.
    
    Args:
        patches: list of 18-byte patches (as bytes objects)
        baud: 340 = Juno-60, 1200 = Juno-106
    
    Returns:
        list of corrected 18-byte patches (same length as input)
    """
    is_juno60 = (baud == 340)
    sw2_mask = 0x1B if is_juno60 else 0x1F  # J60->0x1B, J106->0x1F
    corrected = []
    
    for patch in patches:
        p = bytearray(patch)  # mutable copy
        
        # Step 1: Clear SW2 reserved bits
        p[17] &= sw2_mask
        
        # Step 2: For Juno-106, clear SW1 bit 7
        if not is_juno60:
            p[16] &= 0x7F
        
        # Step 3: Fix SW1 range bits (bits 0-2) for BOTH formats
        # Both Juno-60 and Juno-106 have a single range switch —
        # only ONE range can be active (16', 8', or 4').
        range_bits = p[16] & 0x07
        if range_bits == 0 or (range_bits & (range_bits - 1)) != 0:
            # No range or multiple ranges -> default to 8' (bit 1)
            p[16] = (p[16] & 0xF8) | (1 << 1)
        
        corrected.append(bytes(p))
    
    return corrected


def format_bitfield(byte_val, bit_names, format_name=""):
    """Pretty-print byte as bitfield with named bits."""
    lines = []
    for bit_num, name in sorted(bit_names.items()):
        is_set = (byte_val >> bit_num) & 1
        if is_set:
            lines.append(f"        bit {bit_num}: {name} = 1")
    if not lines:
        lines.append("        (no bits set)")
    return '\n'.join(lines)


def count_byte_diffs(before, after):
    """Count how many bytes differ between two 18-byte patches."""
    return sum(1 for a, b in zip(before, after) if a != b)


def analyze_sw1_fixes(original, corrected, label=""):
    """Analyze what was fixed in SW1."""
    ob = original[16]
    cb = corrected[16]
    diffs = []
    
    # Range bits
    o_range = ob & 0x07
    c_range = cb & 0x07
    if o_range != c_range:
        rnames = {1: "16'", 2: "8'", 4: "4'"}
        or_name = rnames.get(o_range, f"0x{o_range:01X}")
        cr_name = rnames.get(c_range, f"0x{c_range:01X}")
        if o_range == 0:
            diffs.append(f"  SW1 range: NONE -> {cr_name}")
        elif bin(o_range).count('1') > 1:
            ranges = []
            if ob & 1: ranges.append("16'")
            if ob & 2: ranges.append("8'")
            if ob & 4: ranges.append("4'")
            diffs.append(f"  SW1 range: MULTIPLE ({'+'.join(ranges)}) -> {cr_name}")
    
    # Bit 7 (Juno-106 reserved)
    if (ob ^ cb) & 0x80:
        diffs.append(f"  SW1 bit 7: 1 -> 0 (reserved)")
    
    return diffs


def analyze_sw2_fixes(original, corrected, is_juno60):
    """Analyze what was fixed in SW2."""
    ob = original[17]
    cb = corrected[17]
    diffs = []
    
    reserved = {2, 5, 6, 7} if is_juno60 else {5, 6, 7}
    rnames = {2: "bit 2 (reserved)", 5: "bit 5 (reserved)", 
              6: "bit 6 (reserved)", 7: "bit 7 (reserved)"}
    
    for bit in sorted(reserved):
        if (ob >> bit) & 1:
            diffs.append(f"  SW2 {rnames[bit]}: 1 -> 0")
    
    return diffs


# ---------------------------------------------------------------
# Main
# ---------------------------------------------------------------

ALL_TAPES = [
    ('docs/Juno-60 (1)/JUNO-60 Bank A.wav', 0, 'JUNO-60 Bank A'),
    ('docs/Juno-60 (1)/JUNO-60 Bank B.wav', 0, 'JUNO-60 Bank B'),
    ('docs/JUNO-106/JUNO106 Bank A.wav', 0, 'JUNO-106 Bank A'),
    ('docs/JUNO-106/JUNO106 Bank B.wav', 0, 'JUNO-106 Bank B'),
    ('docs/JUNO-106/Roland Juno-60 factory programs group 1.wav', 0, 'Juno-60 G1'),
    ('docs/JUNO-106/Roland Juno-60 factory programs group 2.wav', 0, 'Juno-60 G2'),
    ('JUNO106/original/tapes/roland_juno106_factory/j106ma.wav', 0, 'j106ma'),
    ('JUNO106/original/tapes/roland_juno106_factory/j106mb.wav', 0, 'j106mb'),
]


def decode_tape(path, baud, label):
    """Decode a tape using the Python pipeline and return list of 18-byte patches."""
    r, sr, ch = vis.load_wav(path)
    r2, sr2 = vis.preprocess(r, sr, ch)
    
    if baud == 0:
        detected = vis.detect_format(r2, sr2)
        hint = detected if detected else 1200
        best_result = None
        best_baud = 0
        for try_baud in [hint, 1200 if hint == 340 else 340]:
            res = vis.decode_fsk(r2, sr2, try_baud, fast=True)
            if res and res['num_patches'] > (best_result['num_patches'] if best_result else -1):
                best_result = res
                best_baud = try_baud
        result = best_result
        baud = best_baud
    else:
        result = vis.decode_fsk(r2, sr2, baud, fast=True)
    
    if not result or not result['validated']:
        return [], baud, sr2
    
    patches = []
    for pi in range(result['num_patches']):
        off = pi * 18
        data = bytes(result['validated'][off:off+18])
        patches.append(data)
    
    return patches, baud, sr2


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Test DCB Corrector impact on decoded tape patches")
    parser.add_argument('--tape', type=str, default=None, help='Filter by tape name substring')
    parser.add_argument('--baud', type=int, default=0, choices=[0, 340, 1200], help='Force baud rate')
    parser.add_argument('--show-fixes', type=int, default=0, help='Show detailed fix analysis for first N patches per tape')
    args = parser.parse_args()
    
    tapes = ALL_TAPES
    if args.tape:
        tapes = [(p, b, l) for p, b, l in tapes if args.tape in l]
    
    print("=" * 80)
    print("  DCB CORRECTOR IMPACT ANALYSIS")
    print("  Replicates C++ correctDcbFormat() on Python-decoded patches")
    print("=" * 80)
    print()
    
    total_before = 0
    total_after = 0
    total_patches = 0
    total_corrected = 0  # patches where at least one byte changed
    
    results = []
    
    for path, baud, label in tapes:
        if not os.path.isfile(path):
            print(f"  [SKIP] {label}: file not found: {path}")
            continue
        
        patches, baud_used, sr = decode_tape(path, baud, label)
        if not patches:
            print(f"  [SKIP] {label}: no patches decoded")
            continue
        
        is_j60 = (baud_used == 340)  # Use baud rate, not label heuristic!
        fmt_label = "Juno-60" if is_j60 else "Juno-106"
        
        # Count structurally DCB-valid patches BEFORE correction
        before_structural = 0
        before_fully = 0
        for pi, pdata in enumerate(patches):
            r = vdcb.validate_patch(pdata, pi, label, is_j60)
            if r["structurally_valid"]:
                before_structural += 1
            if r["fully_valid"]:
                before_fully += 1
        
        # Apply correction (same logic as C++ correctDcbFormat)
        corrected = correct_dcb_python(patches, baud_used)
        
        # Count structurally DCB-valid patches AFTER correction
        after_structural = 0
        after_fully = 0
        corrected_count = 0
        fix_details = []
        
        for pi, (pdata, cdata) in enumerate(zip(patches, corrected)):
            r = vdcb.validate_patch(cdata, pi, label, is_j60)
            if r["structurally_valid"]:
                after_structural += 1
            if r["fully_valid"]:
                after_fully += 1
            
            byte_diffs = count_byte_diffs(pdata, cdata)
            if byte_diffs > 0:
                corrected_count += 1
                
                if args.show_fixes > 0 and pi < args.show_fixes:
                    sw1_fixes = analyze_sw1_fixes(pdata, cdata, label)
                    sw2_fixes = analyze_sw2_fixes(pdata, cdata, is_j60)
                    fix_details.append((pi, pdata, cdata, sw1_fixes, sw2_fixes, byte_diffs))
        
        total_before += before_structural
        total_after += after_structural
        total_patches += len(patches)
        total_corrected += corrected_count
        
        # Print per-tape summary
        print(f"  [{label}] ({fmt_label}, {baud_used} baud, {len(patches)} patches)")
        print(f"    Structural (HARD):  {before_structural:3d} -> {after_structural:3d}  "
              f"(+{after_structural - before_structural:2d}, "
              f"{after_structural * 100.0 / max(len(patches), 1):5.1f}%)")
        print(f"    Fully (HARD+SOFT):  {before_fully:3d} -> {after_fully:3d}  "
              f"(+{after_fully - before_fully:2d})")
        pattern_only = after_structural - after_fully
        if pattern_only > 0:
            print(f"    Pattern-only warnings:  {pattern_only} patches")
        print(f"    Patches corrected:  {corrected_count}/{len(patches)} "
              f"({corrected_count * 100.0 / max(len(patches), 1):.0f}%)")
        
        # Show fix details for first N patches if requested
        if fix_details and (args.show_fixes > 0):
            for pi, pdata, cdata, sw1_fixes, sw2_fixes, bd in fix_details[:args.show_fixes]:
                fmt_sw2_mask = "0x1B" if baud_used == 340 else "0x1F"
                print(f"\n    Patch {pi+1} — {bd} byte(s) changed:")
                for fix in sw1_fixes:
                    print(f"      {fix}")
                for fix in sw2_fixes:
                    print(f"      {fix}")
                # Show hex diff summary
                o_hex = ' '.join(f'{b:02X}' for b in pdata)
                c_hex = ' '.join(f'{b:02X}' for b in cdata)
                print(f"      Before: {o_hex}")
                print(f"      After:  {c_hex}")
        
        # Count specific fix types
        total_sw2_fixes = 0
        total_sw1_range = 0
        total_sw1_bit7 = 0
        for pi, (pdata, cdata) in enumerate(zip(patches, corrected)):
            for fix in analyze_sw1_fixes(pdata, cdata, label):
                if "range" in fix:
                    total_sw1_range += 1
                if "bit 7" in fix:
                    total_sw1_bit7 += 1
            for fix in analyze_sw2_fixes(pdata, cdata, is_j60):
                total_sw2_fixes += 1
        
        results.append({
            'label': label,
            'fmt': fmt_label,
            'patches': len(patches),
            'before_structural': before_structural,
            'after_structural': after_structural,
            'pct_after': after_structural * 100.0 / max(len(patches), 1),
            'corrected': corrected_count,
            'sw2_fixes': total_sw2_fixes,
            'sw1_range': total_sw1_range,
            'sw1_bit7': total_sw1_bit7,
        })
        
        # Issue count summary
        print(f"    SW2 reserved bits cleared: {total_sw2_fixes}")
        print(f"    SW1 range bits fixed:      {total_sw1_range}")
        print(f"    SW1 bit 7 cleared:         {total_sw1_bit7}")
        print()
    
    # Grand summary
    print("=" * 80)
    print("  GRAND SUMMARY")
    print("=" * 80)
    print()
    print(f"  {'Tape':30s}  {'Patches':>7}  {'Struct Bef':>10}  {'Struct Aft':>10}  {'%Struct':>8}  {'Fixed':>6}")
    print(f"  {'-'*30}  {'-'*7}  {'-'*10}  {'-'*10}  {'-'*8}  {'-'*6}")
    
    grand_before = 0
    grand_after = 0
    grand_total = 0
    
    for r in results:
        grand_total += r['patches']
        grand_before += r['before_structural']
        grand_after += r['after_structural']
        print(f"  {r['label']:30s}  {r['patches']:>7d}  {r['before_structural']:>10d}  {r['after_structural']:>10d}  {r['pct_after']:>7.1f}%  {r['corrected']:>6d}")
    
    print(f"  {'-'*30}  {'-'*7}  {'-'*10}  {'-'*10}  {'-'*8}  {'-'*6}")
    grand_pct = grand_after * 100.0 / max(grand_total, 1)
    print(f"  {'TOTAL':30s}  {grand_total:>7d}  {grand_before:>10d}  {grand_after:>10d}  {grand_pct:>7.1f}%  {'':>6s}")
    print()
    print(f"  Structurally valid (HARD): {grand_before} -> {grand_after} patches "
          f"(+{grand_after - grand_before}, {grand_pct:.1f}%)")
    
    if grand_before > 0:
        improvement = (grand_after - grand_before) * 100.0 / grand_before
        print(f"  Relative improvement: +{improvement:.0f}%")
    
    print()
    print(f"  Fix type totals across all tapes:")
    print(f"    SW2 reserved bits cleared: {sum(r['sw2_fixes'] for r in results)}")
    print(f"    SW1 range bits fixed:      {sum(r['sw1_range'] for r in results)}")
    print(f"    SW1 bit 7 cleared (J-106): {sum(r['sw1_bit7'] for r in results)}")
    print("=" * 80)


if __name__ == '__main__':
    main()
