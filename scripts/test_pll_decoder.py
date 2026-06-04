#!/usr/bin/env python3
"""
Test PLL-based FSK Decoder vs Existing Goertzel Decoder
=======================================================
Compares patch counts and byte-level agreement between:
- decode_fsk() - existing Goertzel-based decoder (fast=True, single pass)
- decode_fsk_pll() - new PLL-based decoder (quadrature demod + early-late gate)

Usage:
    python scripts/test_pll_decoder.py
    python scripts/test_pll_decoder.py --tape "JUNO-60 Bank A"  # single tape
"""

import sys, os, time, argparse
sys.path.insert(0, 'scripts')
import importlib.util
spec = importlib.util.spec_from_file_location('vis', 'scripts/visualize_tape.py')
vis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vis)

# All reference tapes
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
    """Run both Goertzel and PLL decoders on a tape."""
    r, sr, ch = vis.load_wav(path)
    r2, sr2 = vis.preprocess(r, sr, ch)
    
    if baud == 0:
        # Auto-detect: try both rates, pick the one with more patches (Goertzel result)
        detected = vis.detect_format(r2, sr2)
        hint = detected if detected else 1200
        best_result_g = None
        best_baud = 0
        for try_baud in [hint, 1200 if hint == 340 else 340]:
            res = vis.decode_fsk(r2, sr2, try_baud, fast=True)
            if res and res['num_patches'] > (best_result_g['num_patches'] if best_result_g else -1):
                best_result_g = res
                best_baud = try_baud
        result_g = best_result_g
        baud = best_baud
    else:
        result_g = vis.decode_fsk(r2, sr2, baud, fast=True)
    
    # PLL decoder at same baud rate
    result_p = vis.decode_fsk_pll(r2, sr2, baud)
    
    return {
        'label': label,
        'baud': baud,
        'sr': sr2,
        'goertzel': {
            'patches': result_g['num_patches'] if result_g else 0,
            'bytes': len(result_g['bytes']) if result_g else 0,
            'validated': bytes(result_g['validated']) if result_g and result_g['validated'] else b'',
        },
        'pll': {
            'patches': result_p['num_patches'],
            'bytes': len(result_p['bytes']),
            'validated': bytes(result_p['validated']),
        },
    }


def compare_patches(g_patches, p_patches):
    """Compare validated patches byte-by-byte between two decoders."""
    g_bytes = [g_patches[i:i+18] for i in range(0, len(g_patches), 18)]
    p_bytes = [p_patches[i:i+18] for i in range(0, len(p_patches), 18)]
    
    # Find matching patches (any order)
    g_set = set(g_bytes)
    p_set = set(p_bytes)
    common = g_set & p_set
    
    # Find partial matches (up to N bytes differ)
    partial = 0
    min_diff = 999
    for gb in g_bytes:
        for pb in p_bytes:
            diff = sum(1 for a, b in zip(gb, pb) if a != b)
            if diff < min_diff and diff > 0:
                min_diff = diff
            if 1 <= diff <= 5:
                partial += 1
    
    return {
        'g_count': len(g_bytes),
        'p_count': len(p_bytes),
        'exact_matches': len(common),
        'partial_matches_1to5': partial,
        'min_byte_diff': min_diff if min_diff < 999 else 0,
    }


