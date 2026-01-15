/*
  ==============================================================================

    SevenSegmentDisplay.h
    Created: 13 Jan 2026
    Author:  Antigravity (Procedural UX)

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class SevenSegmentDisplay : public juce::Component {
public:
    SevenSegmentDisplay() {
        setOpaque(false);
    }
    
    void setValue(int v) {
        if (value != v) {
            value = v;
            repaint();
        }
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

        // Encoding 0-9
        const uint8_t enc[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
        uint8_t mask = (value >= 0 && value <= 9) ? enc[value] : 0x00;
        if (value == -1) mask = 0x40; // Minus (-1) shows G

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
    int value = 8;
    // [Fidelidad] Authentic Roland Colors from SVG
    juce::Colour onColor = juce::Colour(0xffff3030); // #ff3030
    juce::Colour offColor = juce::Colour(0xff502020); // #502020
};
