#include "TalImporter.h"
#include <memory>

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

    // Helper to get normalized float
    auto getF = [&](const char* name, float def = 0.0f) {
        return (float)xml.getDoubleAttribute(name, (double)def);
    };

    // Helper to get bool/int
    auto getI = [&](const char* name, int def = 0) {
        return xml.getIntAttribute(name, def);
    };

    // --- OSCILLATORS ---
    vt.setProperty("lfoToDCO", getF("dcolfovalue"), nullptr);
    vt.setProperty("pwm", getF("dcopwmvalue", 0.5f), nullptr);
    vt.setProperty("pwmMode", getI("dcopwmmode", 0), nullptr); 
    
    vt.setProperty("pulseOn", getI("dcopulseenabled", 1) > 0, nullptr);
    vt.setProperty("sawOn", getI("dcosawenabled", 1) > 0, nullptr);
    vt.setProperty("subOsc", getF("dcosuboscvolume", 0.0f), nullptr);
    vt.setProperty("noise", getF("dconoisevolume", 0.0f), nullptr);
    
    float talOct = getF("octavetranspose", 0.5f);
    if (talOct < 0.4f) vt.setProperty("dcoRange", 2, nullptr);      // 16'
    else if (talOct > 0.6f) vt.setProperty("dcoRange", 0, nullptr); // 4'
    else vt.setProperty("dcoRange", 1, nullptr);                   // 8'

    // --- FILTER ---
    float talHpf = getF("hpfvalue", 0.0f);
    vt.setProperty("hpfFreq", quantize(talHpf, 4), nullptr);

    vt.setProperty("vcfFreq", getF("filtercutoff", 1.0f), nullptr);
    vt.setProperty("resonance", getF("filterresonance", 0.0f), nullptr);
    vt.setProperty("vcfPolarity", getI("filterenvelopemode", 0), nullptr);
    vt.setProperty("envAmount", getF("filterenvelopevalue", 0.5f), nullptr);
    vt.setProperty("lfoToVCF", getF("filtermodulationvalue", 0.0f), nullptr);
    vt.setProperty("kybdTracking", getF("filterkeyboardvalue", 0.5f), nullptr);

    // --- VCA ---
    int talVca = getI("vcamode", 1);
    vt.setProperty("vcaMode", (talVca == 1) ? 0 : 1, nullptr);
    vt.setProperty("vcaLevel", getF("volume", 0.8f), nullptr);

    // --- ENVELOPE ---
    vt.setProperty("attack", getF("adsrattack", 0.001f), nullptr);
    vt.setProperty("decay", getF("adsrdecay", 0.1f), nullptr);
    vt.setProperty("sustain", getF("adsrsustain", 0.5f), nullptr);
    vt.setProperty("release", getF("adsrrelease", 0.1f), nullptr);

    // --- LFO ---
    vt.setProperty("lfoRate", getF("lforate", 0.5f), nullptr);
    vt.setProperty("lfoDelay", getF("lfodelaytime", 0.0f), nullptr);

    // --- CHORUS ---
    vt.setProperty("chorus1", getI("chorus1enable", 0) > 0, nullptr);
    vt.setProperty("chorus2", getI("chorus2enable", 0) > 0, nullptr);

    // --- PERFORMANCE ---
    vt.setProperty("benderToDCO", getF("controlbenderdco", 0.1f), nullptr);
    vt.setProperty("benderToVCF", getF("controlbenderfilter", 0.0f), nullptr);

    return vt;
}

} // namespace ABD
