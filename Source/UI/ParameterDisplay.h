#pragma once
#include <JuceHeader.h>
#include "JunoUIHelpers.h"

class ParameterDisplay  : public juce::Component, public juce::Timer
{
public:
    ParameterDisplay()
    {
        // Configuración inicial
    }

    void showParameter(const juce::String& name, const juce::String& value)
    {
        paramName = name;
        paramValue = value;
        setVisible(true);
        toFront(true); // Ensure it's on top of everything
        startTimer(2000); // Se ocultará en 2 segundos
        repaint();
    }

    void setJustificationType(juce::Justification j)
    {
        justification = j;
        splitLayout = false; // User explicitly requested a specific alignment
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // Fondo oscuro y translúcido
        g.setColour(JunoUI::kPanelGrey.withAlpha(0.9f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);
        g.setColour(JunoUI::kTextWhite);
        
        if (splitLayout)
        {
            // Dibujar el nombre del parámetro y su valor separados (Layout original)
            g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
            g.drawText(paramName, getLocalBounds().reduced(10, 0), juce::Justification::centredLeft, true);
            
            g.setFont(juce::FontOptions(36.0f, juce::Font::bold));
            g.drawText(paramValue, getLocalBounds().reduced(10, 0), juce::Justification::centredRight, true);
        }
        else
        {
            // Dibujar juntos con la justificación pedida
            g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
            g.drawText(paramName + " " + paramValue, getLocalBounds().reduced(10, 0), justification, true);
        }
    }

    void timerCallback() override
    {
        setVisible(false);
        stopTimer();
    }

private:
    juce::String paramName;
    juce::String paramValue;
    juce::Justification justification = juce::Justification::centred;
    bool splitLayout = true;
};
