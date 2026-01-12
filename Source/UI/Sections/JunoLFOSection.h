#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoLFOSection : public juce::Component
{
public:
    JunoLFOSection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh)
    {
        JunoUI::setupVerticalSlider(rateSlider); addAndMakeVisible(rateSlider); JunoUI::setupLabel(rateLabel, "RATE", *this);
        JunoUI::setupVerticalSlider(delaySlider); addAndMakeVisible(delaySlider); JunoUI::setupLabel(delayLabel, "DELAY", *this);

        rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfoRate", rateSlider);
        delayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfoDelay", delaySlider);

        JunoUI::setupMidiLearn(rateSlider, mlh, "lfoRate", midiLearnListeners);
        JunoUI::setupMidiLearn(delaySlider, mlh, "lfoDelay", midiLearnListeners);

        rateSlider.getProperties().set("numberSide", "Left");
        delaySlider.getProperties().set("numberSide", "Right");
    }

    void paint(juce::Graphics& g) override
    {
        JunoUI::drawJunoSectionPanel(g, getLocalBounds(), "LFO");
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(5, 30); // Espacio para cabecera
        int sliderWidth = 30; // Standard 30px
        
        int labelHeight = 20;
        int sliderY = area.getY() + 25; // Standard Y
        int sliderHeight = area.getHeight() - 30; // Standard Height

        int gap = (area.getWidth() - (sliderWidth * 2)) / 3;

        rateLabel.setBounds(area.getX() + gap, area.getY(), sliderWidth + 10, labelHeight);
        rateSlider.setBounds(area.getX() + gap + 5, sliderY, sliderWidth, sliderHeight);

        delayLabel.setBounds(rateSlider.getRight() + gap, area.getY(), sliderWidth + 10, labelHeight);
        delaySlider.setBounds(rateSlider.getRight() + gap + 5, sliderY, sliderWidth, sliderHeight);
    }

private:
    juce::Slider rateSlider, delaySlider;
    juce::Label rateLabel, delayLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
