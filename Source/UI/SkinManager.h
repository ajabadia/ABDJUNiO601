#pragma once
#include <JuceHeader.h>
#include "DesignTokens.h"

namespace JunoUI
{
    /**
     * SkinManager - Centralized UI State (Singleton)
     */
    class SkinManager : public juce::ChangeBroadcaster
    {
    public:
        static SkinManager& getInstance()
        {
            static SkinManager instance;
            return instance;
        }

        struct Palette
        {
            juce::Colour background;
            juce::Colour panelBackground;
            juce::Colour panelHeaderRed;
            juce::Colour panelHeaderBlue;
            juce::Colour textMain;
            juce::Colour textDim;
            juce::Colour accentRed;
            juce::Colour accentBlue;
            juce::Colour sliderCap;
            juce::Colour sliderTrack;
            juce::Colour ledOn;
            juce::Colour ledOff;
            juce::Colour beigeButton;
        };

        enum class Theme { Light, Dark, Classic };

        void setTheme(Theme t)
        {
            if (currentTheme != t) {
                currentTheme = t;
                updatePalette();
                sendChangeMessage();
            }
        }

        Theme getTheme() const { return currentTheme; }
        const Palette& getPalette() const { return currentPalette; }
        
    private:
        SkinManager()
        {
            currentTheme = Theme::Classic;
            updatePalette();
        }

        Theme currentTheme;
        Palette currentPalette;

        void updatePalette()
        {
            using namespace Colors;
            
            // Default Classic Juno-106 Palette
            currentPalette.background      = kPanelDarkGrey;
            currentPalette.panelBackground = kPanelGrey;
            currentPalette.panelHeaderRed  = kStripRed;
            currentPalette.panelHeaderBlue = kStripBlue;
            currentPalette.textMain        = kTextWhite;
            currentPalette.textDim         = kTextGrey;
            currentPalette.accentRed       = kStripRed;
            currentPalette.accentBlue      = kStripBlue;
            currentPalette.sliderCap       = juce::Colour(0xffe8e8e3);
            currentPalette.sliderTrack     = juce::Colours::black;
            currentPalette.ledOn           = kLedRed;
            currentPalette.ledOff          = kLedRedDark;
            currentPalette.beigeButton     = kBeigeButton;

            if (currentTheme == Theme::Dark)
            {
                currentPalette.panelBackground = juce::Colour(0xff1a1a1a);
                currentPalette.background      = juce::Colours::black;
            }
            else if (currentTheme == Theme::Light)
            {
                currentPalette.panelBackground = juce::Colour(0xfff0f0f0);
                currentPalette.background      = juce::Colour(0xffe0e0e0);
                currentPalette.textMain        = juce::Colours::black;
                currentPalette.textDim         = juce::Colour(0xff666666);
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkinManager)
    };
}
