#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoDCOSection : public juce::Component, public juce::AudioProcessorValueTreeState::Listener, public juce::AsyncUpdater
{
public:
    static constexpr int kHeaderHeight = 28;

    JunoDCOSection(juce::AudioProcessorValueTreeState& state, MidiLearnHandler& mlh) : apvts(state)
    {
        auto cfgBtn = [&](juce::TextButton& b, const char* txt) {
            b.setButtonText(txt);
            b.setClickingTogglesState(true);
            b.setRadioGroupId(101);
            b.setColour(juce::TextButton::buttonColourId, JunoUI::kPanelDarkGrey);
            b.setColour(juce::TextButton::buttonOnColourId, JunoUI::kPanelDarkGrey.brighter(0.2f));
            b.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.6f));
            b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            addAndMakeVisible(b);
        };
        cfgBtn(range16, "16'"); cfgBtn(range8, "8'"); cfgBtn(range4, "4'");
        
        auto cfgWave = [&](juce::ToggleButton& b) {
             JunoUI::styleToggleButton(b);
             addAndMakeVisible(b);
        };
        cfgWave(pulseButton); cfgWave(sawButton);

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
        sawButton.getProperties().set("ledColor", "0xff3399ff"); 

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
        
        float colW = (float)getWidth() / 4.0f;
        auto drawHdr = [&](const juce::String& t, int col, float subX = 0, float subW = 0) {
            float x = col * colW + subX;
            float w = (subW > 0) ? subW : colW;
            g.drawText(t, (int)x, 0, (int)w, 28, juce::Justification::centred);
        };
        
        drawHdr("RANGE", 0);
        drawHdr("LFO", 1, 0, colW/2);
        drawHdr("PWM", 1, colW/2, colW/2);
        drawHdr("MODE", 2, 0, colW/2); 
        drawHdr("WAVE", 2, colW/2, colW/2); 
        drawHdr("SUB", 3, 0, colW/2);
        drawHdr("NOISE", 3, colW/2, colW/2);

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
        
        drawLED(range16);
        drawLED(range8);
        drawLED(range4);
        drawLED(pulseButton);
        drawLED(sawButton);
        
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
        auto r = getLocalBounds().reduced(0, 28); 
        float colW = (float)getWidth() / 4.0f; 
        
        int sliderW = 30;
        int sliderH = r.getHeight() - 40; 
        int yControls = r.getY() + 20;
        
        int btnW = 40; 
        int btnH = 25;
        int gap = 8;
        float col1Center = colW / 2.0f;
        int totalW = (btnW * 3) + (gap * 2);
        int x1 = (int)(col1Center - totalW/2);
        
        range16.setBounds(x1, yControls + 30, btnW, btnH);
        range8.setBounds(x1 + btnW + gap, yControls + 30, btnW, btnH);
        range4.setBounds(x1 + (btnW + gap)*2, yControls + 30, btnW, btnH);

        float col2Start = colW;
        float subColW = colW / 2.0f;
        auto placeSlider = [&](juce::Slider& s, float xOffset) {
            int cx = (int)(xOffset + (subColW - (float)sliderW)/2.0f);
            s.setBounds(cx, yControls, sliderW, sliderH);
        };
        placeSlider(lfoSlider, col2Start);
        placeSlider(pwmSlider, col2Start + subColW);

        float col3Start = colW * 2;
        pwmModeSwitch.setBounds((int)(col3Start + (subColW - 20)/2), yControls + 20, 20, 40);

        int waveBtnSize = 30;
        int wGap = 10;
        int wTotal = (waveBtnSize * 2) + wGap;
        int wxStart = (int)(col3Start + subColW + (subColW - (float)wTotal)/2.0f);
        
        pulseButton.setBounds(wxStart, yControls + 30, waveBtnSize, waveBtnSize);
        sawButton.setBounds(wxStart + waveBtnSize + wGap, yControls + 30, waveBtnSize, waveBtnSize);

        float col4Start = colW * 3;
        placeSlider(subSlider, col4Start);
        placeSlider(noiseSlider, col4Start + subColW);
    }
private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton range16, range8, range4;
    juce::ToggleButton pulseButton, sawButton;
    juce::Slider lfoSlider, pwmSlider, subSlider, noiseSlider, pwmModeSwitch;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoAttachment, pwmAttachment, subAttachment, noiseAttachment, pwmModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pulseAttachment, sawAttachment;
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
