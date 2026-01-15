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
        
        // WAVE Buttons
        cfgBtn(pulseButton, "PULSE"); 
        cfgBtn(sawButton, "SAW");

        range16.onClick = [this] { if (range16.getToggleState()) setRange(0); else updateRangeUI(); };
        range8.onClick  = [this] { if (range8.getToggleState())  setRange(1); else updateRangeUI(); };
        range4.onClick  = [this] { if (range4.getToggleState())  setRange(2); else updateRangeUI(); };
        
        auto cfgSld = [&](juce::Slider& s, const char* id) {
            JunoUI::setupVerticalSlider(s); 
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // HIDDEN VALUES
            addAndMakeVisible(s); 
            JunoUI::setupMidiLearn(s, mlh, id);
        };
        cfgSld(lfoSlider, "lfoToDCO");
        cfgSld(pwmSlider, "pwm");
        cfgSld(subSlider, "subOsc");
        cfgSld(noiseSlider, "noise");

        lfoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfoToDCO", lfoSlider);
        pwmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pwm", pwmSlider);
        subAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "subOsc", subSlider);
        noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "noise", noiseSlider);

        addAndMakeVisible(range16); range16.setRadioGroupId(101);
        addAndMakeVisible(range8); range8.setRadioGroupId(101);
        addAndMakeVisible(range4); range4.setRadioGroupId(101);
        // PWM MODE Switch (Button)
        addAndMakeVisible(pwmModeBtn);
        pwmModeBtn.setClickingTogglesState(true);
        pwmModeBtn.setButtonText("MAN"); // Default text, updated by listener?
        // Actually, let's use colors to indicate mode LFO/MAN
        pwmModeBtn.setColour(juce::TextButton::buttonOnColourId, JunoUI::kStripBlue); // LFO
        pwmModeBtn.setColour(juce::TextButton::buttonColourId, JunoUI::kPanelDarkGrey); // MAN
        
        // Attachment - If 'pwmMode' is 0/1 param
        pwmModeButtonAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "pwmMode", pwmModeBtn);
        
        // Since button text might confuse if it just says "MAN" or "LFO" statically, 
        // we can update text in parameterChanged or onClick? 
        // "pwmMode" 0=LFO, 1=Manual (or vice versa in DSP? usually 0=LFO, 1=MAN)
        // Let's assume toggle ON = 1 = MAN.
        pwmModeBtn.onClick = [this] {
            pwmModeBtn.setButtonText(pwmModeBtn.getToggleState() ? "MAN" : "LFO");
        };
        // Initial text set
        // (Async update will handle it properly if we sync params)

        pulseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "pulseOn", pulseButton);
        sawAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "sawOn", sawButton);

        apvts.addParameterListener("dcoRange", this);
        apvts.addParameterListener("pulseOn", this); 
        apvts.addParameterListener("sawOn", this);   
        apvts.addParameterListener("pwmMode", this); // Listen for text update
        updateRangeUI(); 
    }

    ~JunoDCOSection() override { 
        apvts.removeParameterListener("dcoRange", this);
        apvts.removeParameterListener("pulseOn", this);
        apvts.removeParameterListener("sawOn", this);
        apvts.removeParameterListener("pwmMode", this);
    }
    void parameterChanged(const juce::String& p, float v) override { 
        if (p == "pwmMode") {
             juce::MessageManager::callAsync([this, v]{
                 pwmModeBtn.setButtonText(v > 0.5f ? "MAN" : "LFO");
             });
        }
        triggerAsyncUpdate(); 
    }
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
        g.drawText("DCO", 0, 0, getWidth(), 28, juce::Justification::centred);

        // Sub-headers (Control Labels)
        g.setFont(juce::FontOptions("Arial", 12.0f, juce::Font::bold)); 
        g.setColour(JunoUI::kTextWhite);

        auto drawLabel = [&](juce::Component& c, const juce::String& txt) {
            // Draw slightly above the component
            g.drawText(txt, c.getX() - 15, c.getY() - 18, c.getWidth() + 30, 15, juce::Justification::centred);
        };
        
        // Buttons: Range - Label fixed at top (aligned with Sliders)
        int labelY = lfoSlider.getY() - 18; // Use Slider Y as reference for Top alignment
        g.drawText("RANGE", range8.getX() - 30, labelY, 100, 15, juce::Justification::centred);
        
        drawLabel(lfoSlider, "LFO");
        drawLabel(pwmSlider, "PWM");
        
        // Mode Label - Fixed Top
        g.drawText("MODE", pwmModeBtn.getX() - 15, labelY, pwmModeBtn.getWidth() + 30, 15, juce::Justification::centred);
        
        // Wave Group - Fixed Top
        g.drawText("WAVE", pulseButton.getX(), labelY, pulseButton.getWidth(), 15, juce::Justification::centred);
        
        drawLabel(subSlider, "SUB");
        drawLabel(noiseSlider, "NOISE");

        g.setColour(juce::Colours::black);
        g.drawVerticalLine(getWidth() - 1, 0, (float)getHeight());
        
        // LEDs ... (keep existing)
        auto drawLED = [&](juce::Component& comp) { 
             if (comp.isVisible()) {
                 auto b = comp.getBounds();
                 float ledSize = 6.0f;
                 float x = b.getCentreX() - ledSize/2;
                 float y = (float)b.getY() - 6.0f; // Tighter
                 
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
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop(28); // Header
        r.removeFromTop(20); // Label Space
        
        int yControls = r.getY(); 
        int sliderH = r.getHeight() - 5; // Use remaining space
        int sliderW = 40; 
        
        // Center Buttons Vertically relative to Sliders
        // Slider H approx 100-150px. Buttons top aligned in previous code.
        // Let's center them.
        int btnW = 35;
        int btnH = 25;
        int gap = 3;
        int x1 = 15;
        
        int yOffset = (sliderH - btnH)/2; // Center single row (Range)
        
        range16.setBounds(x1, yControls + yOffset, btnW, btnH);
        range8.setBounds(x1 + btnW + gap, yControls + yOffset, btnW, btnH);
        range4.setBounds(x1 + (btnW + gap)*2, yControls + yOffset, btnW, btnH);
        
        int xCursor = x1 + (btnW + gap)*3 + 25;
        int spacing = 60;
        
        lfoSlider.setBounds(xCursor, yControls, sliderW, sliderH);
        xCursor += spacing;
        
        pwmSlider.setBounds(xCursor, yControls, sliderW, sliderH);
        xCursor += spacing;
        
        // MODE (Switch) - Center
        pwmModeBtn.setBounds(xCursor, yControls + yOffset, 40, 25);
        xCursor += 55; 
        
        // WAVE - 2 rows. Center group.
        int waveGroupH = 25 + 10 + 25; // btn + gap + btn
        int waveY = yControls + (sliderH - waveGroupH)/2;
        
        int waveBtnW = 50;
        pulseButton.setBounds(xCursor, waveY, waveBtnW, 25);
        sawButton.setBounds(xCursor, waveY + 35, waveBtnW, 25);
        xCursor += waveBtnW + 20;
        
        subSlider.setBounds(xCursor, yControls, sliderW, sliderH);
        xCursor += spacing;
        noiseSlider.setBounds(xCursor, yControls, sliderW, sliderH);
    }
private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton range16, range8, range4;
    juce::TextButton pulseButton, sawButton;
    juce::TextButton pwmModeBtn; // Replaced Switch
    
    juce::Slider lfoSlider, pwmSlider, subSlider, noiseSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoAttachment, pwmAttachment, subAttachment, noiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pulseAttachment, sawAttachment, pwmModeButtonAtt; // New att type
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
