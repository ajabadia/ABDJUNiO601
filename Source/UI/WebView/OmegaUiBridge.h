#pragma once

#include <JuceHeader.h>
#include "../../Core/Instrument.h"

namespace Omega {
namespace UI {

/**
 * OmegaUiBridge - Standardized RPC Layer for OMEGA.
 * Handles communication between the C++ Engine and WebUI.
 */
class OmegaUiBridge {
public:
    OmegaUiBridge(Core::Instrument& inst, juce::WebBrowserComponent& webComp);
    ~OmegaUiBridge() = default;

    // --- RPC Registration ---
    void registerNativeFunctions();

    // --- Outbound Notifications (C++ -> JS) ---
    void triggerEvent(const juce::String& eventName, const juce::var& parameters);
    void updateParameter(const juce::String& paramId, float value);
    void updateLCD(const juce::String& text);
    void updateBankPatch(int bank, int patch);
    void updateSysEx(const juce::MidiMessage& msg);

private:
    Core::Instrument& instrument;
    juce::WebBrowserComponent& webComponent;

    // --- Internal Helpers ---
    void handleParamChange(const juce::String& paramId, float value);
    void handleAction(const juce::String& action, const juce::Array<juce::var>& args);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OmegaUiBridge)
};

} // namespace UI
} // namespace Omega


