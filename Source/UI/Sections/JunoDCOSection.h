#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoDCOSection : public juce::Component, public juce::AudioProcessorValueTreeState::Listener, public juce::AsyncUpdater
{
public:
    static constexpr int kHeaderHeight = 28;

    JunoDCOSection(juce::AudioProcessorValueTreeState& state, MidiLearnHandler& mlh) : apvts(state)
    {
        auto cfgBtn = [&](juce::TextButton& b, const char* txt, int radioId = 0) {
            b.setButtonText(txt);
            b.setClickingTogglesState(true);
            if (radioId > 0) b.setRadioGroupId(radioId);
            b.setColour(juce::TextButton::buttonColourId, JunoUI::kPanelDarkGrey);
            b.setColour(juce::TextButton::buttonOnColourId, JunoUI::kPanelDarkGrey.brighter(0.2f));
            b.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.6f));
            b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            addAndMakeVisible(b);
        };

        // RANGE Buttons
        cfgBtn(range16, "16'", 101); cfgBtn(range8, "8'", 101); cfgBtn(range4, "4'", 101);
        
        // WAVE Buttons (Changed to TextButton to match RANGE style)
        cfgBtn(pulseButton, "PULSE"); 
        cfgBtn(sawButton, "SAW");

        range16.onClick = [this] { if (range16.getToggleState()) setRange(0); else updateRangeUI(); };
        range8.onClick  = [this] { if (range8.getToggleState())  setRange(1); else updateRangeUI(); };
        range4.onClick  = [this] { if (range4.getToggleState())  setRange(2); else updateRangeUI(); };
        
        auto cfgSld = [&](juce::Slider& s, const char* id) {
            JunoUI::setupVerticalSlider(s); addAndMakeVisible(s); 
            JunoUI::setupMidiLearn(s, mlh, id, midiLearnListeners);
        };
        cfgSld(lfoSlider, "lfoToDCO");
        cfgSld(pwmSlider, "pwm");
        cfgSld(subSlider, "subOsc");
        cfgSld(noiseSlider, "noise");

        lfoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfoToDCO", lfoSlider);
        pwmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pwm", pwmSlider);
        subAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "subOsc", subSlider);
        noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "noise", noiseSlider);

        addAndMakeVisible(pwmModeSwitch);
        addAndMakeVisible(pwmModeSwitch);
        // [Senior Audit] PWM Switch is Rotary 2-pos
        pwmModeSwitch.setSliderStyle(juce::Slider::LinearBarVertical); // Wait, User asked for ROTARY.
        // Actually, "LinearBarVertical" acts as a toggle, but visually weird. 
        // Let's use RotaryHorizontalVerticalDrag.
        pwmModeSwitch.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        pwmModeSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pwmModeSwitch.setSliderSnapsToMousePosition(false); // Rotary logic
        pwmModeSwitch.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, 
                                          juce::MathConstants<float>::pi * 1.8f, true); // Small arc (2 pos)
        
        pwmModeSwitch.setRange(0.0, 1.0, 1.0);
        pwmModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pwmMode", pwmModeSwitch);

        pulseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "pulseOn", pulseButton);
        sawAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "sawOn", sawButton);

        apvts.addParameterListener("dcoRange", this);
        apvts.addParameterListener("pulseOn", this); 
        apvts.addParameterListener("sawOn", this);   
        updateRangeUI(); 
    }

    ~JunoDCOSection() override { 
        apvts.removeParameterListener("dcoRange", this);
        apvts.removeParameterListener("pulseOn", this);
        apvts.removeParameterListener("sawOn", this);
    }
    void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { 
        updateRangeUI(); 
        repaint(); 
    }

    void setRange(int index) {
        if (auto* p = apvts.getParameter("dcoRange")) p->setValueNotifyingHost(p->convertTo0to1((float)index));
    }
    void updateRangeUI() {
        int val = (int)*apvts.getRawParameterValue("dcoRange");
        range16.setToggleState(val == 0, juce::dontSendNotification);
        range8.setToggleState(val == 1, juce::dontSendNotification);
        range4.setToggleState(val == 2, juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(JunoUI::kPanelGrey);
        g.fillRect(getLocalBounds());
        
        auto headerRect = getLocalBounds().removeFromTop(28);
        g.setColour(JunoUI::kStripRed);
        g.fillRect(headerRect);
        g.setColour(JunoUI::kTextWhite);
        g.setFont(juce::FontOptions("Arial", 14.0f, juce::Font::bold));
        
        // Header Text
        g.drawText("DCO", 0, 0, getWidth(), 28, juce::Justification::centred);

        // Sub-headers inside section
        g.setFont(10.0f);
        
        // Manual placement matching resized()
        int x1 = 15;
        g.drawText("RANGE", x1 - 10, 28, 60, 20, juce::Justification::centred);
        
        int xCursor = x1 + 40 + 20;
        int spacing = 65;
        g.drawText("LFO", xCursor - 15, 28, 60, 20, juce::Justification::centred);
        xCursor += spacing;
        g.drawText("PWM", xCursor - 15, 28, 60, 20, juce::Justification::centred);
        xCursor += spacing;
        
        g.drawText("MODE", xCursor - 10, 28, 60, 20, juce::Justification::centred);
        xCursor += 55;
        g.drawText("WAVE", xCursor - 10, 28, 70, 20, juce::Justification::centred);
        xCursor += 55 + 25;
        
        g.drawText("SUB", xCursor - 15, 28, 60, 20, juce::Justification::centred);
        xCursor += spacing;
        g.drawText("NOISE", xCursor - 15, 28, 60, 20, juce::Justification::centred);

        g.setColour(juce::Colours::black);
        g.drawVerticalLine(getWidth() - 1, 0, (float)getHeight());
        
        auto drawLED = [&](juce::Component& comp) { 
             if (comp.isVisible()) {
                 auto b = comp.getBounds();
                 float ledSize = 6.0f;
                 float x = b.getCentreX() - ledSize/2;
                 float y = (float)b.getY() - 10.0f;
                 
                 g.setColour(juce::Colours::black);
                 g.fillEllipse(x, y, ledSize, ledSize);
                 
                 bool on = false;
                 if (auto* btn = dynamic_cast<juce::Button*>(&comp)) on = btn->getToggleState();
                 if (on) {
                     g.setColour(juce::Colour(0xffff3030)); 
                     g.fillEllipse(x+1, y+1, ledSize-2, ledSize-2);
                     g.setColour(juce::Colour(0xffff3030).withAlpha(0.4f));
                     g.fillEllipse(x-2, y-2, ledSize+4, ledSize+4);
                 } else {
                     g.setColour(juce::Colour(0xff502020)); 
                     g.fillEllipse(x+1, y+1, ledSize-2, ledSize-2);
                 }
             }
        };
        
        drawLED(range16); drawLED(range8); drawLED(range4);
        drawLED(pulseButton); drawLED(sawButton);
        
        if (pwmModeSwitch.isVisible()) {
             auto b = pwmModeSwitch.getBounds();
             g.setColour(JunoUI::kTextWhite.withAlpha(0.8f));
             g.setFont(10.0f);
             g.drawText("LFO", b.getX(), b.getY() - 12, b.getWidth(), 12, juce::Justification::centred);
             g.drawText("MAN", b.getX(), b.getBottom() + 2, b.getWidth(), 12, juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(0, 48); // Header(28) + SubHeaders(20)
        
        int sliderW = 30;
        int sliderH = r.getHeight() - 30;  // Standard
        int yControls = r.getY() + 25;     // Standard
        
        // COL 1: RANGE (Stacked Vertically)
        int btnW = 40; 
        int btnH = 25;
        int gap = 5;
        
        int x1 = 15; // Left margin
        int btnY = yControls; // Start at same Y as sliders top
        
        // Stack them
        range16.setBounds(x1, btnY, btnW, btnH);
        range8.setBounds(x1, btnY + btnH + gap, btnW, btnH);
        range4.setBounds(x1, btnY + (btnH + gap)*2, btnW, btnH);

        // Shift everything else left
        int xCursor = x1 + btnW + 20;
        
        // LFO & PWM
        int spacing = 65;
        lfoSlider.setBounds(xCursor, yControls, sliderW, sliderH);
        xCursor += spacing;
        
        pwmSlider.setBounds(xCursor, yControls, sliderW, sliderH);
        xCursor += spacing;
        
        // PWM MODE & SWITCHES
        // Center switches vertically relative to sliders
        int switchY = yControls + (sliderH - 40) / 2;
        pwmModeSwitch.setBounds(xCursor, switchY - 15, 40, 40); // Shifted up slightly
        xCursor += 55;
        
        int waveBtnW = 55;
        int waveY = switchY - 15;
        pulseButton.setBounds(xCursor, waveY, waveBtnW, 25);
        sawButton.setBounds(xCursor, waveY + 35, waveBtnW, 25);
        xCursor += waveBtnW + 25;
        
        // SUB & NOISE
        subSlider.setBounds(xCursor, yControls, sliderW, sliderH);
        xCursor += spacing;
        noiseSlider.setBounds(xCursor, yControls, sliderW, sliderH);
    }
private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton range16, range8, range4;
    juce::TextButton pulseButton, sawButton;
    juce::Slider lfoSlider, pwmSlider, subSlider, noiseSlider, pwmModeSwitch;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoAttachment, pwmAttachment, subAttachment, noiseAttachment, pwmModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pulseAttachment, sawAttachment;
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
