#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"
#include "../JunoBender.h"

// Check correct forward declaration for MidiLearnHandler
class MidiLearnHandler;

class JunoPerformanceSection : public juce::Component
{
public:
    JunoPerformanceSection(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh)
    {
        juce::ignoreUnused(mlh); // Future use if Bender supports it
        
        // Bender (Includes Lever + DCO/VCF/LFO sliders)
        addAndMakeVisible(bender);
        bender.attachToParameters(apvts);
    }

    void paint(juce::Graphics& g) override
    {
        // Dark background for performance section
        g.fillAll(JunoUI::kBenderBox); 
        
        // Right separator border
        g.setColour(juce::Colours::black);
        g.fillRect(getWidth()-2, 0, 2, getHeight());
        
        // White outline adjustment if needed? User said "Borde blanco o gris claro alrededor"
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawRect(getLocalBounds().reduced(1), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(5);
        
        // Bender takes the space
        // JunoBender is designed to look like the whole block
        bender.setBounds(r);
    }

private:
    JunoBender bender;
    // Portamento moved to Control Strip as per User Request (Option 1)
};
