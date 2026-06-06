#pragma once

#include <JuceHeader.h>
#include "../BaseClass/PresetManagerBase.h"
#include "../JunoProtocol.h"
#include "../FactoryPresets.h"

namespace ABD {

/**
 * @class JunoFormatConverter
 * @brief Utility class to convert various raw formats (SysEx bytes, Factory structs,
 *        Juno-60 DCB tape format) into ABD::Preset objects for the bridge.
 *
 * The 18-byte patch format is shared between Juno-106 and Juno-60, but the bit
 * assignments for the switch bytes (16-17) differ significantly:
 *
 * Juno-60 SW1 (byte 16):
 *   Bits 0-2: Range (16', 8', 4')
 *   Bit 3:    Saw Wave (1=ON)
 *   Bit 4:    Pulse Wave (1=ON)
 *   Bit 5:    Sub Osc On (1=ON)
 *   Bit 6:    PWM On (1=ON)
 *   Bit 7:    PWM Mode (0=LFO, 1=Manual)
 *
 * Juno-60 SW2 (byte 17):
 *   Bit 0:    VCA Mode (0=ENV, 1=GATE)
 *   Bit 1:    VCF Polarity (0=POS, 1=NEG)
 *   Bits 3-4: HPF (00=pos3, 01=pos2, 10=pos1, 11=pos0)
 *   Bits 2,5-7: Reserved (unused)
 *
 * Compare with Juno-106:
 *   SW1: Range(0-2), Pulse(3), Saw(4), ChorusEnable(5), ChorusMode(6)
 *   SW2: PWMMode(0), VCFPol(1), VCAMode(2), HPF(3-4)
 */
enum class JunoFormat {
    Juno106 = 0,  ///< Standard Juno-106 format (1200 baud tape, SysEx)
    Juno60  = 1   ///< Juno-60 DCB format (340 baud tape)
};

class JunoFormatConverter {
public:
    /**
     * @brief Creates a Preset from a legacy JunoPatch struct (Factory data).
     */
    static ABD::Preset createPresetFromJunoPatch(const struct JunoPatch& p, int globalIdx = -1);

    /**
     * @brief Creates a Preset from 18 raw bytes (Juno-106 SysEx/tape format).
     */
    static ABD::Preset createPresetFromJunoBytes(const juce::String& name, const uint8_t* bytes, const juce::ValueTree& emptyStateTemplate);

    /**
     * @brief Creates a Preset from 18 raw bytes, auto-selecting format based on baud rate.
     * @param name  Patch name
     * @param bytes 18-byte raw patch data
     * @param emptyStateTemplate  Unused template (for future use)
     * @param baudRate  Tape baud rate: 340 = Juno-60 DCB, 1200 = Juno-106
     */
    static ABD::Preset createPresetFromJunoBytes(const juce::String& name, const uint8_t* bytes,
                                                  const juce::ValueTree& emptyStateTemplate,
                                                  int baudRate);

    /**
     * @brief Internal mapping logic: Converts 18 Juno-106 hardware bytes to a juce::ValueTree state.
     */
    static juce::ValueTree bytesToValueTree(const uint8_t* src);

    /**
     * @brief Internal mapping logic: Converts 18 Juno-60 DCB bytes to a juce::ValueTree state.
     * 
     * The slider bytes (0-15) are in the same order as Juno-106, but the switch bytes
     * (16-17) have different bit assignments (no Chorus, dedicated Sub Osc switch, etc.).
     */
    static juce::ValueTree juno60BytesToValueTree(const uint8_t* src);
};

} // namespace ABD
