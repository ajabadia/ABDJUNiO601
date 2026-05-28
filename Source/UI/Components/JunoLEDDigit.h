/*
  ==============================================================================

    JunoLEDDigit.h
    Created: 8 Apr 2026
    Author:  Antigravity (Procedural UX)

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <cctype>

/**
 * [Aesthetic upgrade] High-fidelity LED digit using PNG sprites.
 * Matches the WebUI's "Gold Standard" appearance.
 */
class JunoLEDDigit : public juce::Component {
public:
    JunoLEDDigit() {
        setOpaque(false);
    }
    
    void setCharacter(char c) {
        c = (char)std::toupper(c);
        if (currentChar != c) {
            currentChar = c;
            updateImage();
            repaint();
        }
    }
    
    // Legacy support for numeric values
    void setValue(int v) {
        if (v >= 0 && v <= 9) setCharacter((char)('0' + v));
        else if (v == -1) setCharacter('-');
        else setCharacter(' ');
    }

    void paint(juce::Graphics& g) override {
        if (cachedImage.isValid()) {
            g.drawImageWithin(cachedImage, 0, 0, getWidth(), getHeight(),
                              juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        } else if (currentChar != ' ') {
            // Fallback to simple segment if image fails (unlikely)
            g.setColour(juce::Colours::red.withAlpha(0.3f));
            g.drawRect(getLocalBounds().toFloat());
            g.setFont((float)getHeight() * 0.8f);
            g.drawText(juce::String::charToString(currentChar), getLocalBounds(), juce::Justification::centred);
        }
    }

private:
    char currentChar = ' ';
    juce::Image cachedImage;

    void updateImage() {
        if (currentChar == ' ' || currentChar == '\0') {
            cachedImage = juce::Image();
            return;
        }

        juce::String resourceName;
        if (std::isdigit(currentChar)) {
            resourceName = "_" + juce::String::charToString(currentChar) + "_png";
        } else if (currentChar == '-') {
            resourceName = "DASH_png";
        } else if (std::isalpha(currentChar)) {
            resourceName = juce::String::charToString(currentChar) + "_png";
        } else {
            cachedImage = juce::Image();
            return;
        }

        int size = 0;
        const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), size);
        
        if (data != nullptr) {
            cachedImage = juce::ImageCache::getFromMemory(data, size);
        } else {
            cachedImage = juce::Image();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JunoLEDDigit)
};
