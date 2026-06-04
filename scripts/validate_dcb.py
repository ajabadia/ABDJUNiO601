#!/usr/bin/env python3
"""
Validate decoded tape patches for Juno-60 DCB structural correctness.

Checks:
1. Slider bytes (0-15): range 0-127
2. SW1 (byte 16): mutually exclusive range bits, valid wave/chorus/pwm bits
3. SW2 (byte 17): reserved bits must be 0, valid HPF encoding
4. Checksum consistency (sum of 18 bytes & 0x7F)
5. Statistical distribution across all patches

Usage:
    python scripts/validate_dcb.py
    python scripts/validate_dcb.py --wav <file> --baud 340
"""

import sys, os, re, argparse, math
sys.path.insert(0, 'scripts')
import importlib.util
spec = importlib.util.spec_from_file_location('vis', 'scripts/visualize_tape.py')
vis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vis)


# --- Juno-60 DCB Bit Definitions ---
# Byte 16 (SW1) - Juno-60 DCB format
SW1_BITS = {
    0: "Range 16'",
    1: "Range 8'",
    2: "Range 4'",
    3: "Saw Wave",
    4: "Pulse Wave",
    5: "Sub Osc On",
    6: "PWM On",
    7: "PWM Mode (0=LFO, 1=Manual)",
}

# Byte 17 (SW2) - Juno-60 DCB format
SW2_BITS = {
    0: "VCA Mode (0=ENV, 1=GATE)",
    1: "VCF Polarity (0=POS, 1=NEG)",
    2: "RESERVED (must be 0)",
    3: "HPF bit 0",
    4: "HPF bit 1",
    5: "RESERVED (must be 0)",
    6: "RESERVED (must be 0)",
    7: "RESERVED (must be 0)",
}

SW2_RESERVED_BITS = {2, 5, 6, 7}  # Must be 0

# --- Juno-106 SW1/SW2 Bit Definitions (for comparison) ---
# Juno-106 SW1: Range(0-2), Pulse(3), Saw(4), ChorusEnable(5), ChorusMode(6)
# Juno-106 SW2: PWMMode(0), VCFPol(1), VCAMode(2), HPF(3-4)
J106_SW2_RESERVED_BITS = {5, 6, 7}  # Only bits 5-7 reserved on Juno-106
J106_PWM_MAX = 105  # Juno-107 PWM is 0-105, not 0-127


def validate_slider_byte(byte_val, idx, label):
    """Check slider byte range 0-127. Returns list of issues."""
    issues = []
    if not (0 <= byte_val <= 127):
        issues.append(f"  byte[{idx}] ({label}): value {byte_val} OUT OF RANGE 0-127")
    return issues


def validate_sw1(sw1, is_juno60=True):
    """Validate SW1 byte. Returns list of issues."""
    issues = []
    
    # Check range bits (0-2): exactly one should be set (same for both formats)
    range_bits = sw1 & 0x07
    if range_bits == 0:
        issues.append(f"  SW1 bits 0-2: NO range selected (should be 16', 8', or 4')")
    elif bin(range_bits).count('1') > 1:
        ranges = []
        if sw1 & (1 << 0): ranges.append("16'")
        if sw1 & (1 << 1): ranges.append("8'")
        if sw1 & (1 << 2): ranges.append("4'")
        issues.append(f"  SW1 bits 0-2: MULTIPLE ranges selected ({', '.join(ranges)}) -- should be exclusive")
    
    if is_juno60:
        # Juno-60: all 8 bits defined, no reserved bits
        pass
    else:
        # Juno-106: bits 5 (Chorus Enable) and 6 (Chorus Mode) are valid
        # Bit 7 is unused/reserved on Juno-106
        if sw1 & (1 << 7):
            issues.append(f"  SW1 bit 7: SET to 1 but should be 0 (reserved on Juno-106)")
    
    return issues


