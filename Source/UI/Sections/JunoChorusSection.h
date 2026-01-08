#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoChorusSection : public juce::Component, public juce::AudioProcessorValueTreeState::Listener, public juce::AsyncUpdater
{
public:
    JunoChorusSection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh) : apvts(apvts)
    {
        // ... (Buttons setup) ...
        addAndMakeVisible(b1); JunoUI::styleToggleButton(b1); att1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "chorus1", b1);
        addAndMakeVisible(b2); JunoUI::styleToggleButton(b2); att2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "chorus2", b2);
        JunoUI::setupLabel(l1, "I", *this); JunoUI::setupLabel(l2, "II", *this);

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
        // Usa el helper con cabecera AZUL para Chorus
        JunoUI::drawJunoSection(g, getLocalBounds(), "CHORUS", true);

        // LEDs (Manual check)
        auto drawLED = [&](juce::ToggleButton& btn) {
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
        auto area = getLocalBounds().reduced(5, 30); // Margen para cabecera y texto
        int buttonWidth = 40;
        int buttonHeight = 50; // Botón con LED
        int gap = 10;
        
        int totalButtonsWidth = (buttonWidth * 2) + gap;
        int startX = area.getX() + (area.getWidth() - totalButtonsWidth) / 2; // Centrar los botones
        
        int labelY = area.getY() + 30; // Debajo del título "CHORUS"
        int buttonY = labelY + 20;

        l1.setBounds(startX, labelY, buttonWidth, 20);
        b1.setBounds(startX, buttonY, buttonWidth, buttonHeight);
        
        l2.setBounds(startX + buttonWidth + gap, labelY, buttonWidth, 20);
        b2.setBounds(startX + buttonWidth + gap, buttonY, buttonWidth, buttonHeight);
    }

private:
    juce::ToggleButton b1, b2;
    juce::Label l1, l2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att1, att2;
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
    juce::AudioProcessorValueTreeState& apvts;
};
