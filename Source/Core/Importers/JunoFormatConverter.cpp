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

} // namespace ABD
