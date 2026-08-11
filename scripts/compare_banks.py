import re

def parse_our_bank(filepath):
    with open(filepath, 'r') as f:
        text = f.read()
    
    matches = re.findall(r'\{\"(.*?)\",\s*(.*?)\}', text)
    patches = []
    for name, vals_str in matches:
        # vals_str is like '0x14, 0x31, ...'
        vals = [int(v.strip(), 16) if '0x' in v else int(v.strip()) for v in vals_str.split(',') if v.strip()]
        if len(vals) == 18:
            patches.append((name, vals))
    return patches

def parse_kr106_bank(filepath):
    with open(filepath, 'r') as f:
        text = f.read()
    
    matches = re.findall(r'\{\"(.*?)\",\s*\{(.*?)\}\}', text)
    # J106 are the last 128
    j106_matches = matches[128:]
    patches = []
    for name, vals_str in j106_matches:
        vals = [int(v.strip()) for v in vals_str.split(',') if v.strip()]
        if len(vals) == 44:
            patches.append((name, vals))
    return patches

def compare_patches(our_patch, kr_patch):
    our_name, our_syx = our_patch
    kr_name, kr_vals = kr_patch
    
    # Map KR-106 44 params to our 18 byte sysex format to compare
    kr_syx = [0] * 18
    kr_syx[0] = kr_vals[3]  # lfoRate
    kr_syx[1] = kr_vals[4]  # lfoDelay
    kr_syx[2] = kr_vals[5]  # lfoToDCO
    
    # PWM in KR-106 is clamped to 105, but wait, the factory preset in KR-106 is from a sysex dump. 
    # Did KR-106 save it clamped or raw? Let's check if they match.
    kr_syx[3] = kr_vals[6]  # pwm
    
    kr_syx[4] = kr_vals[8]  # noise
    kr_syx[5] = kr_vals[10] # vcfFreq
    kr_syx[6] = kr_vals[11] # resonance
    kr_syx[7] = kr_vals[12] # envAmount
    kr_syx[8] = kr_vals[13] # lfoToVCF
    kr_syx[9] = kr_vals[14] # kybdTracking
    kr_syx[10] = kr_vals[15] # vcaLevel
    kr_syx[11] = kr_vals[16] # attack
    kr_syx[12] = kr_vals[17] # decay
    kr_syx[13] = kr_vals[18] # sustain
    kr_syx[14] = kr_vals[19] # release
    kr_syx[15] = kr_vals[7]  # subOsc
    
    # SW1
    sw1 = 0
    dcoRange = kr_vals[29]
    if dcoRange == 0: sw1 |= (1 << 0)
    elif dcoRange == 1: sw1 |= (1 << 1)
    elif dcoRange == 2: sw1 |= (1 << 2)
    
    if kr_vals[23]: sw1 |= (1 << 3) # pulseOn
    if kr_vals[24]: sw1 |= (1 << 4) # sawOn
    
    c1 = kr_vals[27]
    c2 = kr_vals[28]
    if not (c1 or c2):
        sw1 |= (1 << 5) # Chorus OFF (bit 5 high means OFF)
    if c1:
        sw1 |= (1 << 6) # Chorus I
        
    kr_syx[16] = sw1
    
    # SW2
    sw2 = 0
    if kr_vals[33]: sw2 |= (1 << 0) # pwmMode
    if kr_vals[34]: sw2 |= (1 << 1) # vcfEnvInv
    if kr_vals[35]: sw2 |= (1 << 2) # vcaMode
    
    hwHpf = 3 - kr_vals[9] # KR uses 0-3 for hpfFreq, HW uses 3-hpfFreq
    sw2 |= ((hwHpf & 0x03) << 3)
    
    kr_syx[17] = sw2
    
    diffs = []
    for i in range(18):
        if our_syx[i] != kr_syx[i]:
            # Exception for PWM: maybe kr_syx clamped it?
            if i == 3 and kr_syx[i] == 105 and our_syx[i] > 105:
                continue
            diffs.append((i, our_syx[i], kr_syx[i]))
            
    return diffs

our_patches = parse_our_bank('Source/Core/FactoryPresets.h')
kr_patches = parse_kr106_bank('temp_ultramaster/Source/KR106_Presets_JUCE.h')

if len(our_patches) != 128:
    print(f"Warning: Expected 128 our patches, got {len(our_patches)}")
if len(kr_patches) != 128:
    print(f"Warning: Expected 128 KR patches, got {len(kr_patches)}")

differences = 0
identical = 0

for i in range(min(len(our_patches), len(kr_patches))):
    diffs = compare_patches(our_patches[i], kr_patches[i])
    if diffs:
        differences += 1
        print(f"Patch {i+1} '{our_patches[i][0]}' vs KR '{kr_patches[i][0]}' differs at bytes:")
        for idx, o_val, k_val in diffs:
            print(f"  Byte {idx}: Our=0x{o_val:02X} ({o_val}), KR=0x{k_val:02X} ({k_val})")
    else:
        identical += 1

print(f"\nComparison Complete. {identical} patches are identical. {differences} patches have differences.")