def main():
    parser = argparse.ArgumentParser(description='Test PLL FSK Decoder vs Goertzel')
    parser.add_argument('--tape', type=str, default=None, help='Filter by tape name substring')
    parser.add_argument('--baud', type=int, default=0, choices=[0, 340, 1200], help='Force baud rate')
    args = parser.parse_args()
    
    tapes = ALL_TAPES
    if args.tape:
        tapes = [(p, b, l) for p, b, l in tapes if args.tape in l]
    
    if not tapes:
        print(f"No tapes match filter: {args.tape}")
        sys.exit(1)
    
    print("=" * 80)
    print("  PLL FSK DECODER vs GOERTZEL DECODER - Comparison")
    print("=" * 80)
    print()
    print(f"  Testing {len(tapes)} tape(s)")
    print()
    
    results = []
    
    for path, baud, label in tapes:
        if not os.path.isfile(path):
            print(f"  [SKIP] {label}: file not found: {path}")
            continue
        
        print(f"  {'-' * 60}")
        print(f"  [{label}]")
        print(f"  {'-' * 60}")
        
        try:
            t0 = time.time()
            r = decode_tape(path, baud, label)
            elapsed = time.time() - t0
            
            g = r['goertzel']
            p = r['pll']
            
            print(f"    Baud rate: {r['baud']}")
            print(f"    Sample rate: {r['sr']} Hz")
            print(f"    Decode time: {elapsed:.1f}s")
            print()
            print(f"    {'Decoder':12s}  {'Patches':>8}  {'Bytes':>8}  {'Validated':>10}")
            print(f"    {'-'*12}  {'-'*8}  {'-'*8}  {'-'*10}")
            print(f"    {'Goertzel':12s}  {g['patches']:>8d}  {g['bytes']:>8d}  {len(g['validated']):>10d}")
            print(f"    {'PLL':12s}  {p['patches']:>8d}  {p['bytes']:>8d}  {len(p['validated']):>10d}")
            
            # Comparison
            cmp = compare_patches(g['validated'], p['validated'])
            print()
            print(f"    Comparison:")
            print(f"      Exact matching patches:  {cmp['exact_matches']}")
            print(f"      Partial matches (1-5B):  {cmp['partial_matches_1to5']}")
            print(f"      Min byte diff:          {cmp['min_byte_diff']}")
            
            # Show first patch from each if available
            if g['patches'] > 0 and p['patches'] > 0:
                g_first = g['validated'][:18]
                p_first = p['validated'][:18]
                
                # Check if first patches are the same
                same = g_first == p_first
                diff_count = sum(1 for a, b in zip(g_first, p_first) if a != b)
                
                print(f"      First patch identical:  {'YES' if same else 'NO'}")
                if not same:
                    print(f"      First patch diff bytes: {diff_count}/18")
                    
                    # Show hex comparison
                    g_hex = ' '.join(f'{b:02X}' for b in g_first)
                    p_hex = ' '.join(f'{b:02X}' for b in p_first)
                    print(f"      Goertzel: {g_hex}")
                    print(f"      PLL:      {p_hex}")
                    
                    # Show which bytes differ
                    diff_markers = []
                    for i in range(18):
                        if i < len(g_first) and i < len(p_first) and g_first[i] != p_first[i]:
                            diff_markers.append('^^')
                        else:
                            diff_markers.append('  ')
                    print(f"      Diff:     {' '.join(diff_markers)}")
            
            results.append({
                'label': label,
                'baud': r['baud'],
                'g_patches': g['patches'],
                'p_patches': p['patches'],
                'exact': cmp['exact_matches'],
                'partial': cmp['partial_matches_1to5'],
            })
            
            # If PLL got no patches, try the other baud rate
            if p['patches'] == 0 and baud in (340, 1200):
                alt_baud = 1200 if baud == 340 else 340
                print(f"\n    Trying alternate baud rate ({alt_baud}) for PLL...")
                r_alt = vis.decode_fsk_pll(r2, r['sr'], alt_baud)
                if r_alt['num_patches'] > 0:
                    print(f"    PLL@{alt_baud}: {r_alt['num_patches']} patches (vs {p['patches']}@{baud})")
            
            print()
            
        except Exception as e:
            print(f"  [ERR] {label}: {e}")
            import traceback
            traceback.print_exc()
    
    # Summary table
    print()
    print("=" * 80)
    print("  SUMMARY TABLE")
    print("=" * 80)
    print()
    print(f"  {'Tape':30s}  {'Baud':>4}  {'Gtz':>4}  {'PLL':>4}  {'Match':>4}  {'Partial':>4}")
    print(f"  {'-'*30}  {'-'*4}  {'-'*4}  {'-'*4}  {'-'*4}  {'-'*4}")
    
    total_g = 0
    total_p = 0
    total_match = 0
    
    for r in results:
        print(f"  {r['label']:30s}  {r['baud']:>4d}  {r['g_patches']:>4d}  {r['p_patches']:>4d}  {r['exact']:>4d}  {r['partial']:>4d}")
        total_g += r['g_patches']
        total_p += r['p_patches']
        total_match += r['exact']
    
    print(f"  {'-'*30}  {'-'*4}  {'-'*4}  {'-'*4}  {'-'*4}  {'-'*4}")
    print(f"  {'TOTAL':30s}  {'':>4s}  {total_g:>4d}  {total_p:>4d}  {total_match:>4d}  {'':>4s}")
    print()
    
    if total_g > 0:
        pct = total_p * 100.0 / total_g
        print(f"  PLL efficiency: {total_p}/{total_g} patches ({pct:.0f}% of Goertzel)")
    
    print("=" * 80)


if __name__ == '__main__':
    main()
