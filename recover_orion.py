import os, sys, json

input_path = r'd:\\desarrollos\\ABDJUNiO601\\JUNO106\\patches\\best_of_orion\\Best of Orion.106'
output_syx = r'd:\\desarrollos\\ABDJUNiO601\\best_of_orion.syx'
output_json = r'd:\\desarrollos\\ABDJUNiO601\\best_of_orion.json'

print(f"Reading {input_path}...")

with open(input_path, 'rb') as f:
    data = f.read()

if data[:6] != b'!j106\\':
    print("Error: Invalid .106 header")
    sys.exit(1)

offset = 6
patches = []

# Helper for JSON
def to_norm(val):
    return f"{float(val)/127.0:.8f}"

while offset < len(data):
    # Try to find null terminator for name
    end = data.find(b'\x00', offset, offset + 32)
    if end == -1:
        # Fallback if no null found implies fixed width or error, try 16 bytes
        name_bytes = data[offset:offset+16]
        name = name_bytes.decode('utf-8', errors='ignore').strip()
        offset += 16
    else:
        name = data[offset:end].decode('utf-8', errors='ignore')
        offset = end + 1
    
    # Safety check for remaining data
    if offset + 18 > len(data):
        break
        
    patch_bytes = data[offset:offset+18]
    offset += 18
    
    patches.append({
        "name": name,
        "bytes": patch_bytes
    })

print(f"Extracted {len(patches)} presets.")

# --- Generate SysEx ---
sysex_data = bytearray()
for p in patches:
    # Roland Header: F0 41 30 00 00
    msg = bytearray([0xF0, 0x41, 0x30, 0x00, 0x00]) 
    msg.extend(p['bytes'])
    msg.append(0xF7)
    sysex_data.extend(msg)

with open(output_syx, 'wb') as f:
    f.write(sysex_data)
print(f"Saved {output_syx}")

# --- Generate JSON ---
json_presets = []
for p in patches:
    b = p['bytes']
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

    xml_parts = ['<Parameters']
    for k, v in params.items():
        xml_parts.append(f'{k}="{v}"')
    xml_parts.append('/>')
    
    json_presets.append({
        "name": p['name'],
        "state": " ".join(xml_parts)
    })

json_output = {
    "libraryName": "Best of Orion",
    "presets": json_presets
}

with open(output_json, 'w', encoding='utf-8') as f:
    json.dump(json_output, f, indent=2)
print(f"Saved {output_json}")
