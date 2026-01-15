#pragma once
#include <JuceHeader.h>

/**
 * DesignTokens.h
 * Global UI Constants for JUNiO 601
 */
namespace JunoUI 
{
    /**
     * Centralized Color Palette
     */
    namespace Colors 
    {
        const juce::Colour kPanelGrey      = juce::Colour(0xff3b3b3b);
        const juce::Colour kPanelDarkGrey  = juce::Colour(0xff222222);
        const juce::Colour kStripRed       = juce::Colour(0xffcf3838);
        const juce::Colour kStripBlue      = juce::Colour(0xff60a8d6);
        const juce::Colour kStripOrange    = juce::Colour(0xffF48C28);
        const juce::Colour kTextWhite      = juce::Colours::white;
        const juce::Colour kTextGrey       = juce::Colour(0xffaaaaaa);
        const juce::Colour kTextDark       = juce::Colour(0xff333333); // Dark text for light backgrounds
        const juce::Colour kBeigeButton    = juce::Colour(0xffE6E1D3);
        const juce::Colour kSliderCapWhite = juce::Colour(0xffe0e0e0);
        const juce::Colour kBenderBox      = juce::Colour(0xff1a1a1a); // Darker background for Bender
        const juce::Colour kLedRed         = juce::Colour(0xffff3030);
        const juce::Colour kLedRedDark     = juce::Colour(0xff502020);
        const juce::Colour kLedGreen       = juce::Colour(0xff82FF4C);
        const juce::Colour kLedYellow      = juce::Colour(0xffFFD23B);
    }

    /**
     * Shared Layout Constants
     */
    namespace Layout 
    {
        const float kCornerRadius          = 4.0f;
        const float kComponentGap          = 10.0f;
        const float kSectionHeaderHeight   = 30.0f;
        const float kSidebarWidth          = 240.0f;
        const float kHeaderHeight          = 80.0f;
    }

    /**
     * Design Fonts
     */
    namespace Fonts 
    {
        inline juce::Font getMainFont(float size, bool bold = false) {
            return juce::FontOptions("Arial", size, bold ? juce::Font::bold : juce::Font::plain);
        }
    }

    // --- Backward Compatibility (Clean Aliases) ---
    using namespace Colors;
    
    inline juce::Font getPanelFont(float size, bool bold = false) {
        return Fonts::getMainFont(size, bold);
    }
}
