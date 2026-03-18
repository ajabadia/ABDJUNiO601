#include "JunoSidePanel.h"
#include "../../UI/JunoBender.h"

JunoSidePanel::JunoSidePanel(juce::AudioProcessorValueTreeState& apvts, MidiLearnHandler& mlh)
{
    // === BENDER ===
    addAndMakeVisible(bender);
    bender.attachToParameters(apvts);

    // Portamento and Assign moved to JunoControlSection (Phase 9)
}

void JunoSidePanel::paint(juce::Graphics& g)
{
    auto b = getLocalBounds();
    
    // Draw "Module Frame" as requested in Phase 9
    g.setColour(JunoUI::kPanelGrey);
    g.fillRect(b); 
    
    g.setColour(juce::Colours::black);
    g.drawRect(b, 1); // Subtle border
}

void JunoSidePanel::resized()
{
    auto r = getLocalBounds().reduced(5);
    
    if (!bender.isVisible()) bender.setVisible(true);

    bender.setBounds(r);
}
