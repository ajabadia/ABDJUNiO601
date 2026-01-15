#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoVCASection : public juce::Component
{
public:
    JunoVCASection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh)
    {
        JunoUI::setupVerticalSlider(levelSlider); addAndMakeVisible(levelSlider); 
        
        JunoUI::setupLabel(levelLabel, "LEVEL", *this);
        
        // Mode Switch (ENV / GATE)
        addAndMakeVisible(modeSwitch);
        JunoUI::styleSwitchSlider(modeSwitch);
        modeSwitch.setRange(0.0, 1.0, 1.0); // 0=ENV, 1=GATE
        modeSwitch.getProperties().set("isSwitch", true);

        JunoUI::setupLabel(modeLabel, "MODE", *this);
        JunoUI::setupLabel(lblEnv, "ENV", *this);
        JunoUI::setupLabel(lblGate, "GATE", *this);

        levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "vcaLevel", levelSlider);
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "vcaMode", modeSwitch);

        JunoUI::setupMidiLearn(levelSlider, mlh, "vcaLevel");
        JunoUI::setupMidiLearn(modeSwitch, mlh, "vcaMode");
    }

    void paint(juce::Graphics& g) override
    {
        JunoUI::drawJunoSectionPanel(g, getLocalBounds(), "VCA");
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(5, 30); 
        
        int sliderWidth = 30;
        int sliderH = area.getHeight() - 30; 
        int yControls = area.getY() + 25;    

        // Split 50/50 for Level / Mode
        int halfW = area.getWidth() / 2;
        int startX = area.getX();
        
        // Level Col
        int cxLevel = startX + halfW / 2;
        levelLabel.setBounds(cxLevel - 20, area.getY(), 40, 20);
        levelSlider.setBounds(cxLevel - sliderWidth/2, yControls, sliderWidth, sliderH);

        // Mode Col
        int cxMode = startX + halfW + halfW/2;
        modeLabel.setBounds(cxMode - 20, area.getY(), 40, 20);
        
        int switchW = 30;
        int switchH = 50; 
        int centerY = yControls + sliderH/2;
        
        modeSwitch.setBounds(cxMode - switchW/2, centerY - switchH/2, switchW, switchH);
        
        lblGate.setBounds(cxMode - 40, centerY - switchH/2 - 20, 80, 20);
        lblEnv.setBounds(cxMode - 40, centerY + switchH/2, 80, 20);
    }

private:
    juce::Slider levelSlider;
    juce::Slider modeSwitch;
    juce::Label levelLabel, modeLabel, lblEnv, lblGate;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modeAttachment;
    
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
