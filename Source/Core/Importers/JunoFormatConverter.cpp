#include "JunoFormatConverter.h"

namespace ABD {

ABD::Preset JunoFormatConverter::createPresetFromJunoPatch(const struct JunoPatch& p, int globalIdx) {
    uint8_t b[18];
    b[0] = p.lfoRate;
    b[1] = p.lfoDelay;
    b[2] = p.lfoToDCO;
    b[3] = p.pwm;
    b[4] = p.noise;
    b[5] = p.vcfFreq;
    b[6] = p.resonance;
    b[7] = p.envAmount;
    b[8] = p.lfoToVCF;
    b[9] = p.kybdTracking;
    b[10] = p.vcaLevel;
    b[11] = p.attack;
    b[12] = p.decay;
    b[13] = p.sustain;
    b[14] = p.release;
    b[15] = p.subOsc;
    b[16] = p.sw1;
    b[17] = p.sw2;

    ABD::Preset preset;
    preset.name = p.name;
    preset.category = "Factory";
    preset.state = bytesToValueTree(b);

    if (globalIdx >= 0) {
        preset.originGroup = globalIdx / 64;
        preset.originBank = ((globalIdx % 64) / 8) + 1;
        preset.originPatch = (globalIdx % 8) + 1;
    }
    
    return preset;
}

ABD::Preset JunoFormatConverter::createPresetFromJunoBytes(const juce::String& name, const uint8_t* bytes, const juce::ValueTree& emptyStateTemplate) {
    juce::ignoreUnused(emptyStateTemplate);
    ABD::Preset preset;
    preset.name = name;
    preset.category = "Imported";
    preset.state = bytesToValueTree(bytes);
    return preset;
}

ABD::Preset JunoFormatConverter::createPresetFromJunoBytes(const juce::String& name, const uint8_t* bytes,
                                                            const juce::ValueTree& emptyStateTemplate,
                                                            int baudRate) {
    juce::ignoreUnused(emptyStateTemplate);
    ABD::Preset preset;
    preset.name = name;
    preset.category = "Imported";
    
    // Route to the correct byte mapping based on tape format
    if (baudRate == 340) {
        // Juno-60 DCB format
        preset.state = juno60BytesToValueTree(bytes);
    } else {
        // Default to Juno-106 (also covers 1200 baud and unknown/unset)
        preset.state = bytesToValueTree(bytes);
    }
    
    return preset;
}

juce::ValueTree JunoFormatConverter::bytesToValueTree(const uint8_t* src) {
    juce::ValueTree vt("Preset");
    auto toNorm = [](uint8_t val) { return juce::jlimit(0.0f, 1.0f, (float)val / 127.0f); };
    
    vt.setProperty("lfoRate", toNorm(src[0]), nullptr);
    vt.setProperty("lfoDelay", toNorm(src[1]), nullptr);
    vt.setProperty("lfoToDCO", toNorm(src[2]), nullptr);
    vt.setProperty("pwm", toNorm(src[3]), nullptr);
    vt.setProperty("noise", toNorm(src[4]), nullptr);
    vt.setProperty("vcfFreq", toNorm(src[5]), nullptr);
    vt.setProperty("resonance", toNorm(src[6]), nullptr);
    vt.setProperty("envAmount", toNorm(src[7]), nullptr);
    vt.setProperty("lfoToVCF", toNorm(src[8]), nullptr);
    vt.setProperty("kybdTracking", toNorm(src[9]), nullptr);
    vt.setProperty("vcaLevel", toNorm(src[10]), nullptr);
    vt.setProperty("attack", toNorm(src[11]), nullptr);
    vt.setProperty("decay", toNorm(src[12]), nullptr);
    vt.setProperty("sustain", toNorm(src[13]), nullptr);
    vt.setProperty("release", toNorm(src[14]), nullptr);
    vt.setProperty("subOsc", toNorm(src[15]), nullptr);
    
    SynthParams p;
    JunoProtocol::decodeSW1(src[16], p);
    JunoProtocol::decodeSW2(src[17], p);

    vt.setProperty("vcaMode", p.vcaMode, nullptr); // 0=ENV, 1=GATE
    vt.setProperty("dcoRange", p.dcoRange, nullptr);
    vt.setProperty("pulseOn", p.pulseOn, nullptr);
    vt.setProperty("sawOn", p.sawOn, nullptr);
    vt.setProperty("chorus1", p.chorus1, nullptr);
    vt.setProperty("chorus2", p.chorus2, nullptr);
    vt.setProperty("pwmMode", p.pwmMode, nullptr);
    vt.setProperty("vcfPolarity", p.vcfPolarity, nullptr);
    vt.setProperty("hpfFreq", p.hpfFreq, nullptr);

    return vt;
}

juce::ValueTree JunoFormatConverter::juno60BytesToValueTree(const uint8_t* src) {
    juce::ValueTree vt("Preset");
    auto toNorm = [](uint8_t val) { return juce::jlimit(0.0f, 1.0f, (float)val / 127.0f); };
    
    // === Bytes 0-15: Same slider order as Juno-106 ===
    // The continuous slider controls are in the same panel order on both synths.
    // Note: pwm (src[3]) is set conditionally below after checking the PWM On/Off switch.
    vt.setProperty("lfoRate", toNorm(src[0]), nullptr);
    vt.setProperty("lfoDelay", toNorm(src[1]), nullptr);
    vt.setProperty("lfoToDCO", toNorm(src[2]), nullptr);
    // pwm is NOT set here — handled below with PWM On/Off switch
    vt.setProperty("noise", toNorm(src[4]), nullptr);
    vt.setProperty("vcfFreq", toNorm(src[5]), nullptr);
    vt.setProperty("resonance", toNorm(src[6]), nullptr);
    vt.setProperty("envAmount", toNorm(src[7]), nullptr);
    vt.setProperty("lfoToVCF", toNorm(src[8]), nullptr);
    vt.setProperty("kybdTracking", toNorm(src[9]), nullptr);
    vt.setProperty("vcaLevel", toNorm(src[10]), nullptr);
    vt.setProperty("attack", toNorm(src[11]), nullptr);
    vt.setProperty("decay", toNorm(src[12]), nullptr);
    vt.setProperty("sustain", toNorm(src[13]), nullptr);
    vt.setProperty("release", toNorm(src[14]), nullptr);
    
    // Byte 15: Sub Osc Level (same as Juno-106)
    uint8_t subLevel = src[15];
    
    // === Byte 16 (SW1) - Juno-60 DCB Format ===
    // Bit 0-2: Range (16', 8', 4') - same encoding as Juno-106
    // Bit 3: Saw Wave (1=ON) — NOTE: Swapped vs Juno-106!
    // Bit 4: Pulse Wave (1=ON) — Juno-106 has Pulse@bit3, Saw@bit4
    //   Why? The Juno-60 front panel has SWITCH columns LEFT->RIGHT:
    //   [SAW] [PULSE], matching bit 3 (left switch) = Saw, bit 4 = Pulse.
    //   The Juno-106 panel has them in opposite physical order.
    // Bit 5: Sub Osc On (1=ON) — Juno-60 has dedicated toggle switch
    // Bit 6: PWM On (1=ON) — Juno-60 has dedicated toggle switch
    // Bit 7: PWM Mode (0=LFO, 1=Manual) — Moved from SW2 bit 0 (Juno-106) to SW1 bit 7
    uint8_t sw1 = src[16];
    
    // Range (Bits 0-2) — same as Juno-106
    int dcoRange = 1; // Default 8'
    if (sw1 & (1 << 0))      dcoRange = 0; // 16'
    else if (sw1 & (1 << 1)) dcoRange = 1; // 8'
    else if (sw1 & (1 << 2)) dcoRange = 2; // 4'
    vt.setProperty("dcoRange", dcoRange, nullptr);
    
    // Waves (Bits 3-4)
    bool sawOn   = (sw1 & (1 << 3)) != 0;
    bool pulseOn = (sw1 & (1 << 4)) != 0;
    vt.setProperty("sawOn", sawOn, nullptr);
    vt.setProperty("pulseOn", pulseOn, nullptr);
    
    // Sub Osc On/Off (Bit 5)
    bool subOscOn = (sw1 & (1 << 5)) != 0;
    // If sub oscillator is switched off, force level to 0
    vt.setProperty("subOsc", subOscOn ? toNorm(subLevel) : 0.0f, nullptr);
    
    // PWM On/Off (Bit 6)
    bool pwmOn = (sw1 & (1 << 6)) != 0;
    // If PWM is switched off, force PWM depth to 0
    float pwmValue = pwmOn ? toNorm(src[3]) : 0.0f;
    vt.setProperty("pwm", pwmValue, nullptr);
    
    // PWM Mode (Bit 7): 0=LFO, 1=Manual
    int pwmMode = (sw1 & (1 << 7)) ? 1 : 0;
    vt.setProperty("pwmMode", pwmMode, nullptr);
    
    // === Byte 17 (SW2) - Juno-60 DCB Format ===
    // Bit 0: VCA Mode (0=ENV, 1=GATE)
    // Bit 1: VCF Polarity (0=POS, 1=NEG)
    // Bit 2: Reserved (unused)
    // Bits 3-4: HPF (00=pos3, 01=pos2, 10=pos1, 11=pos0)
    // Bits 5-7: Reserved (unused)
    uint8_t sw2 = src[17];
    
    int vcaMode = (sw2 & (1 << 0)) ? 1 : 0;      // Bit 0: VCA ENV/GATE
    int vcfPolarity = (sw2 & (1 << 1)) ? 1 : 0;  // Bit 1: VCF Polarity
    
    // HPF (Bits 3-4): Same encoding as Juno-106
    int hwHpf = (sw2 >> 3) & 0x03;
    int hpfFreq = 3 - hwHpf;  // 0=FLAT(J60)/BassBoost(J106), 1=Cut1, 2=Cut2, 3=Cut3
    
    vt.setProperty("vcaMode", vcaMode, nullptr);
    vt.setProperty("vcfPolarity", vcfPolarity, nullptr);
    vt.setProperty("hpfFreq", hpfFreq, nullptr);
    
    // === Juno-60 has no Chorus ===
    vt.setProperty("chorus1", false, nullptr);
    vt.setProperty("chorus2", false, nullptr);
    
    // === Set Juno-60 model routing defaults ===
    // The model routing tells the engine to use Juno-60-specific algorithms
    // (HPF frequencies, ADSR curves, etc.)
    vt.setProperty("modelDCO", 1, nullptr);
    vt.setProperty("modelHPF", 1, nullptr);
    vt.setProperty("modelVCF", 1, nullptr);
    vt.setProperty("modelADSR", 1, nullptr);
    vt.setProperty("modelChorus", 1, nullptr);
    vt.setProperty("modelArp", 1, nullptr);
    vt.setProperty("modelPoly", 1, nullptr);
    vt.setProperty("modelPorta", 1, nullptr);
    vt.setProperty("modelUnison", 1, nullptr);
    
    return vt;
}

} // namespace ABD
