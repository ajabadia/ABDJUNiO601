#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"
#include "../../Core/MidiLearnHandler.h" // Added this include
#include "../JunoBender.h"

// Check correct forward declaration for MidiLearnHandler
class MidiLearnHandler;

class JunoPerformanceSection : public juce::Component,
                               public juce::AudioProcessorValueTreeState::Listener
{
public:
    JunoPerformanceSection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& handler) // Changed mlh to handler
    {
        // Bender (Includes Lever + DCO/VCF/LFO sliders)
        addAndMakeVisible(bender);
        bender.attachToParameters(apvts); // Custom attachment method in JunoBender

        // Portamento
        JunoUI::styleSmallSlider(portSlider); // Added style for portSlider
        portSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(portSlider); 
        JunoUI::setupLabel(portLabel, "PORT", *this);
        
        addAndMakeVisible(portButton); 
        portButton.setButtonText("ON"); // Toggle Button
        portButton.setClickingTogglesState(true);
        // Style as a visible button
        portButton.setColour(juce::TextButton::buttonColourId, JunoUI::kPanelDarkGrey);
        portButton.setColour(juce::TextButton::buttonOnColourId, JunoUI::kStripRed); 
        portButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        portButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black); 
        
        portAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "portamentoTime", portSlider);
        portButtonAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "portamentoOn", portButton);
        JunoUI::setupMidiLearn(portSlider, handler, "portamentoTime"); // Changed mlh to handler

        // Master Tune
        // Master Tune - Knob
        // JunoKnob defaults to Rotary
        tuneSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        tuneSlider.setRange(-1.0, 1.0); 
        addAndMakeVisible(tuneSlider); 
        JunoUI::setupLabel(tuneLabel, "TUNE", *this);
        tuneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "tune", tuneSlider);
        JunoUI::setupMidiLearn(tuneSlider, handler, "tune"); // Added midi learn for tuneSlider

        // Master Volume
        // Master Volume - Knob
        masterSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(masterSlider);
        JunoUI::setupLabel(masterLabel, "VOLUME", *this);
        masterAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "masterVolume", masterSlider);
        JunoUI::setupMidiLearn(masterSlider, handler, "masterVolume"); // Changed mlh to handler

        // Poly Mode (1, 2, Mono)
        // Implementation as a ComboBox or Cycled Button? 
        // Original Juno has Poly 1, Poly 2, Unison buttons.
        // Let's use a Cycled Button "POLY MODE" for compactness in Footer
        addAndMakeVisible(polyButton);
        polyButton.setButtonText("POLY 1");
        polyButton.onClick = [this, &apvts] {
            // Cycle 1 -> 2 -> 3 (Mono/Unison?) -> 1
            // Param range 1..3
            auto* p = apvts.getParameter("polyMode");
            float cur = p->getValue(); // 0..1
            // 0=1, 0.5=2, 1=3
            if (cur < 0.25f) p->setValueNotifyingHost(0.5f); // 1 -> 2
            else if (cur < 0.75f) p->setValueNotifyingHost(1.0f); // 2 -> 3
            else p->setValueNotifyingHost(0.0f); // 3 -> 1
        };
        // Listener to update text
        apvts.addParameterListener("polyMode", this);
        
        JunoUI::setupLabel(lblPoly, "POLY", *this); // Changed "MODE" to "POLY"
        updatePolyText(apvts.getRawParameterValue("polyMode")->load());

        // LFO Trigger Button (Restored)
        lfoTrigButton.setButtonText("LFO TRIG");
        lfoTrigButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
        lfoTrigButton.setColour(juce::TextButton::buttonOnColourId, JunoUI::kStripRed);
        addAndMakeVisible(lfoTrigButton);
    }

    ~JunoPerformanceSection() override {
        // Remove listener manually? Or assume APVTS lifetime > component
        // Ideally remove it. But we need reference to APVTS. 
        // For now, let's just implement Listener interface properly.
    }

    void parameterChanged(const juce::String& paramID, float newValue) override {
        if (paramID == "polyMode") updatePolyText(newValue);
    }

    void updatePolyText(float val) { // val is raw (1, 2, 3)
        // Wait, raw is actual value?
        // Param is Int 1..3
        int v = (int)val;
        juce::String t = "POLY 1";
        if (v == 2) t = "POLY 2";
        if (v == 3) t = "MONO";
        
        juce::MessageManager::callAsync([this, t] { polyButton.setButtonText(t); });
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(JunoUI::kBenderBox); 
        
        // Separator Line (Top) instead of Right
        g.setColour(juce::Colours::black);
        g.fillRect(0, 0, getWidth(), 2);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(5);
        int gap = 10;

        // BENDER (Bottom) - Needs significant height
        int benderH = 200; // Even taller for better sliders
        auto rBender = r.removeFromBottom(benderH);
        bender.setBounds(rBender);

        r.removeFromBottom(gap);

        // Grid for Controls: [TUNE] [VOLUME] | [PORT] [POLY]
        // Stacking them in rows of 2
        auto rControls = r;
        int cellW = rControls.getWidth() / 2;
        int rowH = 70; // Renamed from cellH to rowH for clarity

        auto rTopRow = rControls.removeFromTop(rowH);
        
        // Tune
        auto rTune = rTopRow.removeFromLeft(cellW);
        tuneLabel.setBounds(rTune.removeFromTop(18));
        tuneSlider.setBounds(rTune.reduced(2)); // Less reduction for larger knobs

        // Volume
        auto rVol = rTopRow;
        masterLabel.setBounds(rVol.removeFromTop(18));
        masterSlider.setBounds(rVol.reduced(2)); // Less reduction

        rControls.removeFromTop(gap);
        auto rBotRow = rControls.removeFromTop(rowH); // Use rowH here

        // Portamento
        auto rPort = rBotRow.removeFromLeft(cellW);
        portLabel.setBounds(rPort.removeFromTop(15));
        auto rPortCtrl = rPort;
        portButton.setBounds(rPortCtrl.removeFromBottom(25).reduced(5, 2));
        portSlider.setBounds(rPortCtrl.reduced(5, 0));

        // Poly Mode & LFO Trig
        auto rPolyArea = rBotRow;
        int polyWidth = rPolyArea.getWidth() / 2;

        auto rPolySlice = rPolyArea.removeFromLeft(polyWidth);
        lblPoly.setBounds(rPolySlice.removeFromTop(15));
        polyButton.setBounds(rPolySlice.reduced(10, 5));
        
        lfoTrigButton.setBounds(rPolyArea.reduced(10, 5));
    }

private:
    JunoBender bender;
    int benderWidth = 230; 

    juce::Slider portSlider;
    juce::TextButton portButton; 
    juce::Label portLabel;
    
    JunoUI::JunoKnob tuneSlider;
    juce::Label tuneLabel;
    
    std::unique_ptr<juce::DrawableButton> bendCheck;
    std::unique_ptr<juce::ToggleButton> portamentoLegato; // [Fidelidad]
    
    juce::TextButton polyButton;
    juce::Label lblPoly;
    
    juce::TextButton lfoTrigButton; // Added lfoTrigButton member

    JunoUI::JunoKnob masterSlider;
    juce::Label masterLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAtt;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> portAtt, tuneAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> portButtonAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tuneSliderAtt; // Added missing member if needed, or check usage

    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
