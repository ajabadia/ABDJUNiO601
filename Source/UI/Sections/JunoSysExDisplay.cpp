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
    currentData.resize(24, 0);
}

JunoSysExDisplay::~JunoSysExDisplay()
{
}

void JunoSysExDisplay::updateData(const juce::MidiMessage& msg)
{
    if (!msg.isSysEx()) return;
    
    const uint8_t* raw = msg.getRawData();
    int size = msg.getRawDataSize();
    
    // Check for changes to highlight
    lastChangedIndex = -1;
    bool sizeChanged = (size != (int)currentData.size());
    
    if (sizeChanged) {
        currentData.resize(size);
    }
    
    for (int i = 0; i < size; ++i) {
        if (i < currentData.size()) {
            if (currentData[i] != raw[i]) {
                lastChangedIndex = i;
            }
        }
        currentData[i] = raw[i];
    }
    
    repaint();
}

void JunoSysExDisplay::paint(juce::Graphics& g)
{
    // Background (transparent or very subtle)
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
    
    if (currentData.empty()) return;
    
    int fontSize = 14; 
    g.setFont(juce::FontOptions("Courier New", (float)fontSize, juce::Font::plain));
    
    int xStart = 5;
    int x = xStart;
    int y = 0;
    int w = 20; // width per byte char pair
    int rowHeight = getHeight() / 2;
    
    for (int i = 0; i < (int)currentData.size(); ++i) {
        if (i == 12) { // Start second row
             x = xStart;
             y += rowHeight;
        }

        juce::String hex = juce::String::toHexString(currentData[i]).toUpperCase();
        if (hex.length() < 2) hex = "0" + hex;
        
        if (i == lastChangedIndex) g.setColour(highlightColour);
        else g.setColour(defaultColour);
        
        g.drawText(hex, x, y, w, rowHeight, juce::Justification::centredLeft);
        x += 24; // advance with some spacing
    }
}

void JunoSysExDisplay::resized()
{
}
