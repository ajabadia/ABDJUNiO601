#pragma once

#include <JuceHeader.h>
#include "../BaseClass/PresetManagerBase.h"
#include "../JunoProtocol.h"
#include "../FactoryPresets.h"

namespace ABD {

/**
 * @class JunoFormatConverter
 * @brief Utility class to convert various raw formats (SysEx bytes, Factory structs)
 *        into ABD::Preset objects for the bridge.
 */
class JunoFormatConverter {
public:
    /**
     * @brief Creates a Preset from a legacy JunoPatch struct (Factory data).
     */
    static ABD::Preset createPresetFromJunoPatch(const struct JunoPatch& p, int globalIdx = -1);

    /**
     * @brief Creates a Preset from 18 raw bytes (Juno 106 patch format).
     */
    static ABD::Preset createPresetFromJunoBytes(const juce::String& name, const uint8_t* bytes, const juce::ValueTree& emptyStateTemplate);

    /**
     * @brief Internal mapping logic: Converts 18 hardware bytes to a juce::ValueTree state.
     */
    static juce::ValueTree bytesToValueTree(const uint8_t* src);
};

} // namespace ABD
