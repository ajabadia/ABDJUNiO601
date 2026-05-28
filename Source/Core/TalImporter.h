#pragma once

#include "JunoConstants.h"
#include <JuceHeader.h>
#include "../BaseClass/PresetManagerBase.h"

namespace ABD {

/**
 * @class TalImporter
 * @brief Handles importing presets from TAL-U-No-LX (.pjunoxl) XML files.
 */
class TalImporter {
public:
    struct ImportResult {
        std::vector<ABD::Preset> presets;
        juce::Result result = juce::Result::ok();
    };

    /**
     * @brief Parses a TAL-U-No-LX XML file and returns a list of candidate presets.
     */
    static ImportResult loadFromFile(const juce::File& file);

private:
    /**
     * @brief Maps a single <program> element from TAL XML to our internal state.
     */
    static juce::ValueTree parseProgram(const juce::XmlElement& programXml);

    /**
     * @brief Quantizes a continuous normalized value to a discrete set of steps.
     */
    static int quantize(float normalizedValue, int numSteps) {
        return juce::jlimit(0, numSteps - 1, (int)std::floor(normalizedValue * (float)numSteps));
    }
};

} // namespace ABD
