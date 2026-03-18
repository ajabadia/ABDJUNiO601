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

        JunoUI::setupMidiLearn(rateSlider, mlh, "lfoRate");
        JunoUI::setupMidiLearn(delaySlider, mlh, "lfoDelay");

        rateSlider.getProperties().set("numberSide", "Left");
        delaySlider.getProperties().set("numberSide", "Right");
        
        addAndMakeVisible(lfoLed);
        rateSlider.onValueChange = [this] { lfoLed.setRate((float)rateSlider.getValue()); };
        lfoLed.setRate((float)rateSlider.getValue()); // Init
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

        // Center calculation
        int totalW = (sliderWidth * 2) + 40; // 40px gap
        int startX = area.getX() + (area.getWidth() - totalW) / 2;

        auto layout = [&](juce::Slider& s, juce::Label& l, int x) {
             l.setBounds(x - 10, area.getY(), sliderWidth + 20, labelHeight);
             l.setJustificationType(juce::Justification::centred);
             s.setBounds(x, sliderY, sliderWidth, sliderHeight);
        };

        layout(rateSlider, rateLabel, startX);
        layout(delaySlider, delayLabel, startX + sliderWidth + 40);
        
        // LED above Rate slider
        lfoLed.setBounds(startX + sliderWidth/2 - 4, rateLabel.getY() - 10, 8, 8);
    }
    
    // [Fidelidad] LFO Rate LED
    // We update this via a simple timer or just use the last known rate to animate in paint?
    // Paint animation requires constant repaint. Let's do it simple: Timer.
    struct LfoLed : public juce::Component, public juce::Timer {
        float phase = 0.0f;
        float rateHz = 1.0f;
        
        LfoLed() { startTimerHz(30); }
        void setRate(float r) { rateHz = 0.5f + r * 10.0f; } // Approx mapping
        void timerCallback() override {
            phase += rateHz * 0.033f; 
            if (phase > 1.0f) phase -= 1.0f;
            repaint();
        }
        void paint(juce::Graphics& g) override {
            float bright = 0.5f + 0.5f * std::sin(phase * 6.28f);
            g.setColour(juce::Colours::red.withAlpha(bright));
            g.fillEllipse(getLocalBounds().toFloat());
        }
    };
    LfoLed lfoLed;

private:
    juce::Slider rateSlider, delaySlider;
    juce::Label rateLabel, delayLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
