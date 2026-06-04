import csv
import re
import os

def parse_juno60_presets(csv_path):
    presets = []
    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        header = next(reader)
        for row in reader:
            if not row:
                continue
            name = row[0]
            # Strip brackets from [value]
            def get_val(val_str):
                cleaned = val_str.replace('[', '').replace(']', '')
                return int(cleaned) if cleaned else 0
            
            lfoRate = get_val(row[1])
            lfoDelay = get_val(row[2])
            lfoToDCO = get_val(row[3])
            pwm = get_val(row[4])
            subOsc = get_val(row[5])
            noise = get_val(row[6])
            hpf = get_val(row[7])
            vcfFreq = get_val(row[8])
            resonance = get_val(row[9])
            envAmount = get_val(row[10])
            lfoToVCF = get_val(row[11])
            kybdTracking = get_val(row[12])
            vcaLevel = get_val(row[13])
            attack = get_val(row[14])
            decay = get_val(row[15])
            sustain = get_val(row[16])
            release = get_val(row[17])
            pulse = get_val(row[18])
            saw = get_val(row[19])
            subSw = get_val(row[20])
            chorusOff = get_val(row[21])
            chorusI = get_val(row[22])
            chorusII = get_val(row[23])
            octave = get_val(row[24]) # 0=16', 1=8', 2=4'
            pwmMode = get_val(row[26])
            vcfPolarity = get_val(row[27])
            vcaMode = get_val(row[28])
            
            # Pack sw1
            sw1 = 0
            if octave == 0: sw1 |= (1 << 0)
            elif octave == 1: sw1 |= (1 << 1)
            elif octave == 2: sw1 |= (1 << 2)
            if pulse: sw1 |= (1 << 3)
            if saw: sw1 |= (1 << 4)
            if chorusOff: sw1 |= (1 << 5)
            if chorusI: sw1 |= (1 << 6)
            
            # Pack sw2
            sw2 = 0
            if pwmMode == 1: sw2 |= (1 << 0)
            if vcfPolarity == 1: sw2 |= (1 << 1)
            if vcaMode == 1: sw2 |= (1 << 2)
            hwHpf = 3 - hpf
            sw2 |= (hwHpf & 0x03) << 3
            
            presets.append({
                'name': name,
                'lfoRate': lfoRate, 'lfoDelay': lfoDelay, 'lfoToDCO': lfoToDCO,
                'pwm': pwm, 'noise': noise, 'vcfFreq': vcfFreq, 'resonance': resonance,
                'envAmount': envAmount, 'lfoToVCF': lfoToVCF, 'kybdTracking': kybdTracking,
                'vcaLevel': vcaLevel, 'attack': attack, 'decay': decay, 'sustain': sustain,
                'release': release, 'subOsc': subOsc, 'sw1': sw1, 'sw2': sw2
            })
    return presets