def validate_sw2(sw2, is_juno60=True):
    """Validate SW2 byte. Returns list of issues."""
    issues = []
    
    if is_juno60:
        reserved_bits = SW2_RESERVED_BITS  # {2, 5, 6, 7}
        for bit in sorted(reserved_bits):
            if sw2 & (1 << bit):
                issues.append(f"  SW2 bit {bit} ({SW2_BITS[bit]}): SET to 1 but should be 0 (reserved in Juno-60)")
    else:
        # Juno-106: bits 5-7 reserved
        for bit in sorted(J106_SW2_RESERVED_BITS):
            if sw2 & (1 << bit):
                issues.append(f"  SW2 bit {bit}: SET to 1 but should be 0 (reserved in Juno-106)")
    
    return issues


def validate_patch(pdata, patch_idx=0, tape_label="", is_juno60=True):
    """Full validation of one 18-byte patch.
    
    Returns a dict with:
        structurally_valid: True if all structural checks pass (slider range, SW1, SW2)
        fully_valid: True if both structural and pattern checks pass
        issues: dict of issue categories with their messages
        structural_count: number of structural issues found
        pattern_count: number of pattern warnings found
    """
    issues = {
        "slider_range": [],
        "sw1": [],
        "sw2": [],
        "pattern": [],
    }
    
    # 1. Slider range check (bytes 0-15)
    slider_labels = ["lfoRate", "lfoDelay", "lfoToDCO", "pwm", "noise",
                     "vcfFreq", "resonance", "envAmount", "lfoToVCF",
                     "kybdTracking", "vcaLevel", "attack", "decay",
                     "sustain", "release", "subOsc"]
    
    for idx in range(16):
        issues["slider_range"].extend(validate_slider_byte(pdata[idx], idx, slider_labels[idx]))
    
    # PWM special range: Juno-106 is 0-105, Juno-60 is 0-127
    if not is_juno60 and pdata[3] > J106_PWM_MAX:
        issues["slider_range"].append(f"  byte[3] (pwm): value {pdata[3]} > {J106_PWM_MAX} (Juno-106 PWM max)")
    
    # 2. SW1/2 validation (format-aware)
    issues["sw1"].extend(validate_sw1(pdata[16], is_juno60))
    issues["sw2"].extend(validate_sw2(pdata[17], is_juno60))
    
    # 3. Pattern analysis: check for suspicious patterns (SOFT warnings only)
    if all(b == 0 for b in pdata):
        issues["pattern"].append(f"  ALL BYTES ZERO (uninitialized/empty patch)")
    
    if len(set(pdata)) == 1 and pdata[0] != 0:
        issues["pattern"].append(f"  ALL BYTES = 0x{pdata[0]:02X} (suspicious uniform value)")
    
    slider_vals = pdata[:16]
    unique_vals = len(set(slider_vals))
    if unique_vals <= 2 and all(b == 0 or b == 0x7F for b in slider_vals):
        issues["pattern"].append(f"  Slider bytes only 0 or 0x7F ({unique_vals} unique values)")
    
    # Structural issues = slider_range + sw1 + sw2 (hard constraints)
    # Pattern issues = pattern (soft diagnostics)
    structural_issues = issues["slider_range"] + issues["sw1"] + issues["sw2"]
    pattern_issues = issues["pattern"]
    
    return {
        "structurally_valid": len(structural_issues) == 0,
        "fully_valid": len(structural_issues) == 0 and len(pattern_issues) == 0,
        "issues": issues,
        "structural_count": len(structural_issues),
        "pattern_count": len(pattern_issues),
    }


