#include "JunoCsvImporter.h"

namespace ABD {

float JunoCsvImporter::parseValue(const juce::String& valStr) {
    if (valStr.startsWith("[") && valStr.endsWith("]")) {
        return valStr.substring(1, valStr.length() - 2).getFloatValue();
    }
    return valStr.getFloatValue();
}

int JunoCsvImporter::parseIntValue(const juce::String& valStr) {
    if (valStr.startsWith("[") && valStr.endsWith("]")) {
        return valStr.substring(1, valStr.length() - 2).getIntValue();
    }
    return valStr.getIntValue();
}

JunoCsvImporter::ImportResult JunoCsvImporter::loadFromFile(const juce::File& file) {
    ImportResult res;
    if (!file.existsAsFile()) {
        res.result = juce::Result::fail("File does not exist.");
        return res;
    }

    juce::StringArray lines;
    file.readLines(lines);

    if (lines.size() < 2) {
        res.result = juce::Result::fail("CSV file is empty or missing headers.");
        return res;
    }

    // Parse header
    juce::StringArray headers = juce::StringArray::fromTokens(lines[0], ",", "\"");
    res.columnHeaders = headers;
    
    // Process rows
    for (int i = 1; i < lines.size(); ++i) {
        if (lines[i].trim().isEmpty()) continue;
        
        juce::StringArray tokens = juce::StringArray::fromTokens(lines[i], ",", "\"");
        if (tokens.size() < headers.size()) continue;

        ABD::Preset p;
        p.name = tokens[0].trim();
        if (p.name.startsWith("\"") && p.name.endsWith("\"")) {
            p.name = p.name.substring(1, p.name.length() - 2);
        }
        
        juce::ValueTree vt("Preset");
        
        for (int c = 1; c < headers.size() && c < tokens.size(); ++c) {
            juce::String header = headers[c].trim();
            juce::String valStr = tokens[c].trim();
            
            if (header == "kLfoRate") vt.setProperty("lfoRate", parseValue(valStr), nullptr);
            else if (header == "kLfoDelay") vt.setProperty("lfoDelay", parseValue(valStr), nullptr);
            else if (header == "kDcoLfo") vt.setProperty("lfoToDCO", parseValue(valStr), nullptr);
            else if (header == "kDcoPwm") vt.setProperty("pwm", parseValue(valStr), nullptr);
            else if (header == "kDcoNoise") vt.setProperty("noise", parseValue(valStr), nullptr);
            else if (header == "kHpfFreq") vt.setProperty("hpfFreq", parseIntValue(valStr), nullptr);
            else if (header == "kVcfFreq") vt.setProperty("vcfFreq", parseValue(valStr), nullptr);
            else if (header == "kVcfRes") vt.setProperty("resonance", parseValue(valStr), nullptr);
            else if (header == "kVcfEnv") vt.setProperty("envAmount", parseValue(valStr), nullptr);
            else if (header == "kVcfLfo") vt.setProperty("lfoToVCF", parseValue(valStr), nullptr);
            else if (header == "kVcfKbd") vt.setProperty("kybdTracking", parseValue(valStr), nullptr);
            else if (header == "kVcaLevel") vt.setProperty("vcaLevel", parseValue(valStr), nullptr);
            else if (header == "kEnvA") vt.setProperty("attack", parseValue(valStr), nullptr);
            else if (header == "kEnvD") vt.setProperty("decay", parseValue(valStr), nullptr);
            else if (header == "kEnvS") vt.setProperty("sustain", parseValue(valStr), nullptr);
            else if (header == "kEnvR") vt.setProperty("release", parseValue(valStr), nullptr);
            else if (header == "kDcoSub") vt.setProperty("subOsc", parseValue(valStr), nullptr);
            
            else if (header == "kDcoPulse") vt.setProperty("pulseOn", parseIntValue(valStr) != 0, nullptr);
            else if (header == "kDcoSaw") vt.setProperty("sawOn", parseIntValue(valStr) != 0, nullptr);
            else if (header == "kChorusI") vt.setProperty("chorus1", parseIntValue(valStr) != 0, nullptr);
            else if (header == "kChorusII") vt.setProperty("chorus2", parseIntValue(valStr) != 0, nullptr);
            
            else if (header == "kPwmMode") vt.setProperty("pwmMode", parseIntValue(valStr), nullptr);
            else if (header == "kVcfEnvInv") vt.setProperty("vcfPolarity", parseIntValue(valStr), nullptr);
            else if (header == "kVcaMode") vt.setProperty("vcaMode", parseIntValue(valStr), nullptr);
            else if (header == "kOctTranspose") vt.setProperty("dcoRange", parseIntValue(valStr), nullptr);
        }
        
        p.state = vt;
        res.presets.push_back(p);
    }
    
    return res;
}

bool JunoCsvImporter::exportToFile(const juce::File& file, const std::vector<ABD::Preset>& presets) {
    juce::String csv = "name,kLfoRate,kLfoDelay,kDcoLfo,kDcoPwm,kDcoNoise,kHpfFreq,kVcfFreq,kVcfRes,kVcfEnv,kVcfLfo,kVcfKbd,kVcaLevel,kEnvA,kEnvD,kEnvS,kEnvR,kDcoSub,kDcoPulse,kDcoSaw,kChorusI,kChorusII,kPwmMode,kVcfEnvInv,kVcaMode,kOctTranspose\n";
    
    for (const auto& p : presets) {
        juce::String row = "\"" + p.name + "\",";
        const auto& vt = p.state;
        
        auto f = [&](const juce::Identifier& id, float def = 0.0f) { return juce::String(vt.getProperty(id, def).operator float()); };
        auto i = [&](const juce::Identifier& id, int def = 0) { return "[" + juce::String(vt.getProperty(id, def).operator int()) + "]"; };
        auto b = [&](const juce::Identifier& id, bool def = false) { return "[" + juce::String(vt.getProperty(id, def).operator bool() ? 1 : 0) + "]"; };

        row += f("lfoRate") + ",";
        row += f("lfoDelay") + ",";
        row += f("lfoToDCO") + ",";
        row += f("pwm") + ",";
        row += f("noise") + ",";
        row += i("hpfFreq") + ",";
        row += f("vcfFreq") + ",";
        row += f("resonance") + ",";
        row += f("envAmount") + ",";
        row += f("lfoToVCF") + ",";
        row += f("kybdTracking") + ",";
        row += f("vcaLevel") + ",";
        row += f("attack") + ",";
        row += f("decay") + ",";
        row += f("sustain") + ",";
        row += f("release") + ",";
        row += f("subOsc") + ",";
        
        row += b("pulseOn") + ",";
        row += b("sawOn") + ",";
        row += b("chorus1") + ",";
        row += b("chorus2") + ",";
        
        row += i("pwmMode") + ",";
        row += i("vcfPolarity") + ",";
        row += i("vcaMode") + ",";
        row += i("dcoRange");
        
        csv += row + "\n";
    }
    
    return file.replaceWithText(csv);
}

} // namespace ABD
