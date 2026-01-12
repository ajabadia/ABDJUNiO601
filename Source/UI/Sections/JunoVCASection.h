#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoVCASection : public juce::Component
{
public:
    JunoVCASection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh)
    {
        JunoUI::setupVerticalSlider(levelSlider); addAndMakeVisible(levelSlider); 
        
        // "VCA... sliders tienen un tamaño diferente al resto y no están alineados" -> Use standard helper setup or manual to match sizes.
        
        JunoUI::setupLabel(levelLabel, "LEVEL", *this);
        
        // Mode Switch (ENV / GATE)
        addAndMakeVisible(modeSwitch);
        JunoUI::styleSwitchSlider(modeSwitch);
        modeSwitch.setRange(0.0, 1.0, 1.0); // 0=ENV, 1=GATE
        modeSwitch.getProperties().set("isSwitch", true);

        // "pon los literales del switch del vca encima y debajo del switch"
        JunoUI::setupLabel(modeLabel, "MODE", *this);
        JunoUI::setupLabel(lblEnv, "ENV", *this);
        JunoUI::setupLabel(lblGate, "GATE", *this);

        levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "vcaLevel", levelSlider);
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "vcaMode", modeSwitch);

        JunoUI::setupMidiLearn(levelSlider, mlh, "vcaLevel", midiLearnListeners);
        JunoUI::setupMidiLearn(modeSwitch, mlh, "vcaMode", midiLearnListeners);
    }

    void paint(juce::Graphics& g) override
    {
        JunoUI::drawJunoSectionPanel(g, getLocalBounds(), "VCA");
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(5, 30); // 24 -> 30 to match others (approx, others use reduced(5,30))
        // Wait, other sections used reduced(5,30) because kHeaderHeight is 28 approx.
        // Let's stick to consistent internal area: Y+25, H-30.
        
        int halfW = area.getWidth() / 2;
        int startX = area.getX();
        
        int sliderW = 30;
        int sliderH = area.getHeight() - 30; // Standard
        int yControls = area.getY() + 25;    // Standard

        // Level
        levelLabel.setBounds(startX, area.getY(), halfW, 20);
        levelLabel.setJustificationType(juce::Justification::centred);
        levelSlider.setBounds(startX + (halfW - sliderW)/2, yControls, sliderW, sliderH);

        // Mode Switch
        int switchW = 30;
        int switchH = 50; 
        
        modeLabel.setBounds(startX + halfW, area.getY(), halfW, 20);
        modeLabel.setJustificationType(juce::Justification::centred);
        
        int switchCenterX = startX + halfW + (halfW - switchW)/2;
        int centerY = yControls + sliderH/2;
        
        lblGate.setBounds(switchCenterX - 10, centerY - switchH/2 - 20, 50, 20);
        modeSwitch.setBounds(switchCenterX, centerY - switchH/2, switchW, switchH);
        lblEnv.setBounds(switchCenterX - 10, centerY + switchH/2, 50, 20);
    }

private:
    juce::Slider levelSlider;
    juce::Slider modeSwitch;
    juce::Label levelLabel, modeLabel, lblEnv, lblGate;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modeAttachment;
    
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
