import os, sys, json

input_106 = r'd:\\desarrollos\\ABDJUNiO601\\JUNO106\\patches\\factory_patches\\factory patches.106'
output_json = r'd:\\desarrollos\\ABDJUNiO601\\factory_presets.json'

with open(input_106, 'rb') as f:
    data = f.read()

if data[:6] != b'!j106\\':
    print("Invalid .106 header")
    sys.exit(1)

offset = 6
patches = []

# Helper to normalize 0-127 to float string
def to_norm(val):
    return f"{float(val)/127.0:.8f}" # Match precision roughly

while offset < len(data):
    end = data.find(b'\x00', offset, offset + 32)
    if end == -1:
        name_bytes = data[offset:offset+16]
        name = name_bytes.rstrip(b'\x00').decode('utf-8', errors='ignore')
        offset += 16
    else:
        name = data[offset:end].decode('utf-8', errors='ignore')
        offset = end + 1
    
    if offset + 18 > len(data):
        break
    
    b = data[offset:offset+18] # 18 bytes
    offset += 18

    # Parse parameters
    # Bytes mapping from PresetManager.cpp
    # 0: lfoRate, 1: lfoDelay, 2: lfoToDCO, 3: pwm, 4: noise
    # 5: vcfFreq, 6: resonance, 7: envAmount, 8: lfoToVCF, 9: kybdTracking
    # 10: vcaLevel, 11: attack, 12: decay, 13: sustain, 14: release, 15: subOsc
    
    params = {}
    params['lfoRate'] = to_norm(b[0])
    params['lfoDelay'] = to_norm(b[1])
    params['lfoToDCO'] = to_norm(b[2])
    params['pwm'] = to_norm(b[3])
    params['noise'] = to_norm(b[4])
    params['vcfFreq'] = to_norm(b[5])
    params['resonance'] = to_norm(b[6])
    params['envAmount'] = to_norm(b[7])
    params['lfoToVCF'] = to_norm(b[8])
    params['kybdTracking'] = to_norm(b[9])
    params['vcaLevel'] = to_norm(b[10])
    params['attack'] = to_norm(b[11])
    params['decay'] = to_norm(b[12])
    params['sustain'] = to_norm(b[13])
    params['release'] = to_norm(b[14])
    params['subOsc'] = to_norm(b[15])
    
    sw1 = b[16]
    # int range = (sw1 & (1 << 0)) ? 0 : (sw1 & (1 << 1) ? 1 : 2);
    if (sw1 & 1): dcoWait = 0
    elif (sw1 & 2): dcoWait = 1
    else: dcoWait = 2
    params['dcoRange'] = str(dcoWait)
    
    params['pulseOn'] = "1" if (sw1 & 8) else "0"
    params['sawOn'] = "1" if (sw1 & 16) else "0"
    
    chorusOn = ((sw1 & 32) == 0)
    chorusI = ((sw1 & 64) != 0)
    
    params['chorus1'] = "1" if (chorusOn and chorusI) else "0"
    params['chorus2'] = "1" if (chorusOn and not chorusI) else "0"

    sw2 = b[17]
    params['pwmMode'] = "1" if (sw2 & 1) else "0"
    params['vcaMode'] = "1" if (sw2 & 2) else "0"
    params['vcfPolarity'] = "1" if (sw2 & 4) else "0"
    
    hpfBits = (sw2 >> 3) & 0x03
    params['hpfFreq'] = str(3 - hpfBits)

    # Build XML string
    # <Parameters attr="val" ... />
    xml_parts = ['<Parameters']
    for k, v in params.items():
        xml_parts.append(f'{k}="{v}"')
    xml_parts.append('/>')
    xml_state = " ".join(xml_parts)
    
    patches.append({
        "name": name,
        "state": xml_state
    })
    
    if len(patches) >= 128:
        break

output_structure = {
    "libraryName": "Factory Presets",
    "presets": patches
}

with open(output_json, 'w', encoding='utf-8') as f:
    json.dump(output_structure, f, indent=2)

print(f"Generated {output_json} with {len(patches)} presets.")
