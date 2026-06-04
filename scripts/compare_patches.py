#!/usr/bin/env python3
"""
Compare decoded WAV tape patches against known factory presets.
Extracts all patches from FactoryPresets.h and matches decoded bytes.
Shows hex dumps and field-level comparison for the first patches.

Usage:
    python scripts/compare_patches.py
    python scripts/compare_patches.py --show-hex 5   # Show first 5 patches per tape
    python scripts/compare_patches.py --wav <file> --baud 1200  # Single file mode
"""

import sys
import os
import re
import argparse
import importlib.util

# --- Load the visualizer module ---
sys.path.insert(0, 'scripts')
spec = importlib.util.spec_from_file_location('vis', 'scripts/visualize_tape.py')
vis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vis)


# --- JunoPatch struct field definition ---
JUNO_PATCH_FIELDS = [
    (0,  "lfoRate",       "LFO Rate"),
    (1,  "lfoDelay",      "LFO Delay"),
    (2,  "lfoToDCO",      "LFO->DCO"),
    (3,  "pwm",            "PWM"),
    (4,  "noise",          "Noise"),
    (5,  "vcfFreq",        "VCF Freq"),
    (6,  "resonance",      "Resonance"),
    (7,  "envAmount",      "Env Amount"),
    (8,  "lfoToVCF",       "LFO->VCF"),
    (9,  "kybdTracking",   "Kybd Track"),
    (10, "vcaLevel",       "VCA Level"),
    (11, "attack",         "Attack"),
    (12, "decay",          "Decay"),
    (13, "sustain",        "Sustain"),
    (14, "release",        "Release"),
    (15, "subOsc",         "Sub Osc"),
    (16, "sw1",            "SW1 (switches)"),
    (17, "sw2",            "SW2 (switches)"),
]


def format_field_table(data, diff_indices=None):
    """Format a field-by-field table for 18-byte patch data."""
    lines = []
    lines.append(f"    {'Idx':>3} {'Field':16s} {'Hex':>4} {'Dec':>4}  {'Description'}")
    lines.append(f"    {'---':>3} {'-'*16} {'----':>4} {'---':>4}  {'-'*20}")
    for idx, varname, desc in JUNO_PATCH_FIELDS:
        b = data[idx]
        marker = " *" if diff_indices and idx in diff_indices else "  "
        lines.append(f"    {idx:3d} {varname:16s} {b:02X} {b:4d}  {desc}{marker}")
    return '\n'.join(lines)


def format_sw1_sw2_details(data, label="Juno-60"):
    """Decode SW1 and SW2 bitfields (Juno-60 DCB format)."""
    sw1 = data[16]
    sw2 = data[17]
    foot = "'"  # foot symbol (apostrophe)
    lines = []
    lines.append(f"    SW1 (0x{sw1:02X}) [{label} format]:")
    lines.append(f"      Bit 0 (0x01) - VCO Gate:       {'ON' if sw1 & 0x01 else 'OFF'}")
    lines.append(f"      Bit 1 (0x02) - VCF Gate:       {'ON' if sw1 & 0x02 else 'OFF'}")
    lines.append(f"      Bit 2 (0x04) - VCA Gate:       {'ON' if sw1 & 0x04 else 'OFF'}")
    lines.append(f"      Bit 3 (0x08) - Chorus I:       {'ON' if sw1 & 0x08 else 'OFF'}")
    lines.append(f"      Bit 4 (0x10) - Chorus II:      {'ON' if sw1 & 0x10 else 'OFF'}")
    lines.append(f"      Bit 5 (0x20) - LFO Key Trigger: {'ON' if sw1 & 0x20 else 'OFF'}")
    lines.append(f"      Bit 6 (0x40) - VCF ENV Polarity:{'POS' if sw1 & 0x40 else 'NEG'}")
    lines.append(f"      Bit 7 (0x80) - Range:          {('4' + foot) if sw1 & 0x80 else ('8' + foot)}")
    lines.append(f"    SW2 (0x{sw2:02X}) [{label} format]:")
    lines.append(f"      Bits 0-1 - HPF:               {sw2 & 0x03} (0=THRU, 1=pos1, 2=pos2, 3=FLAT)")
    lines.append(f"      Bit 2 (0x04) - VCA Mode:      {'GATE' if sw2 & 0x04 else 'ENV'}")
    lines.append(f"      Bit 3 (0x08) - Pulse Width:   {'MANUAL' if sw2 & 0x08 else 'LFO'}")
    lines.append(f"      Bit 4 (0x10) - Saw Wave:      {'ON' if sw2 & 0x10 else 'OFF'}")
    lines.append(f"      Bit 5 (0x20) - Pulse Wave:    {'ON' if sw2 & 0x20 else 'OFF'}")
    lines.append(f"      Bits 6-7 - Model Route:       {(sw2 >> 6) & 0x03} (0=A, 1=B, 2=C, 3=D)")
    return '\n'.join(lines)