def parse_david_churcher_presets(h_path):
    presets = []
    # MakePreset("Acid bass", 0., 0., 90., 0.023622, ...);
    pattern = re.compile(r'MakePreset\s*\(\s*"(.*?)"\s*,\s*(.*?)\s*\)\s*;')
    with open(h_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    matches = pattern.findall(content)
    # David Churcher presets are the ones after index 127
    churcher_matches = matches[128:]
    
    for name, args_str in churcher_matches:
        args = [a.strip() for a in args_str.split(',')]
        
        def to7(v):
            val = float(v)
            if val <= 1.0001:
                return int(round(val * 127))
            else:
                return int(round(val))
        
        lfoRate = to7(args[3])
        lfoDelay = to7(args[4])
        lfoToDCO = to7(args[5])
        pwm = to7(args[6])
        subOsc = to7(args[7])
        noise = to7(args[8])
        hpf = int(float(args[9])) # 0..3
        vcfFreq = to7(args[10])
        resonance = to7(args[11])
        envAmount = to7(args[12])
        lfoToVCF = to7(args[13])
        kybdTracking = to7(args[14])
        vcaLevel = to7(args[15])
        attack = to7(args[16])
        decay = to7(args[17])
        sustain = to7(args[18])
        release = to7(args[19])
        
        pulse = int(float(args[23]))
        saw = int(float(args[24]))
        subSw = int(float(args[25]))
        chorusOff = int(float(args[26]))
        chorusI = int(float(args[27]))
        chorusII = int(float(args[28]))
        octave = int(float(args[29])) # octTranspose (0=16', 1=8', 2=4' in gen_presets.py input)
        pwmMode = int(float(args[33]))
        vcfPolarity = int(float(args[34]))
        vcaMode = int(float(args[35]))
        
        # Pack sw1
        sw1 = 0
        if octave == 0: sw1 |= (1 << 0)
        elif octave == 1: sw1 |= (1 << 1)
        elif octave == 2: sw1 |= (1 << 2)
        if pulse: sw1 |= (1 << 3)
        if saw: sw1 |= (1 << 4)
        if chorusOff: sw1 |= (1 << 5)
        if chorusI: sw1 |= (1 << 6)
        
        # Pack sw2
        sw2 = 0
        if pwmMode == 1: sw2 |= (1 << 0)
        if vcfPolarity == 1: sw2 |= (1 << 1)
        if vcaMode == 1: sw2 |= (1 << 2)
        hwHpf = 3 - hpf
        sw2 |= (hwHpf & 0x03) << 3
        
        presets.append({
            'name': name,
            'lfoRate': lfoRate, 'lfoDelay': lfoDelay, 'lfoToDCO': lfoToDCO,
            'pwm': pwm, 'noise': noise, 'vcfFreq': vcfFreq, 'resonance': resonance,
            'envAmount': envAmount, 'lfoToVCF': lfoToVCF, 'kybdTracking': kybdTracking,
            'vcaLevel': vcaLevel, 'attack': attack, 'decay': decay, 'sustain': sustain,
            'release': release, 'subOsc': subOsc, 'sw1': sw1, 'sw2': sw2
        })
    return presets

def main():
    csv_path = r'D:\desarrollos\ABDSynths\synthClones\kr106\tools\juno60_presets.csv'
    h_path = r'D:\desarrollos\ABDSynths\synthClones\kr106\KR106_Presets.h'
    
    juno60 = parse_juno60_presets(csv_path)
    churcher = parse_david_churcher_presets(h_path)
    
    # Read current FactoryPresets.h to preserve the 128 Juno-106 presets exactly
    with open(r'D:\desarrollos\ABDSynths\ABDJUNiO601\Source\Core\FactoryPresets.h', 'r', encoding='utf-8') as f:
        original_content = f.read()
    
    # Find start and end of junoFactoryPatches array
    start_marker = 'static const JunoPatch junoFactoryPatches[128] = {'
    end_marker = '};'
    
    start_idx = original_content.find(start_marker)
    if start_idx == -1:
        print("Error: Could not find junoFactoryPatches in original FactoryPresets.h")
        return
        
    end_idx = original_content.find(end_marker, start_idx)
    original_array = original_content[start_idx : end_idx + len(end_marker)]
    
    out_lines = [
        '#pragma once',
        '#include <JuceHeader.h>',
        '',
        'struct JunoPatch {',
        '    const char* name;',
        '    uint8_t lfoRate, lfoDelay, lfoToDCO, pwm, noise, vcfFreq, resonance, envAmount, lfoToVCF, kybdTracking, vcaLevel, attack, decay, sustain, release, subOsc, sw1, sw2;',
        '};',
        '',
        original_array,
        '',
        '// --- Juno-60 Factory Patches ---',
        'static constexpr int kNumJuno60FactoryPatches = ' + str(len(juno60)) + ';',
        'static const JunoPatch juno60FactoryPatches[] = {'
    ]
    
    for p in juno60:
        out_lines.append(f'    {{"{p["name"]}", 0x{p["lfoRate"]:02X}, 0x{p["lfoDelay"]:02X}, 0x{p["lfoToDCO"]:02X}, 0x{p["pwm"]:02X}, 0x{p["noise"]:02X}, 0x{p["vcfFreq"]:02X}, 0x{p["resonance"]:02X}, 0x{p["envAmount"]:02X}, 0x{p["lfoToVCF"]:02X}, 0x{p["kybdTracking"]:02X}, 0x{p["vcaLevel"]:02X}, 0x{p["attack"]:02X}, 0x{p["decay"]:02X}, 0x{p["sustain"]:02X}, 0x{p["release"]:02X}, 0x{p["subOsc"]:02X}, 0x{p["sw1"]:02X}, 0x{p["sw2"]:02X}}},')
    out_lines.append('};')
    out_lines.append('')
    
    out_lines.extend([
        '// --- David Churcher Custom Patches ---',
        'static constexpr int kNumDavidChurcherPatches = ' + str(len(churcher)) + ';',
        'static const JunoPatch davidChurcherPatches[] = {'
    ])
    
    for p in churcher:
        safe_name = p["name"].replace('"', '\\"')
        out_lines.append(f'    {{"{safe_name}", 0x{p["lfoRate"]:02X}, 0x{p["lfoDelay"]:02X}, 0x{p["lfoToDCO"]:02X}, 0x{p["pwm"]:02X}, 0x{p["noise"]:02X}, 0x{p["vcfFreq"]:02X}, 0x{p["resonance"]:02X}, 0x{p["envAmount"]:02X}, 0x{p["lfoToVCF"]:02X}, 0x{p["kybdTracking"]:02X}, 0x{p["vcaLevel"]:02X}, 0x{p["attack"]:02X}, 0x{p["decay"]:02X}, 0x{p["sustain"]:02X}, 0x{p["release"]:02X}, 0x{p["subOsc"]:02X}, 0x{p["sw1"]:02X}, 0x{p["sw2"]:02X}}},')
    out_lines.append('};')
    
    with open(r'D:\desarrollos\ABDSynths\ABDJUNiO601\Source\Core\FactoryPresets.h', 'w', encoding='utf-8') as f:
        f.write('\n'.join(out_lines) + '\n')
        
    print("FactoryPresets.h generated successfully!")

if __name__ == '__main__':
    main()