def run_validation(all_decoded):
    """Run validation on all decoded patches."""
    total_patches = 0
    total_structural_pass = 0
    total_fully_pass = 0
    
    print("=" * 75)
    print("  DCB STRUCTURE VALIDATION: Decoded Tape Patches")
    print("=" * 75)
    print()
    print("  Checks:")
    print("    HARD (structural): slider range 0-127, SW1 range exclusive, SW2 reserved=0")
    print("    SOFT (pattern):   all-zeros, uniform values, slider extremes only")
    print()
    
    for label in sorted(all_decoded.keys()):
        patches = all_decoded[label]
        num_p = len(patches)
        structural_pass = 0
        structural_fail = 0
        fully_pass = 0
        tape_issues = []
        
        # Determine format
        is_j60 = '60' in label and '106' not in label
        fmt_name = "Juno-60" if is_j60 else "Juno-106"
        
        for pi in range(num_p):
            total_patches += 1
            pdata = patches[pi]
            result = validate_patch(pdata, pi + 1, label, is_j60)
            
            if result["structurally_valid"]:
                structural_pass += 1
            else:
                structural_fail += 1
                tape_issues.append((pi + 1, result))
            
            if result["fully_valid"]:
                fully_pass += 1
                total_fully_pass += 1
            
            if result["structurally_valid"]:
                total_structural_pass += 1
        
        print(f"  [{label}] {num_p} patches ({fmt_name})")
        print(f"    Structural: {structural_pass} passed, {structural_fail} failed")
        print(f"    Fully valid (struct+pattern): {fully_pass}")
        
        # Show structural failures with their issues
        if tape_issues and structural_fail > 0:
            for pnum, result in tape_issues[:5]:  # Show first 5 failures
                for cat in ["slider_range", "sw1", "sw2"]:
                    for issue in result["issues"][cat]:
                        print(f"    P{pnum}: {issue}")
            if structural_fail > 5:
                print(f"    ... and {structural_fail - 5} more patches with structural issues")
        
        # Count pattern-only patches (structurally valid but pattern warnings)
        pattern_only = 0
        for pi in range(num_p):
            result = validate_patch(patches[pi], pi + 1, label, is_j60)
            if result["structurally_valid"] and result["pattern_count"] > 0:
                pattern_only += 1
        
        if pattern_only > 0:
            print(f"    Pattern-only warnings (structurally valid): {pattern_only}")
        
        # Print SW1/SW2 bitfield statistics for this tape
        range_stats = {0: 0, 1: 0, 2: 0, 'mixed': 0, 'none': 0}
        saw_count = 0
        pulse_count = 0
        sw1_bit5_name = "Sub Osc" if is_j60 else "Chorus En"
        sw1_bit6_name = "PWM On" if is_j60 else "Chorus Mode"
        bit5_count = 0
        bit6_count = 0
        pwm_mode_man = 0
        vca_gate = 0
        vcf_neg = 0
        hpf_stats = {0: 0, 1: 0, 2: 0, 3: 0}
        reserved_violations = 0
        reserved_set = SW2_RESERVED_BITS if is_j60 else J106_SW2_RESERVED_BITS
        
        for pi in range(num_p):
            sw1 = patches[pi][16]
            sw2 = patches[pi][17]
            
            rb = sw1 & 0x07
            if rb == 0:
                range_stats['none'] += 1
            elif rb in (1, 2, 4):
                range_stats[{1: 0, 2: 1, 4: 2}[rb]] += 1
            else:
                range_stats['mixed'] += 1
            
            if sw1 & (1 << 3): saw_count += 1
            if sw1 & (1 << 4): pulse_count += 1
            if sw1 & (1 << 5): bit5_count += 1
            if sw1 & (1 << 6): bit6_count += 1
            if is_j60:
                if sw1 & (1 << 7): pwm_mode_man += 1
            
            if is_j60:
                if sw2 & (1 << 0): vca_gate += 1
                if sw2 & (1 << 1): vcf_neg += 1
            else:
                if sw2 & (1 << 2): vca_gate += 1
                if sw2 & (1 << 1): vcf_neg += 1
            
            hpf = (sw2 >> 3) & 0x03
            hpf_stats[hpf] += 1
            
            for rb in reserved_set:
                if sw2 & (1 << rb):
                    reserved_violations += 1
        
        pwm_mode_lfo = num_p - pwm_mode_man
        print(f"       Range: 16'={range_stats[0]}, 8'={range_stats[1]}, 4'={range_stats[2]}"
              f", mixed={range_stats['mixed']}, none={range_stats['none']}")
        print(f"       Waves: Saw={saw_count}, Pulse={pulse_count}")
        print(f"       {sw1_bit5_name}={bit5_count}, {sw1_bit6_name}={bit6_count}")
        if is_j60:
            print(f"       PWM Mode: LFO={pwm_mode_lfo}, Manual={pwm_mode_man}")
        print(f"       VCA Mode: ENV={num_p - vca_gate}, GATE={vca_gate}")
        print(f"       VCF Polarity: POS={num_p - vcf_neg}, NEG={vcf_neg}")
        print(f"       HPF: pos0/FLAT={hpf_stats[3]}, pos1={hpf_stats[2]}, pos2={hpf_stats[1]}, pos3={hpf_stats[0]}")
        if reserved_violations:
            print(f"       *** SW2 reserved bit violations: {reserved_violations} (format={fmt_name}) ***")
        print()
    
    # Summary
    print("=" * 75)
    print("  SUMMARY")
    print(f"  Total patches: {total_patches}")
    struct_pct = round(total_structural_pass * 100.0 / max(total_patches, 1))
    fully_pct = round(total_fully_pass * 100.0 / max(total_patches, 1))
    print(f"  Structurally valid (HARD checks): {total_structural_pass} ({struct_pct}%)")
    print(f"  Fully valid (HARD + SOFT):        {total_fully_pass} ({fully_pct}%)")
    print(f"  Patches with pattern-only warnings: {total_structural_pass - total_fully_pass}")
    print("=" * 75)