# --- Parse FactoryPresets.h ---
def parse_factory_header(path="Source/Core/FactoryPresets.h"):
    """Parse JunoPatch entries from the C++ header file."""
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    patches = {}  # name -> bytes

    sections = {
        'junoFactoryPatches': 'Juno-106 Factory',
        'juno60FactoryPatches': 'Juno-60 Factory',
        'davidChurcherPatches': 'David Churcher Custom',
    }

    for var_name, section_label in sections.items():
        pattern = re.compile(
            r'static const JunoPatch ' + re.escape(var_name) +
            r'(?:\[\d*\])?\s*=\s*\{'
        )
        match = pattern.search(content)
        if not match:
            print(f"  WARNING: Could not find {var_name}")
            continue

        brace_start = match.end() - 1
        depth = 0
        brace_end = brace_start
        for i in range(brace_start, len(content)):
            if content[i] == '{':
                depth += 1
            elif content[i] == '}':
                depth -= 1
                if depth == 0:
                    brace_end = i + 1
                    break

        array_text = content[brace_start:brace_end]
        entries = re.findall(
            r'\{"([^"]+)"\s*,\s*((?:0x[0-9A-Fa-f]{1,2}\s*,\s*){17}0x[0-9A-Fa-f]{1,2})\s*\}',
            array_text
        )

        for name, byte_str in entries:
            hex_vals = re.findall(r'0x([0-9A-Fa-f]{1,2})', byte_str)
            if len(hex_vals) == 18:
                byte_data = bytes(int(h, 16) for h in hex_vals)
                patches[name] = byte_data

    return patches


def show_patch_hex_dump(label, patches, factory_patches, max_show=3):
    """Show hex dump of first N patches with field-level detail."""
    num_show = min(max_show, len(patches))

    for pi in range(num_show):
        pdata = patches[pi]
        print(f"\n  --- {label} — Patch {pi+1} ---")

        # Hex dump with index ruler
        hex_str = ' '.join(f'{b:02X}' for b in pdata)
        idx_str = ' '.join(f'{i:02d}' for i in range(18))
        print(f"    {'Bytes:':12s} {hex_str}")
        print(f"    {'Index:':12s} {idx_str}")

        # Compact field-value line
        field_vals = []
        for idx, varname, desc in JUNO_PATCH_FIELDS:
            field_vals.append(f"{varname}={pdata[idx]:02X}")
        print(f"    {'Fields:':12s} {' '.join(field_vals)}")

        # Field detail table
        print()
        print(format_field_table(pdata))
        print()

        # SW1/SW2 decoder (Juno-60 DCB format — bit meanings may differ for Juno-106)
        print(format_sw1_sw2_details(pdata, label="Juno-60 DCB"))
        print("    (Note: bit meanings are based on Juno-60 DCB format; Juno-106 may differ)")

        # Look for closest factory match
        best_match = None
        best_diff = 999
        for fname, fdata in factory_patches.items():
            diff = sum(1 for a, b in zip(pdata, fdata) if a != b)
            if diff < best_diff:
                best_diff = diff
                best_match = (fname, fdata, diff)

        if best_match:
            fname, fdata, diff = best_match
            print(f"\n    --- Closest factory match: \"{fname}\" ({diff} bytes differ) ---")
            # Show diff markers
            diff_indices = set(i for i in range(18) if pdata[i] != fdata[i])
            marked = []
            for i, b in enumerate(fdata):
                if i in diff_indices:
                    marked.append(f'<{b:02X}>')
                else:
                    marked.append(f' {b:02X} ')
            print(f"    {'Factory:':12s} {' '.join(marked)}")
            print(f"    {'Decoded:':12s} {hex_str}")
            print()

            # Show which fields differ
            diff_fields = []
            for idx, varname, desc in JUNO_PATCH_FIELDS:
                if idx in diff_indices:
                    decoded_val = pdata[idx]
                    factory_val = fdata[idx]
                    diff_fields.append(f"{varname}: 0x{decoded_val:02X} vs 0x{factory_val:02X} (factory)")
            for df in diff_fields:
                print(f"      DIFF: {df}")
            print()

            # Field table for factory
            print(format_field_table(fdata, diff_indices=diff_indices))


