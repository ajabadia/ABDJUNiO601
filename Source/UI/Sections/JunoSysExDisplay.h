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
    void setDumpData(const std::vector<uint8_t>& dump);

private:
    std::vector<uint8_t> lastDump;
    std::vector<uint8_t> currentDump;
    juce::Colour defaultColour = juce::Colours::grey;
    juce::Colour highlightColour = juce::Colours::red;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JunoSysExDisplay)
};
