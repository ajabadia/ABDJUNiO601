#pragma once

#include <JuceHeader.h>
#include "../BaseClass/PresetManagerBase.h"
#include "JunoFormatConverter.h"
#include "JunoTapeDecoder.h"

namespace ABD {

/**
 * @class JunoTapeImporter
 * @brief Responsible for loading Tape dumps (.wav) and converting them into Libraries.
 */
class JunoTapeImporter {
public:
    struct ImportResult {
        std::vector<ABD::Library> libraries;
        juce::Result result = juce::Result::ok();
    };

    /**
     * @brief Decodes a WAV file and creates libraries of 64 patches each.
     */
    static ImportResult loadFromFile(const juce::File& file);
};

} // namespace ABD