def show_all_hex_dumps(all_decoded, factory_patches, max_per_tape=3):
    """Show hex dumps for the first N patches of each tape."""
    print("\n" + "=" * 75)
    print("  HEX DUMP: First patches from each tape (field-level analysis)")
    print("=" * 75)
    print("\n  JunoPatch struct layout (18 bytes):")
    row1 = '  '.join(f'{i:2d}:{JUNO_PATCH_FIELDS[i][1]:>12}' for i in range(0, 9))
    row2 = '  '.join(f'{i:2d}:{JUNO_PATCH_FIELDS[i][1]:>12}' for i in range(9, 18))
    print(f"  {row1}")
    print(f"  {row2}")
    print()

    for label in sorted(all_decoded.keys()):
        patches = all_decoded[label]
        show_patch_hex_dump(label, patches, factory_patches, max_show=max_per_tape)


def main():
    parser = argparse.ArgumentParser(description="Compare decoded WAV tape patches against factory presets")
    parser.add_argument("--show-hex", type=int, default=3, nargs='?', const=3,
                        help="Show hex dump of first N patches per tape (default: 3, 0 to disable)")
    parser.add_argument("--wav", type=str, default=None,
                        help="Decode a single WAV file instead of all defaults")
    parser.add_argument("--baud", type=int, default=0, choices=[0, 340, 1200],
                        help="Force baud rate (0 = auto)")
    args = parser.parse_args()

    print("=" * 75)
    print("  PATCH COMPARISON: Decoded WAV tapes vs Factory Presets")
    print("=" * 75)

    # --- Parse factory presets ---
    print("\n[1] Parsing FactoryPresets.h...")
    factory_patches = parse_factory_header()
    print(f"  Found {len(factory_patches)} known patches")

    # Build reverse lookup: bytes -> [names]
    bytes_to_names = {}
    for name, pdata in factory_patches.items():
        key = pdata.hex().upper()
        bytes_to_names.setdefault(key, [])
        bytes_to_names[key].append(name)

    # --- Decode WAV files ---
    print("\n[2] Decoding WAV files...")

    if args.wav:
        wav_files = [(args.wav, args.baud if args.baud is not None else 0, os.path.basename(args.wav))]
    else:
        wav_files = [
            ('docs/Juno-60 (1)/JUNO-60 Bank A.wav', 0, 'JUNO-60 Bank A'),
            ('docs/Juno-60 (1)/JUNO-60 Bank B.wav', 0, 'JUNO-60 Bank B'),
            ('docs/JUNO-106/JUNO106 Bank A.wav', 0, 'JUNO-106 Bank A'),
            ('docs/JUNO-106/JUNO106 Bank B.wav', 0, 'JUNO-106 Bank B'),
            ('docs/JUNO-106/Roland Juno-60 factory programs group 1.wav', 0, 'Juno-60 G1'),
            ('docs/JUNO-106/Roland Juno-60 factory programs group 2.wav', 0, 'Juno-60 G2'),
            ('JUNO106/original/tapes/roland_juno106_factory/j106ma.wav', 0, 'j106ma'),
            ('JUNO106/original/tapes/roland_juno106_factory/j106mb.wav', 0, 'j106mb'),
        ]

    all_decoded = {}  # label -> [bytes]
    per_tape_sr = {}    # label -> sample rate
    per_tape_baud = {}  # label -> baud rate used

    for path, baud, label in wav_files:
        if not os.path.isfile(path):
            sys.stderr.write(f"  [WARN] {label}: file not found, skipping\n")
            continue
        try:
            r, sr, ch = vis.load_wav(path)
            r2, sr = vis.preprocess(r, sr, ch)

            if baud != 0:
                # User forced a specific baud rate — try only that one (matches C++ forcedBaudRate exclusivity)
                result = vis.decode_fsk(r2, sr, baud, fast=True)
            else:
                # Auto-detect mode: try both rates, pick the one with the most patches
                detected = vis.detect_format(r2, sr)
                hint = detected if detected else 340

                best_result = None
                best_baud = 0
                for try_baud in [hint, 1200 if hint == 340 else 340]:
                    r = vis.decode_fsk(r2, sr, try_baud, fast=True)
                    if r and r['num_patches'] > (best_result['num_patches'] if best_result else -1):
                        best_result = r
                        best_baud = try_baud

                result = best_result
                baud = best_baud

            if result and result['validated']:
                patches = []
                for pi in range(result['num_patches']):
                    off = pi * 18
                    data = bytes(result['validated'][off:off+18])
                    patches.append(data)
                all_decoded[label] = patches
                per_tape_sr[label] = sr
                per_tape_baud[label] = baud
                sys.stdout.write(f"  [OK] {label}: {len(patches)} patches decoded (baud={baud}, sr={sr})\n")
            else:
                per_tape_sr[label] = sr
                per_tape_baud[label] = baud
                sys.stderr.write(f"  [--] {label}: no patches decoded (baud={baud})\n")
        except Exception as e:
            sys.stderr.write(f"  [ERR] {label}: {e}\n")

    # --- Show hex dumps ---
    if args.show_hex and all_decoded:
        show_all_hex_dumps(all_decoded, factory_patches, max_per_tape=args.show_hex)

    # --- Compare for matches ---
    print("\n[3] Comparing decoded patches against factory presets...")
    print()

    total_decoded = 0
    total_matches = 0
    total_unique_matches = set()

    for label, patches in sorted(all_decoded.items()):
        sys.stdout.write(f"  -- {label} ({len(patches)} patches) --\n")
        for pi, pdata in enumerate(patches):
            total_decoded += 1
            key = pdata.hex().upper()

            if key in bytes_to_names:
                for name in bytes_to_names[key]:
                    total_matches += 1
                    total_unique_matches.add(name)
                    hex_str = ' '.join(f'{b:02X}' for b in pdata)
                    sys.stdout.write(f"    [MATCH] Patch {pi+1:2d} = \"{name}\"\n")
                    sys.stdout.write(f"             {hex_str}\n")
            else:
                # Check for near match (1-2 bytes off)
                best_match = None
                best_diff = 999
                for fname, fdata in factory_patches.items():
                    diff = sum(1 for a, b in zip(pdata, fdata) if a != b)
                    if diff < best_diff:
                        best_diff = diff
                        best_match = fname

                if best_diff <= 2:
                    hex_str = ' '.join(f'{b:02X}' for b in pdata)
                    sys.stdout.write(f"    [NEAR] Patch {pi+1:2d} ~ \"{best_match}\" ({best_diff} bytes off)\n")
                    sys.stdout.write(f"             {hex_str}\n")
                else:
                    # Only print first 2 non-matching patches to avoid clutter
                    if pi < 2:
                        hex_str = ' '.join(f'{b:02X}' for b in pdata)
                        sys.stdout.write(f"           Patch {pi+1:2d}: {hex_str}  (no match)\n")
                    elif pi == 2:
                        sys.stdout.write(f"           ... ({len(patches)-2} more non-matching patches)\n")
        print()

    # --- Summary ---
    print("=" * 75)
    print("  SUMMARY")
    print()
    print(f"  {'Tape':30s} {'SR (Hz)':>9}  {'Baud':>5}  {'Patches':>7}")
    print(f"  {'-'*30} {'-'*9}  {'-'*5}  {'-'*7}")
    for label, patches in sorted(all_decoded.items()):
        # We stored sr in a dict during decoding
        sr = per_tape_sr.get(label, '?')
        baud = per_tape_baud.get(label, '?')
        print(f"  {label:30s} {str(sr):>9}  {str(baud):>5}  {len(patches):>7d}")
    print()
    print(f"  Total decoded patches: {total_decoded}")
    print(f"  Exact factory matches: {total_matches}")
    print(f"  Unique factory patches identified: {len(total_unique_matches)}")
    if total_unique_matches:
        print(f"\n  Identified patches:")
        for name in sorted(total_unique_matches):
            print(f"    - {name}")
    print("=" * 75)


if __name__ == '__main__':
    main()
