import re

with open('temp_ultramaster/Source/KR106_Presets_JUCE.h', 'r') as f:
    text = f.read()

matches = re.findall(r'\{\"(.*?)\",\s*\{(.*?)\}\}', text)
j106_matches = matches[128:]

# The 44 parameters in KR106:
# kBenderDco = 0, kBenderVcf, kArpRate, kLfoRate, kLfoDelay, kDcoLfo, kDcoPwm, kDcoSub, kDcoNoise, kHpfFreq, kVcfFreq, kVcfRes, kVcfEnv, kVcfLfo, kVcfKbd, kVcaLevel, kEnvA, kEnvD, kEnvS, kEnvR, kTranspose, kHold, kArpeggio, kDcoPulse, kDcoSaw, kDcoSubSw, kChorusOff, kChorusI, kChorusII, kOctTranspose, kArpMode, kArpRange, kLfoMode, kPwmMode, kVcfEnvInv, kVcaMode, kBender, kTuning, kPower, kPortaMode, kPortaRate, kTransposeOffset, kBenderLfo, kAdsrMode

csv = "name,kBenderDco,kBenderVcf,kArpRate,kLfoRate,kLfoDelay,kDcoLfo,kDcoPwm,kDcoSub,kDcoNoise,kHpfFreq,kVcfFreq,kVcfRes,kVcfEnv,kVcfLfo,kVcfKbd,kVcaLevel,kEnvA,kEnvD,kEnvS,kEnvR,kTranspose,kHold,kArpeggio,kDcoPulse,kDcoSaw,kDcoSubSw,kChorusOff,kChorusI,kChorusII,kOctTranspose,kArpMode,kArpRange,kLfoMode,kPwmMode,kVcfEnvInv,kVcaMode,kBender,kTuning,kPower,kPortaMode,kPortaRate,kTransposeOffset,kBenderLfo,kAdsrMode\n"

for name, vals in j106_matches:
    val_arr = [int(v.strip()) for v in vals.split(',')]
    row = f'"{name}"'
    
    # We must format floats as 0.0-1.0 and ints as [val]
    # Int fields in KR106: kHpfFreq(9), kDcoPulse(23), kDcoSaw(24), kDcoSubSw(25), kChorusOff(26), kChorusI(27), kChorusII(28), kOctTranspose(29), kArpMode(30), kArpRange(31), kLfoMode(32), kPwmMode(33), kVcfEnvInv(34), kVcaMode(35), kPower(38), kPortaMode(39), kAdsrMode(43)
    int_fields = {9, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 38, 39, 43}
    
    for i, v in enumerate(val_arr):
        if i in int_fields:
            row += f',[{v}]'
        else:
            if i == 6: # kDcoPwm needs to be divided by 105 instead of 127 based on SysEx clamping
                norm = min(v, 105) / 105.0
            else:
                norm = v / 127.0
            row += f',{norm:.6f}'
            
    csv += row + "\n"

with open('J106_Factory_Bank.csv', 'w') as f:
    f.write(csv)

print("Created J106_Factory_Bank.csv")
