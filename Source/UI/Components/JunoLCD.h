/*
  ==============================================================================

    JunoLCD.h
    Created: 14 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../DesignTokens.h"

namespace JunoUI
{
    class JunoLCD : public juce::Component
    {
    public:
        JunoLCD()
        {
            setOpaque(false);
        }
        
        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            
            // Housing
            g.setColour(JunoUI::Colors::kPanelDarkGrey);
            g.fillRoundedRectangle(b, 4.0f);
            
            // Screen
            g.setColour(juce::Colours::black);
            g.fillRoundedRectangle(b.reduced(2.0f), 2.0f);
            
            juce::Colour ledRed = JunoUI::Colors::kLedRed;
            
            // [Fidelidad] "Sub-atomic Glow" 
            g.setColour(ledRed.withAlpha(0.25f));
            g.setFont(juce::FontOptions("Courier New", 24.0f, juce::Font::bold));
            g.drawText(text, getLocalBounds(), juce::Justification::centred);
            
            g.setColour(ledRed); 
            g.setFont(juce::FontOptions("Courier New", 22.0f, juce::Font::bold));
            g.drawText(text, getLocalBounds(), juce::Justification::centred);
        }

        void setText(const juce::String& t) { text = t; repaint(); }

    private:
        juce::String text = "--";
    };
}
