#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoChorusSection : public juce::Component, public juce::AudioProcessorValueTreeState::Listener, public juce::AsyncUpdater
{
public:
    JunoChorusSection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh) : apvts(apvts)
    {
        auto cfgBtn = [&](juce::TextButton& b, const char* txt) {
            b.setButtonText(txt);
            b.setClickingTogglesState(true);
            b.setColour(juce::TextButton::buttonColourId, JunoUI::kPanelDarkGrey);
            b.setColour(juce::TextButton::buttonOnColourId, JunoUI::kPanelDarkGrey.brighter(0.2f));
            b.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.6f));
            b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            addAndMakeVisible(b);
        };

        cfgBtn(b1, "I"); cfgBtn(b2, "II");
        
        att1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "chorus1", b1);
        att2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "chorus2", b2);

        JunoUI::setupMidiLearn(b1, mlh, "chorus1", midiLearnListeners);
        JunoUI::setupMidiLearn(b2, mlh, "chorus2", midiLearnListeners);

        apvts.addParameterListener("chorus1", this);
        apvts.addParameterListener("chorus2", this);
    }

    ~JunoChorusSection() override {
        apvts.removeParameterListener("chorus1", this);
        apvts.removeParameterListener("chorus2", this);
    }
    
    void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { repaint(); }

    void paint(juce::Graphics& g) override
    {
        JunoUI::drawJunoSection(g, getLocalBounds(), "CHORUS", true);

        // Sub-header
        g.setFont(10.0f);
        g.setColour(JunoUI::kTextWhite);
        g.drawText("MODE", 0, 28, getWidth(), 20, juce::Justification::centred);

        auto drawLED = [&](juce::TextButton& btn) {
            if (!btn.isVisible()) return;
            auto b = btn.getBounds();
            float ledSize = 6.0f;
            float x = b.getCentreX() - ledSize/2;
            float y = (float)b.getY() - 10.0f;
            
            g.setColour(juce::Colours::black); g.fillEllipse(x, y, ledSize, ledSize);
            if (btn.getToggleState()) {
                g.setColour(juce::Colour(0xffff3030));
                g.fillEllipse(x+1, y+1, ledSize-2, ledSize-2);
                g.setColour(juce::Colour(0xffff3030).withAlpha(0.4f));
                g.fillEllipse(x-2, y-2, ledSize+4, ledSize+4);
            } else {
                 g.setColour(juce::Colour(0xff502020));
                 g.fillEllipse(x+1, y+1, ledSize-2, ledSize-2);
            }
        };
        drawLED(b1);
        drawLED(b2);
    }

    void resized() override
    {
        int btnW = 40;
        int btnH = 25;
        int gap = 10;
        int totalW = (btnW * 2) + gap;
        int startX = (getWidth() - totalW) / 2;
        int startY = 28 + 20 + 15; // Header + SubHeader + Margin

        b1.setBounds(startX, startY, btnW, btnH);
        b2.setBounds(startX + btnW + gap, startY, btnW, btnH);
    }

private:
    juce::TextButton b1, b2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att1, att2;
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
    juce::AudioProcessorValueTreeState& apvts;
};