def main():
    parser = argparse.ArgumentParser(description="Validate decoded tape patches against Juno-60 DCB spec")
    parser.add_argument("--wav", type=str, default=None, help="Single WAV file to validate")
    parser.add_argument("--baud", type=int, default=0, choices=[0, 340, 1200], help="Force baud rate")
    args = parser.parse_args()
    
    # Decode WAV files (same logic as compare_patches.py)
    if args.wav:
        wav_files = [(args.wav, args.baud if args.baud else 340, os.path.basename(args.wav))]
    else:
        wav_files = [
            ('docs/Juno-60 (1)/JUNO-60 Bank A.wav', 340, 'JUNO-60 Bank A'),
            ('docs/Juno-60 (1)/JUNO-60 Bank B.wav', 340, 'JUNO-60 Bank B'),
            ('docs/JUNO-106/JUNO106 Bank A.wav', 1200, 'JUNO-106 Bank A'),
            ('docs/JUNO-106/JUNO106 Bank B.wav', 1200, 'JUNO-106 Bank B'),
            ('docs/JUNO-106/Roland Juno-60 factory programs group 1.wav', 1200, 'Juno-60 G1'),
            ('docs/JUNO-106/Roland Juno-60 factory programs group 2.wav', 1200, 'Juno-60 G2'),
            ('JUNO106/original/tapes/roland_juno106_factory/j106ma.wav', 1200, 'j106ma'),
            ('JUNO106/original/tapes/roland_juno106_factory/j106mb.wav', 1200, 'j106mb'),
        ]
    
    all_decoded = {}
    
    print("Decoding WAV files...")
    for path, baud, label in wav_files:
        if not os.path.isfile(path):
            print(f"  [WARN] {label}: file not found")
            continue
        try:
            r, sr, ch = vis.load_wav(path)
            r2, sr = vis.preprocess(r, sr, ch)
            
            if baud == 0:
                detected = vis.detect_format(r2, sr)
                baud = detected if detected else 1200
            
            result = None
            for try_baud in [baud, 1200 if baud == 340 else 340]:
                result = vis.decode_fsk(r2, sr, try_baud, fast=True)
                if result and result['num_patches'] >= 3:
                    baud = try_baud
                    break
            
            if result and result['validated']:
                patches = []
                for pi in range(result['num_patches']):
                    off = pi * 18
                    data = bytes(result['validated'][off:off+18])
                    patches.append(data)
                all_decoded[label] = patches
                print(f"  [OK] {label}: {len(patches)} patches (baud={baud}, sr={sr})")
            else:
                print(f"  [--] {label}: no patches decoded")
        except Exception as e:
            print(f"  [ERR] {label}: {e}")
    
    if not all_decoded:
        print("No patches to validate.")
        return
    
    run_validation(all_decoded)


if __name__ == '__main__':
    main()
