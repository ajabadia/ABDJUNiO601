"""Create a minimal Roland Juno-106 SysEx bulk dump for testing."""
import struct

def create_test_sysex(path):
    # Roland Juno-106 Bulk Dump: F0 41 3n 02 01 [64 patches x 18 bytes] [checksum] F7
    payload = bytearray()
    payload.append(0xF0)  # SysEx start
    payload.append(0x41)  # Roland
    payload.append(0x30)  # Device ID = 0x30 (typical)
    payload.append(0x02)  # Juno-106
    payload.append(0x01)  # Bulk Dump (function code)

    # Generate 64 patches of 18 bytes each with recognizable data
    for patch_idx in range(64):
        patch = bytearray(18)
        # Patch 0: lfoRate
        patch[0] = (patch_idx * 7) & 0x7F   # lfoRate varies per patch
        patch[1] = 0x00                      # lfoDelay
        patch[2] = 0x00                      # lfoToDCO
        patch[3] = 0x40                      # pwm (50%)
        patch[4] = 0x00                      # noise
        patch[5] = 0x7F                      # vcfFreq (full open)
        patch[6] = 0x00                      # resonance
        patch[7] = 0x40                      # envAmount
        patch[8] = 0x00                      # lfoToVCF
        patch[9] = 0x40                      # kybdTracking
        patch[10] = 0x7F                     # vcaLevel (full)
        patch[11] = 0x00                     # attack (fast)
        patch[12] = 0x20                     # decay
        patch[13] = 0x40                     # sustain (50%)
        patch[14] = 0x20                     # release
        patch[15] = 0x00                     # subOsc
        patch[16] = 0x29                     # SW1: sawOn=1, pulseOn=0, dcoRange=8'
        patch[17] = 0x18                     # SW2: chorus=off, vcaMode=env, vcfPol=pos, hpf=0, pwmMode=lfo
        payload.extend(patch)

    # Checksum (sum of all patch bytes & 0x7F, then negate & 0x7F)
    checksum = 0
    for b in payload[5:]:  # after header (F0 41 30 02 01)
        checksum += b
    checksum = (-checksum) & 0x7F
    payload.append(checksum)
    payload.append(0xF7)  # SysEx end

    with open(path, 'wb') as f:
        f.write(payload)
    print(f"Created: {path}")
    print(f"Size: {len(payload)} bytes")
    print(f"Expected: 5 + 1152 + 1 + 1 = 1159 bytes")
    return path

if __name__ == '__main__':
    import os
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    out_dir = os.path.join(script_dir, "..", "docs")
    os.makedirs(out_dir, exist_ok=True)
    create_test_sysex(os.path.join(script_dir, "..", "docs", "test_juno106_bank.syx"))
