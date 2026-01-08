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
        JunoUI::styleSwitchSlider(pwmModeSwitch);
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
        if (auto* p = apvts.getParameter("dcoRange")) p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1((float)index));
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
        
        // [FIX] DCO Header
        g.drawText("DCO", 0, 0, getWidth(), 28, juce::Justification::centred);

        // Sub-headers inside section
        g.setFont(10.0f);
        float colW = (float)getWidth() / 4.0f;
        auto drawSubHdr = [&](const juce::String& t, float x, float w) {
            g.drawText(t, (int)x, 28, (int)w, 20, juce::Justification::centred);
        };
        
        drawSubHdr("RANGE", 0, colW);
        drawSubHdr("LFO", colW, colW/2);
        drawSubHdr("PWM", colW + colW/2, colW/2);
        drawSubHdr("MODE", colW * 2, colW/2); 
        drawSubHdr("WAVE", colW * 2 + colW/2, colW/2); 
        drawSubHdr("SUB", colW * 3, colW/2);
        drawSubHdr("NOISE", colW * 3 + colW/2, colW/2);

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
        auto r = getLocalBounds().reduced(0, 48); // Header (28) + SubHeaders (20)
        float colW = (float)getWidth() / 4.0f; 
        
        int sliderW = 30;
        int sliderH = r.getHeight() - 20; 
        int yControls = r.getY() + 10;
        
        // COL 1: RANGE
        int btnW = 40; 
        int btnH = 25;
        int gap = 8;
        int totalW = (btnW * 3) + (gap * 2);
        int x1 = (int)((colW - totalW)/2);
        range16.setBounds(x1, yControls + 30, btnW, btnH);
        range8.setBounds(x1 + btnW + gap, yControls + 30, btnW, btnH);
        range4.setBounds(x1 + (btnW + gap)*2, yControls + 30, btnW, btnH);

        // COL 2: LFO & PWM
        float subColW = colW / 2.0f;
        auto placeSlider = [&](juce::Slider& s, float xOffset) {
            int cx = (int)(xOffset + (subColW - (float)sliderW)/2.0f);
            s.setBounds(cx, yControls, sliderW, sliderH);
        };
        placeSlider(lfoSlider, colW);
        placeSlider(pwmSlider, colW + subColW);

        // COL 3: MODE & WAVE
        pwmModeSwitch.setBounds((int)(colW * 2 + (subColW - 20)/2), yControls + 20, 20, 40);
        int waveBtnW = 50;
        int wxStart = (int)(colW * 2 + subColW + (subColW - waveBtnW)/2.0f);
        pulseButton.setBounds(wxStart, yControls + 20, waveBtnW, 25);
        sawButton.setBounds(wxStart, yControls + 55, waveBtnW, 25);

        // COL 4: SUB & NOISE
        placeSlider(subSlider, colW * 3);
        placeSlider(noiseSlider, colW * 3 + subColW);
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
