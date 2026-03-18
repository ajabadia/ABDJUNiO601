#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"
#include "../../UI/JunoBender.h" // Include full definition

class MidiLearnHandler;

class JunoSidePanel : public juce::Component
{
public:
    JunoSidePanel(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh);
    ~JunoSidePanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Controls
    JunoBender bender; // Use custom component
    // Portamento and Assign Moved to JunoControlSection in Phase 9

    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JunoSidePanel)
};
