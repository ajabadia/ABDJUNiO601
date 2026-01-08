#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoENVSection : public juce::Component
{
public:
    JunoENVSection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh)
    {
        JunoUI::setupVerticalSlider(aSlider); addAndMakeVisible(aSlider); JunoUI::setupLabel(aLabel, "A", *this);
        JunoUI::setupVerticalSlider(dSlider); addAndMakeVisible(dSlider); JunoUI::setupLabel(dLabel, "D", *this);
        JunoUI::setupVerticalSlider(sSlider); addAndMakeVisible(sSlider); JunoUI::setupLabel(sLabel, "S", *this);
        JunoUI::setupVerticalSlider(rSlider); addAndMakeVisible(rSlider); JunoUI::setupLabel(rLabel, "R", *this);

        // --- CORRECCIÓN CRÍTICA DE IDs ---
        // Antes: "envAttack" (Incorrecto) -> Ahora: "attack" (Correcto)
        // Antes: "envDecay" (Incorrecto) -> Ahora: "decay" (Correcto)
        
        aAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", aSlider);
        dAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "decay", dSlider);
        sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "sustain", sSlider);
        rAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", rSlider);

        JunoUI::setupMidiLearn(aSlider, mlh, "attack", midiLearnListeners);
        JunoUI::setupMidiLearn(dSlider, mlh, "decay", midiLearnListeners);
        JunoUI::setupMidiLearn(sSlider, mlh, "sustain", midiLearnListeners);
        JunoUI::setupMidiLearn(rSlider, mlh, "release", midiLearnListeners);
    }

    void paint(juce::Graphics& g) override {
        JunoUI::drawJunoSection(g, getLocalBounds(), "ENV");
    }

    void resized() override {
        auto r = getLocalBounds().reduced(0, 24); // Saltar cabecera
        int w = r.getWidth() / 4;
        int sliderW = 30;
        int sliderH = r.getHeight() - 25;
        int y = r.getY() + 20;

        auto layout = [&](juce::Slider& s, juce::Label& l, int idx) {
            l.setBounds(idx * w, r.getY(), w, 20);
            s.setBounds(idx * w + (w - sliderW)/2, y, sliderW, sliderH);
        };

        layout(aSlider, aLabel, 0);
        layout(dSlider, dLabel, 1);
        layout(sSlider, sLabel, 2);
        layout(rSlider, rLabel, 3);
    }
private:
    juce::Slider aSlider, dSlider, sSlider, rSlider;
    juce::Label aLabel, dLabel, sLabel, rLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aAtt, dAtt, sAtt, rAtt;
    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
