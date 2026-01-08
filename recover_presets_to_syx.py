import os, sys

# Paths
input_106 = r'd:\\desarrollos\\ABDJUNiO601\\JUNO106\\patches\\factory_patches\\factory patches.106'
output_syx = r'd:\\desarrollos\\ABDJUNiO601\\factory_presets.syx'

with open(input_106, 'rb') as f:
    data = f.read()

# Verify header
if data[:6] != b'!j106\\':
    print("Error: Invalid .106 header")
    sys.exit(1)

offset = 6
patches = []

# Extract patches
while offset < len(data):
    # Find name delimiter
    end = data.find(b'\x00', offset, offset + 32)
    if end == -1:
        # Fixed length fallback
        offset += 16
    else:
        offset = end + 1
    
    # Check if data remains
    if offset + 18 > len(data):
        break
        
    # Read 18 bytes of patch data
    patch_data = data[offset:offset+18]
    patches.append(patch_data)
    offset += 18
    
    # Retrieve only the first 128 patches (Factory A + B)
    if len(patches) >= 128:
        break

print(f"Extracted {len(patches)} patches.")

# Create valid Roland SysEx Dump for each patch
# Format: F0 41 30 [Channel=0] [PatchNum=0] [18 bytes body] F7
# Note: PatchNum is ignored by import code usually, but we include it.
# The `JunoSysEx::parseMessage` logic expects 18 bytes starting at offset 4 (0-based from data body excluding F0 41 30 ch).
# Wait, `parseMessage`: type(30) chan(0) -> data[4] is patchNum?
# `if (type == kMsgPatchDump && size >= 20) ... dumpBody18Bytes[i] = data[4 + i];`
# data[0]=F0, data[1]=41, data[2]=30, data[3]=ch.
# JUCE `getSysExData()` usually includes F0.
# So `msg.getSysExData()`: [F0, 41, 30, ch, patchNum, 18 bytes..., F7]
# `parseMessage` skips F0(0), 41(1). `type` is data[1] (41? No). 
# JUCE: `getSysExData()` returns pointer to F0.
# File `JunoSysEx.h`:
# `type = data[1];` (41 is ID, so data[1] is 41). No.
# `if (data[0] != kRolandID)` -> Wait, `JunoSysEx.h` line 80: `if (size < 3 || data[0] != kRolandID)`
# This implies `data` points AFTER F0?
# JUCE `MidiMessage::getSysExData()` returns the raw array starting with F0.
# Let's re-read `JunoSysEx.h` carefully.
# Line 78: `const uint8_t* data = msg.getSysExData();` -> This is F0.
# Line 80: `if (data[0] != kRolandID)` -> Checks if F0 is 41? No, F0 is status.
# Standard: F0 41 ...
# If `data[0]` is checked against `kRolandID` (0x41), then `data` must be pointing to the byte AFTER F0.
# BUT `msg.getSysExData()` returns START of message (F0).
# So `JunoSysEx.h` might assume `data` is the payload? 
# "The getSysExData() method returns a pointer to the hex data... including the leading 0xf0 and trailing 0xf7."
# So `data[0]` is 0xF0. `data[1]` is 0x41 (Roland ID).
# `JunoSysEx.h` logic: `if (... data[0] != kRolandID)` => This checks 0xF0 against 0x41. usage error in code?
# OR `JunoSysEx::parseMessage` might be buggy? 
# Line 82: `type = data[1];` -> 0x41?
# kMsgPatchDump = 0x30.
# If `data` is `F0 41 30 ...`, then `data[0]=F0`.
# If the code checks `data[0] != kRolandID`, it will FAIL for standard messages.
# UNLESS `msg.getSysExData()` in that specific JUCE version or context behaves differently.
# However, assuming standard `F0 41 30` generation is safer.

sysex_data = bytearray()
for patch in patches:
    # Construct SysEx: F0 41 30 00 00 [18 bytes] F7
    # 0xF0
    # 0x41 (Roland ID)
    # 0x30 (Patch Dump cmd)
    # 0x00 (Channel 0)
    # 0x00 (Format/Group?) -> In `createPatchDump` it puts `0x00` at data[4].
    # Then 16 params + SW1 + SW2 = 18 bytes.
    # Total length: 1+1+1+1+1+18+1 = 24 bytes?
    # `JunoSysEx.h`: `data[4] = 0x00; // Patch Number placeholder`
    # `for (int i = 0; i < 16; ++i) data[5 + i] = ...`
    # It constructs 23 bytes (CreatePatchDump params).
    msg = bytearray([0xF0, 0x41, 0x30, 0x00, 0x00]) # Header
    msg.extend(patch) # 18 bytes raw
    msg.append(0xF7) # EOX
    sysex_data.extend(msg)

with open(output_syx, 'wb') as f:
    f.write(sysex_data)

print(f"Generated {output_syx} with {len(patches)} SysEx messages.")
