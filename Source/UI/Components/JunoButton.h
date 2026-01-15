/*
  ==============================================================================

    JunoButton.h
    Created: 14 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "../DesignTokens.h"

namespace JunoUI
{
    class JunoButton : public juce::TextButton
    {
    public:
        JunoButton(const juce::String& buttonName) : juce::TextButton(buttonName)
        {
            // Set base styling
            setColour(juce::TextButton::buttonColourId, JunoUI::Colors::kPanelDarkGrey);
            setColour(juce::TextButton::textColourOffId, JunoUI::Colors::kTextWhite);
            setColour(juce::TextButton::buttonOnColourId, JunoUI::Colors::kStripRed); 
        }

        // Additional behaviors (e.g., LED integration) can be added here
        void setStyle(bool isFunction)
        {
             if (isFunction) {
                 setColour(juce::TextButton::buttonColourId, JunoUI::Colors::kBeigeButton);
                 setColour(juce::TextButton::textColourOffId, JunoUI::Colors::kTextDark);
             }
        }
    };
}
