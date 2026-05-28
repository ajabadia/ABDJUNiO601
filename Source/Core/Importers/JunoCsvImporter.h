#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../BaseClass/PresetManagerBase.h"

namespace ABD {

class JunoCsvImporter {
public:
    struct ImportResult {
        std::vector<ABD::Preset> presets;
        juce::Result result = juce::Result::ok();
    };

    static ImportResult loadFromFile(const juce::File& file);
    static bool exportToFile(const juce::File& file, const std::vector<ABD::Preset>& presets);

private:
    static float parseValue(const juce::String& valStr);
    static int parseIntValue(const juce::String& valStr);
};

} // namespace ABD
