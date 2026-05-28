#pragma once

#include <JuceHeader.h>
#include "../BaseClass/PresetManagerBase.h"
#include "JunoFormatConverter.h"

namespace ABD {

/**
 * @class JunoSysexImporter
 * @brief Handles Roland SysEx dumps (.syx) for Juno-106.
 */
class JunoSysexImporter {
public:
    struct ImportResult {
        std::vector<ABD::Library> libraries;
        juce::Result result = juce::Result::ok();
    };

    /**
     * @brief Parses a SysEx file and extracts patches.
     */
    static ImportResult loadFromFile(const juce::File& file);
};

} // namespace ABD
