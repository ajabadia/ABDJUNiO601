/*
  ==============================================================================

    JunoSysExDisplay.cpp
    Created: 9 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#include "JunoSysExDisplay.h"

JunoSysExDisplay::JunoSysExDisplay()
{
    // Default empty state 
    currentDump.resize(24, 0);
	lastDump = currentDump;
}

JunoSysExDisplay::~JunoSysExDisplay()
{
}

void JunoSysExDisplay::setDumpData(const std::vector<uint8_t>& dump)
{
    if (dump == currentDump) 
        return;

    lastDump = currentDump;
    currentDump = dump;
    
    repaint();
}

void JunoSysExDisplay::paint(juce::Graphics& g)
{
    // Background (transparent or very subtle)
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
    
    if (currentDump.empty()) return;
    
    int fontSize = 14; 
    g.setFont(juce::FontOptions("Courier New", (float)fontSize, juce::Font::plain));
    
    int xStart = 5;
    int x = xStart;
    int y = 0;
    int byteW = 22; // Width per byte block
    int rowHeight = 16;
    int maxW = getWidth() - 10;
    
    for (int i = 0; i < (int)currentDump.size(); ++i) {
        // Wrap if needed
        if (x + byteW > maxW) {
             x = xStart;
             y += rowHeight;
        }

        juce::String hex = juce::String::toHexString(currentDump[i]).toUpperCase();
        if (hex.length() < 2) hex = "0" + hex;
        
        bool isChanged = (i >= lastDump.size() || lastDump[i] != currentDump[i]);

        if (isChanged) g.setColour(highlightColour);
        else g.setColour(defaultColour);
        
        g.drawText(hex, x, y, byteW, rowHeight, juce::Justification::centredLeft);
        x += 24; 
    }
}

void JunoSysExDisplay::resized()
{
}
