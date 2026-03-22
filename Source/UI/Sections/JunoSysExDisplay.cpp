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
    int byteW = 22; 
    int rowHeight = 16;
    
    for (int i = 0; i < (int)currentDump.size(); ++i) {
        // [Fidelidad] Mandatory layout: First row = 12 bytes, Second row = rest
        int row = (i < 12) ? 0 : 1;
        int col = (i < 12) ? i : (i - 12);
        
        int x = xStart + (col * 30); // Controlled spacing for 12 pairs
        int y = row * rowHeight;

        juce::String hex = juce::String::toHexString(currentDump[i]).toUpperCase();
        if (hex.length() < 2) hex = "0" + hex;
        
        bool isChanged = (i >= lastDump.size() || lastDump[i] != currentDump[i]);

        if (isChanged) g.setColour(highlightColour);
        else g.setColour(defaultColour);
        
        g.drawText(hex, x, y, byteW, rowHeight, juce::Justification::centredLeft);
    }
}

void JunoSysExDisplay::resized()
{
}
