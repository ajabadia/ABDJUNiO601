/*
  ==============================================================================

    JunoSysExDisplay.h
    Created: 9 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoSysExDisplay : public juce::Component
{
public:
    JunoSysExDisplay();
    ~JunoSysExDisplay() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    
    /** Update the display with new SysEx data. Highlights changed bytes. */
    void updateData(const juce::MidiMessage& sysexMsg);

private:
    std::vector<uint8_t> currentData;
    int lastChangedIndex = -1;
    juce::Colour defaultColour = juce::Colours::grey;
    juce::Colour highlightColour = juce::Colours::red;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JunoSysExDisplay)
};
