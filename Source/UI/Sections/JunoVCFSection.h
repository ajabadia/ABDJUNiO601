#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"

class JunoVCFSection : public juce::Component
{
public:
    JunoVCFSection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh)
    {
        // ... (Sliders setup se mantiene igual) ...
        JunoUI::setupVerticalSlider(cutoffSlider); addAndMakeVisible(cutoffSlider); JunoUI::setupLabel(cutoffLabel, "FREQ", *this);
        JunoUI::setupVerticalSlider(resSlider); addAndMakeVisible(resSlider); JunoUI::setupLabel(resLabel, "RES", *this);
        JunoUI::setupVerticalSlider(envSlider); addAndMakeVisible(envSlider); JunoUI::setupLabel(envLabel, "ENV", *this);
        JunoUI::setupVerticalSlider(lfoSlider); addAndMakeVisible(lfoSlider); JunoUI::setupLabel(lfoLabel, "LFO", *this);
        JunoUI::setupVerticalSlider(keySlider); addAndMakeVisible(keySlider); JunoUI::setupLabel(keyLabel, "KYBD", *this);

        // Polarity Switch
        addAndMakeVisible(invSwitch);
        JunoUI::styleSwitchSlider(invSwitch);
        invSwitch.setRange(0.0, 1.0, 1.0);
        invSwitch.getProperties().set("isSwitch", true); 
        
        // Attachments
        cutoffAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "vcfFreq", cutoffSlider);
        resAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "resonance", resSlider);
        envAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "envAmount", envSlider); 
        lfoAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfoToVCF", lfoSlider); 
        keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "kybdTracking", keySlider);
        invAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "vcfPolarity", invSwitch); 

        // MIDI Learn
        auto setupMidi = [&](juce::Component& c, const juce::String& p) { JunoUI::setupMidiLearn(c, mlh, p, midiLearnListeners); };
        setupMidi(cutoffSlider, "vcfFreq");
        setupMidi(resSlider, "resonance");
        setupMidi(invSwitch, "vcfPolarity");
        setupMidi(envSlider, "envAmount");
        setupMidi(lfoSlider, "lfoToVCF");
        setupMidi(keySlider, "kybdTracking");
    }

    void paint(juce::Graphics& g) override
    {
        JunoUI::drawJunoSectionPanel(g, getLocalBounds(), "VCF");
        
        // Dibujar los iconos de Polarity (ENV +/-) encima del switch
        auto swBounds = invSwitch.getBounds();
        if (swBounds.isEmpty()) return; // Asegurarse de que el switch tenga bounds

        g.setColour(JunoUI::kTextGrey);
        juce::Path p;
        
        // Icono "Normal" (arriba del switch, pequeño triángulo hacia arriba)
        float x = (float)swBounds.getCentreX();
        float yTop = (float)swBounds.getY() - 10.0f; // Un poco por encima del switch
        p.startNewSubPath(x - 5, yTop + 5);
        p.lineTo(x, yTop);
        p.lineTo(x + 5, yTop + 5);
        p.closeSubPath();
        g.fillPath(p); // Triángulo sólido

        // Icono "Invertido" (abajo del switch, pequeño triángulo hacia abajo)
        p.clear();
        float yBottom = (float)swBounds.getBottom() + 10.0f; // Un poco por debajo del switch
        p.startNewSubPath(x - 5, yBottom - 5);
        p.lineTo(x, yBottom);
        p.lineTo(x + 5, yBottom - 5);
        p.closeSubPath();
        g.fillPath(p); // Triángulo sólido
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(5, 30); // Margen para cabecera y texto
        // 6 columnas: Freq, Res, Switch, Env, LFO, Kybd
        float totalColumns = 5.6f; // El switch es mas estrecho
        float standardColW = area.getWidth() / totalColumns;
        float switchColW = standardColW * 0.6f; // Switch 60% ancho
        
        int startX = area.getX();
        int sliderWidth = (int)(standardColW * 0.8f); 
        int sliderY = area.getY() + 30; // Debajo de las labels
        int sliderH = area.getHeight() - 30;
        int labelH = 20;
        
        auto formatCol = [&](juce::Component& comp, juce::Component& label, float w) {
            label.setBounds(startX, area.getY(), (int)w, labelH);
            comp.setBounds(startX + ((int)w - sliderWidth)/2, sliderY, sliderWidth, sliderH);
            startX += (int)w;
        };

        // Freq
        formatCol(cutoffSlider, cutoffLabel, standardColW);
        // Res
        formatCol(resSlider, resLabel, standardColW);
        
        // Polarity Switch
        int switchW = 30;
        int switchH = 50;
        int switchY = area.getY() + (area.getHeight() - switchH) / 2;
        invSwitch.setBounds(startX + ((int)switchColW - switchW)/2, switchY, switchW, switchH);
        startX += (int)switchColW;

        // Env
        formatCol(envSlider, envLabel, standardColW);
        // LFO
        formatCol(lfoSlider, lfoLabel, standardColW);
        // Kybd
        formatCol(keySlider, keyLabel, standardColW);
    }

private:
    juce::Slider cutoffSlider, resSlider, envSlider, lfoSlider, keySlider;
    juce::Slider invSwitch; 
    juce::Label cutoffLabel, resLabel, envLabel, lfoLabel, keyLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAtt, resAtt, envAtt, lfoAtt, keyAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> invAtt;

    juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection> midiLearnListeners;
};
