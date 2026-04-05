/*
  ==============================================================================

    SevenSegmentDisplay.h
    Created: 13 Jan 2026
    Author:  Antigravity (Procedural UX)

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <cctype>
#include <cstdint>

class SevenSegmentDisplay : public juce::Component {
public:
    SevenSegmentDisplay() {
        setOpaque(false);
    }
    
    void setCharacter(char c) {
        if (currentChar != c) {
            currentChar = c;
            repaint();
        }
    }
    
    // Legacy support
    void setValue(int v) {
        if (v >= 0 && v <= 9) setCharacter((char)('0' + v));
        else if (v == -1) setCharacter('-');
        else setCharacter(' ');
    }

    void setColor(juce::Colour c) {
        onColor = c;
        offColor = c.withAlpha(0.08f); // Low ghosting
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        float w = bounds.getWidth();
        float h = bounds.getHeight();
        
        // Authenticity: 10 degrees slant
        juce::AffineTransform slant = juce::AffineTransform::shear(-0.2f, 0.0f).translated(w * 0.15f, 0);
        
        // Dimensions
        float t = w * 0.14f; // Thickness
        float gS = t * 0.3f; // Gap size
        float cy = h * 0.5f;

        // Expanded Encoding: A-G segments
        juce::uint8 mask = 0x00;
        switch (std::toupper(currentChar)) {
            case '0': mask = 0x3F; break;
            case '1': mask = 0x06; break;
            case '2': mask = 0x5B; break;
            case '3': mask = 0x4F; break;
            case '4': mask = 0x66; break;
            case '5': mask = 0x6D; break;
            case '6': mask = 0x7D; break;
            case '7': mask = 0x07; break;
            case '8': mask = 0x7F; break;
            case '9': mask = 0x6F; break;
            case 'A': mask = 0x77; break;
            case 'B': mask = 0x7C; break; // lowercase 'b' for clarity
            case 'C': mask = 0x39; break;
            case 'D': mask = 0x5E; break; // lowercase 'd'
            case 'E': mask = 0x79; break;
            case 'F': mask = 0x71; break;
            case 'L': mask = 0x38; break;
            case 'P': mask = 0x73; break;
            case 'U': mask = 0x3E; break;
            case '-': mask = 0x40; break;
            case '_': mask = 0x08; break;
            default : mask = 0x00; break;
        }

        auto draw = [&](int idx, float x, float y, float sw, float sh) {
             juce::Path p;
             // Polygons for cleaner joints
             if (sw > sh) { // Horizontal
                 p.addRoundedRectangle(x + gS, y, sw - 2*gS, sh, sh*0.2f);
             } else { // Vertical
                 p.addRoundedRectangle(x, y + gS, sw, sh - 2*gS, sw*0.2f);
             }
             p.applyTransform(slant);
             
             bool on = (mask & (1 << idx));
             g.setColour(on ? onColor : offColor);
             g.fillPath(p);
             
             if (on) {
                 // Inner core brightness (Hot spot)
                 g.setColour(juce::Colours::white.withAlpha(0.2f));
                 juce::Path core = p;
                 juce::AffineTransform shrink = juce::AffineTransform::scale(0.6f, 0.6f, p.getBounds().getCentreX(), p.getBounds().getCentreY());
                 core.applyTransform(shrink);
                 g.fillPath(core);
             }
        };
        
        // Segments A-G
        // A (Top)
        draw(0, t, 0, w-2*t, t);
        // B (Top Right)
        draw(1, w-t, t, t, cy-t);
        // C (Bot Right)
        draw(2, w-t, cy, t, cy-t);
        // D (Bot)
        draw(3, t, h-t, w-2*t, t);
        // E (Bot Left)
        draw(4, 0, cy, t, cy-t);
        // F (Top Left)
        draw(5, 0, t, t, cy-t);
        // G (Mid)
        draw(6, t, cy-t/2, w-2*t, t);
    }

private:
    char currentChar = ' ';
    // [Fidelidad] Authentic Roland Colors from SVG
    juce::Colour onColor = juce::Colour(0xffff3030); // #ff3030
    juce::Colour offColor = juce::Colour(0xff502020); // #502020
};
