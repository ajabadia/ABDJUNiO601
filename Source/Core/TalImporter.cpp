#include "TalImporter.h"

namespace ABD {

TalImporter::ImportResult TalImporter::loadFromFile(const juce::File& file) {
    ImportResult result;

    if (!file.existsAsFile()) {
        result.result = juce::Result::fail("File does not exist: " + file.getFullPathName());
        return result;
    }

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (xml == nullptr || !xml->hasTagName("tal")) {
        result.result = juce::Result::fail("Invalid TAL-U-No-LX XML format");
        return result;
    }

    auto* programs = xml->getChildByName("programs");
    if (programs == nullptr) {
        // Some formats might have the <program> directly or in a different structure
        // But standard bank exports use <programs>
        auto* singleProgram = xml->getChildByName("program");
        if (singleProgram != nullptr) {
            ABD::Preset p;
            p.name = singleProgram->getStringAttribute("programname", file.getFileNameWithoutExtension());
            p.category = "Imported (TAL)";
            p.state = parseProgram(*singleProgram);
            result.presets.push_back(p);
            return result;
        }
        
        result.result = juce::Result::fail("No program entries found in TAL XML");
        return result;
    }

    for (auto* programXml : programs->getChildIterator()) {
        if (programXml->hasTagName("program")) {
            ABD::Preset p;
            p.name = programXml->getStringAttribute("programname", "Unnamed TAL Patch");
            p.category = "Imported (TAL)";
            p.state = parseProgram(*programXml);
            result.presets.push_back(p);
        }
    }

    if (result.presets.empty()) {
        result.result = juce::Result::fail("No valid TAL programs found");
    }

    return result;
}

juce::ValueTree TalImporter::parseProgram(const juce::XmlElement& xml) {
    juce::ValueTree vt("Preset");

    // Helper to get normalized float with fallbacks
    auto getF = [&](const std::vector<const char*>& names, float def = 0.0f) {
        for (auto name : names) {
            if (xml.hasAttribute(name)) return (float)xml.getDoubleAttribute(name);
        }
        return def;
    };

    // Helper to get bool/int with fallbacks
    auto getI = [&](const std::vector<const char*>& names, int def = 0) {
        for (auto name : names) {
            if (xml.hasAttribute(name)) return xml.getIntAttribute(name);
        }
        return def;
    };

    // --- OSCILLATORS ---
    vt.setProperty("lfoToDCO", getF({"dcolfovalue", "lfo_dco"}), nullptr);
    vt.setProperty("pwm", getF({"dcopwmvalue", "pwm_value", "pwm"}, 0.5f), nullptr);
    
    // PWM Mode: in TAL, this is often a float where > 0.5 is Manual
    float talPwmMode = getF({"dcopwmmode", "pwm_mode"}, 0.0f);
    vt.setProperty("pwmMode", (talPwmMode > 0.5f) ? 1 : 0, nullptr); 
    
    vt.setProperty("pulseOn", getI({"dcopulseenabled", "pulse_on"}, 1) > 0, nullptr);
    vt.setProperty("sawOn", getI({"dcosawenabled", "saw_on"}, 1) > 0, nullptr);
    
    vt.setProperty("subOsc", getF({"dcosuboscvolume", "subosc", "sub_level"}, 0.0f), nullptr);
    vt.setProperty("noise", getF({"dconoisevolume", "noise", "noise_level"}, 0.0f), nullptr);
    
    float talOct = getF({"octavetranspose", "transpose", "dco_range"}, 0.5f);
    if (talOct < 0.4f) vt.setProperty("dcoRange", 2);      
    else if (talOct > 0.6f) vt.setProperty("dcoRange", 0); 
    else vt.setProperty("dcoRange", 1);                   

    // --- FILTER ---
    float talHpf = getF({"hpfvalue", "hpf"}, 0.0f);
    vt.setProperty("hpfFreq", quantize(talHpf, 4), nullptr);

    vt.setProperty("vcfFreq", getF({"filtercutoff", "cutoff"}, 1.0f), nullptr);
    vt.setProperty("resonance", getF({"filterresonance", "resonance"}, 0.0f), nullptr);
    
    // vcfPolarity: TAL 1 = Normal (Pos), 0 = Inverted (Neg)
    // JUNiO 601: 0 = Pos, 1 = Neg. So we must flip it.
    int talPol = getI({"filterenvelopemode", "vcf_polarity"}, 1);
    vt.setProperty("vcfPolarity", (talPol == 0) ? 1 : 0, nullptr);
    vt.setProperty("envAmount", getF({"filterenvelopevalue", "env_amount", "envamt"}, 0.5f), nullptr);
    vt.setProperty("lfoToVCF", getF({"filtermodulationvalue", "lfo_vcf"}, 0.0f), nullptr);
    vt.setProperty("kybdTracking", getF({"filterkeyboardvalue", "kybd"}, 0.5f), nullptr);

    // --- VCA ---
    int talVca = getI({"vcamode", "vca_mode"}, 1);
    vt.setProperty("vcaMode", (talVca == 1) ? 0 : 1, nullptr);
    vt.setProperty("vcaLevel", getF({"volume", "mastervolume", "vca_level"}, 0.8f), nullptr);

    // --- ENVELOPE ---
    vt.setProperty("attack", getF({"adsrattack", "attack", "adsr_a"}, 0.001f), nullptr);
    vt.setProperty("decay", getF({"adsrdecay", "decay", "adsr_d"}, 0.1f), nullptr);
    vt.setProperty("sustain", getF({"adsrsustain", "sustain", "adsr_s"}, 0.5f), nullptr);
    vt.setProperty("release", getF({"adsrrelease", "release", "adsr_r"}, 0.1f), nullptr);

    // --- LFO ---
    vt.setProperty("lfoRate", getF({"lforate", "lfo_rate"}, 0.5f), nullptr);
    vt.setProperty("lfoDelay", getF({"lfodelaytime", "lfo_delay"}, 0.0f), nullptr);

    // --- CHORUS ---
    vt.setProperty("chorus1", getI({"chorus1enable", "chorus1"}, 0) > 0, nullptr);
    vt.setProperty("chorus2", getI({"chorus2enable", "chorus2"}, 0) > 0, nullptr);

    // --- PERFORMANCE / BENDER ---
    vt.setProperty("benderToDCO", getF({"controlbenderdco", "bender_dco"}, 0.1f), nullptr);
    vt.setProperty("benderToVCF", getF({"controlbenderfilter", "bender_vcf"}, 0.0f), nullptr);

    return vt;
}

} // namespace ABD
