#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoHPFSection : public juce::Component
{
public:
    JunoHPFSection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh)
    {
        addAndMakeVisible(freqSlider);
        JunoUI::styleSwitchSlider(freqSlider);
        JunoUI::setupLabel(freqLabel, "FREQ", *this);

        freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hpfFreq", freqSlider);
        
        JunoUI::setupMidiLearn(freqSlider, mlh, "hpfFreq", midiLearnListeners);
        
        // HPF Slider has 4 distinct steps usually, but we implement as linear for this plugin logic or continuous?
        // Original Juno HPF is a 4-position switch. Let's stick to what we have (Slider) but maybe snap it in logic.
        // The implementation plan says "Position 0 (Bass Boost)".
    }

    void paint(juce::Graphics& g) override
    {
        JunoUI::drawJunoSectionPanel(g, getLocalBounds(), "HPF");
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(5, 30);
        int sliderWidth = 30; // Standard
        int sliderY = area.getY() + 25; // Standard Y
        int sliderH = area.getHeight() - 30; // Standard H

        freqLabel.setBounds(area.getX(), area.getY(), area.getWidth(), 20);
        freqLabel.setJustificationType(juce::Justification::centred);
        freqSlider.setBounds(area.getX() + (area.getWidth() - sliderWidth)/2, sliderY, sliderWidth, sliderH);
    }

private:
    juce::Slider freqSlider;
    juce::Label freqLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
