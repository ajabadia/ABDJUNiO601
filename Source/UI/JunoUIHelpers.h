#pragma once
#include <JuceHeader.h>
#include "DesignTokens.h"
#include "../Core/MidiLearnHandler.h"
#include "Components/JunoKnob.h"
#include "Components/JunoButton.h"
#include "Components/JunoLCD.h"

namespace JunoUI
{
    // Forward declaration for OwnedArray usage
    // (Forward declaration removed)

    // Colour constants are now in DesignTokens.h
    // Compatibility is maintained via 'using namespace Colors' in DesignTokens.h

    // LookAndFeel placeholder for Juno UI
    class JunoLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        JunoLookAndFeel()
        {
            setColour(juce::ResizableWindow::backgroundColourId, kPanelGrey);
            setColour(juce::TextButton::buttonColourId, kPanelDarkGrey);
            setColour(juce::TextButton::textColourOffId, kTextWhite);
        }

        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPos, float minSliderPos, float maxSliderPos,
                             const juce::Slider::SliderStyle style, juce::Slider& slider) override
        {
            if (style == juce::Slider::LinearVertical)
            {
                auto b = juce::Rectangle<int>(x, y, width, height).toFloat();
                float trackW = 4.0f;
                float centerX = b.getCentreX();
                
                // Track
                g.setColour(juce::Colours::black);
                g.fillRoundedRectangle(centerX - trackW/2, b.getY() + 5, trackW, b.getHeight() - 10, 2.0f);
                
                // Track Highlight (Hover)
                if (slider.isMouseOverOrDragging()) {
                    g.setColour(kStripBlue.withAlpha(0.15f));
                    g.drawRoundedRectangle(centerX - trackW/2 - 1, b.getY() + 4, trackW + 2, b.getHeight() - 8, 2.0f, 1.0f);
                }

                // Tick marks
                g.setColour(kTextGrey.withAlpha(0.3f));
                for (int i = 0; i <= 10; ++i) {
                    float ty = b.getY() + 10 + (b.getHeight() - 20) * (i / 10.0f);
                    g.drawHorizontalLine((int)ty, centerX - 8, centerX - 4);
                    g.drawHorizontalLine((int)ty, centerX + 4, centerX + 8);
                }

                // Cap
                auto capW = 26.0f;
                auto capH = 18.0f;
                auto capRect = juce::Rectangle<float>(centerX - capW/2, sliderPos - capH/2, capW, capH);
                
                // [Fidelidad] Detect switches (black caps) vs sliders (white caps)
                bool isSwitch = slider.getSliderSnapsToMousePosition();
                juce::Colour capBase = isSwitch ? juce::Colours::black.brighter(0.1f) : kSliderCapWhite;

                // [Fidelidad] Glass-like Cap Gradient
                juce::ColourGradient capGrad (capBase, capRect.getCentreX(), capRect.getY(),
                                              capBase.darker(0.2f), capRect.getCentreX(), capRect.getBottom(), false);
                g.setGradientFill(capGrad);
                g.fillRoundedRectangle(capRect, 2.0f);
                
                // Reflection line
                g.setColour(juce::Colours::white.withAlpha(isSwitch ? 0.3f : 0.6f));
                g.drawHorizontalLine((int)(capRect.getY() + 2), capRect.getX() + 2, capRect.getRight() - 2);

                juce::ignoreUnused(minSliderPos, maxSliderPos);

                // Cap Stripe [Horizontal]
                g.setColour(kStripRed);
                g.fillRect(capRect.getX() + 4, capRect.getCentreY() - 1, capRect.getWidth() - 8, 2.0f);
                
                // Cap Highlight (Hover) - "Sub-atomic Glow"
                if (slider.isMouseOverOrDragging()) {
                    // Outer glow
                    g.setColour(kStripRed.withAlpha(0.3f));
                    g.drawRoundedRectangle(capRect.expanded(2.0f), 4.0f, 2.0f);
                    
                    // Inner highlight
                    g.setColour(juce::Colours::white.withAlpha(0.3f));
                    g.drawRoundedRectangle(capRect.reduced(0.5f), 2.0f, 1.0f);
                }
            }
            else if (style == juce::Slider::LinearHorizontal) // [Bender Support]
            {
                auto b = juce::Rectangle<int>(x, y, width, height).toFloat();
                float trackH = 10.0f;
                float centerY = b.getCentreY();
                
                // Track (Slot)
                g.setColour(juce::Colours::black);
                g.fillRoundedRectangle(b.getX() + 5, centerY - trackH/2, b.getWidth() - 10, trackH, 4.0f);
                
                // Track Highlight (Center indent)
                g.setColour(juce::Colours::darkgrey);
                g.drawHorizontalLine((int)centerY, b.getX() + 10, b.getRight() - 10);

                // LEVER Cap
                float capW = 30.0f;
                // float capH = b.getHeight() - 4.0f; // Unused
                float capX = sliderPos - capW/2;
                
                // 3D Lever Shaft (Base)
                g.setColour(juce::Colours::grey);
                g.fillRect(capX + 10, centerY - 5, 10.0f, 10.0f); 
                
                // Stick/Handle
                auto handleRect = juce::Rectangle<float>(capX + 5, centerY - 15, 20.0f, 30.0f);
                
                juce::ColourGradient grad (juce::Colours::white, handleRect.getCentreX(), handleRect.getY(),
                                           juce::Colours::lightgrey, handleRect.getCentreX(), handleRect.getBottom(), false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(handleRect, 4.0f);
                
                g.setColour(juce::Colours::black.withAlpha(0.3f));
                g.drawRoundedRectangle(handleRect, 4.0f, 1.0f);
                
                // Red Tip (Authentic Roland look)
                g.setColour(kStripRed);
                g.fillRoundedRectangle(handleRect.withHeight(6.0f), 2.0f);
            }
        }

        void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                  bool isMouseOverButton, bool isButtonDown) override
        {
            auto b = button.getLocalBounds().toFloat();
            
            // [Fidelidad] Mechanical click shift
            if (isButtonDown) b = b.reduced(1.0f);

            auto baseCol = backgroundColour;
            
            if (isButtonDown) baseCol = baseCol.darker(0.2f);
            else if (isMouseOverButton) baseCol = baseCol.brighter(0.1f);
            
            g.setColour(baseCol);
            g.fillRoundedRectangle(b, 2.0f);
            
            // LED Bloom if ON
            if (button.getToggleState()) {
                g.setColour(kStripRed.withAlpha(0.4f));
                g.fillRoundedRectangle(b.expanded(3.0f), 5.0f);
                
                g.setColour(kStripRed.withAlpha(0.2f));
                g.fillRoundedRectangle(b.expanded(6.0f), 8.0f);

                // LED center intensity
                g.setColour(kStripRed.brighter(0.4f).withAlpha(0.5f));
                g.fillRoundedRectangle(b, 2.0f);
            }
            
            g.setColour(juce::Colours::black.withAlpha(0.2f));
            g.drawRoundedRectangle(b.reduced(0.5f), 2.0f, 1.0f);
        }
    };



    inline void drawJunoSection(juce::Graphics& g, juce::Rectangle<int> b, const juce::String& title, bool isBlue = false)
    {
        // [Premium] Glassmorphism-style background
        g.setColour(kPanelGrey);
        g.fillRect(b);
        
        juce::ColourGradient grad (kPanelGrey.brighter(0.04f), 0, (float)b.getY(),
                                   kPanelGrey.darker(0.04f), 0, (float)b.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRect(b);

        auto header = b.removeFromTop(28);
        g.setColour(isBlue ? kStripBlue : kStripRed);
        g.fillRect(header);
        
        // Inner highlight for "Glass" feel
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine(header.getBottom(), (float)b.getX(), (float)b.getRight());

        g.setColour(kTextWhite);
        g.setFont(getPanelFont(14.0f));
        g.drawText(title, header, juce::Justification::centred);
        
        g.setColour(juce::Colours::black);
        g.drawVerticalLine(b.getRight() - 1, (float)b.getY(), (float)b.getBottom());
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawVerticalLine(b.getRight(), (float)b.getY(), (float)b.getBottom());
    }


    inline void drawJunoSectionPanel(juce::Graphics& g, juce::Rectangle<int> b, const juce::String& title)
    {
        drawJunoSection(g, b, title, false);
    }

    // UI helper functions
    inline void setupVerticalSlider(juce::Slider& slider, const juce::String& name = {})
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        if (name.isNotEmpty())
            slider.setName(name);
    }

    inline void styleSwitchSlider(juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setSliderSnapsToMousePosition(true); // FIX: Snapping enabled for easy clicking
    }

    inline void styleSmallSlider(juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, kTextWhite);
        slider.setColour(juce::Slider::thumbColourId, kStripRed);
    }

    inline void styleToggleButton(juce::ToggleButton& btn)
    {
        btn.setColour(juce::ToggleButton::tickColourId, kTextWhite);
        btn.setColour(juce::ToggleButton::tickDisabledColourId, kTextGrey);
    }

    // setupLabel overloads
    inline void setupLabel(juce::Label& label, const juce::String& text, float fontSize = 12.0f)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, kTextWhite);
        label.setFont(getPanelFont(fontSize));
        label.setJustificationType(juce::Justification::centred);
    }

    inline void setupLabel(juce::Label& label, const char* text, juce::Component& parent, float fontSize = 12.0f)
    {
        label.setText(juce::String(text), juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, kTextWhite);
        label.setFont(getPanelFont(fontSize));
        label.setJustificationType(juce::Justification::centred);
        parent.addAndMakeVisible(label);
    }

    // MidiLearnMouseListener
    class MidiLearnMouseListener : public juce::Component
    {
    public:
        MidiLearnMouseListener() = default;
        MidiLearnMouseListener(MidiLearnHandler& handler, const juce::String& paramID)
            : midiHandler(&handler), parameterID(paramID) {}

        void setHandler(MidiLearnHandler& handler, const juce::String& paramID)
        {
            midiHandler = &handler;
            parameterID = paramID;
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (midiHandler != nullptr && e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem(1, "Learn MIDI CC");
                m.addItem(2, "Clear MIDI Mapping");
                
                m.showMenuAsync(juce::PopupMenu::Options(), [this](int result) {
                    if (result == 1) midiHandler->startLearning(parameterID);
                    else if (result == 2) midiHandler->unbindParam(parameterID); // Assuming unbindParam exists in Handler
                });
            }
        }

    private:
        MidiLearnHandler* midiHandler = nullptr;
        juce::String parameterID;
    };

    // setupMidiLearn overloads
    inline void setupMidiLearn(juce::Component& comp,
                               MidiLearnHandler& handler,
                               const juce::String& paramID)
    {
        // [Critical Fix] Memory Safe Listener
        // We create the listener and add it as a CHILD COMPONENT.
        // This ensures the Component owns the memory and deletes it automatically.
        // We also register it as a MouseListener.
        MidiLearnMouseListener* listener = new MidiLearnMouseListener(handler, paramID);
        comp.addChildComponent(listener); // Ownership transferred here
        comp.addMouseListener(listener, true); // Logic hook
    }

    // Forwarding for const char*
    inline void setupMidiLearn(juce::Component& comp,
                               MidiLearnHandler& handler,
                               const char* paramID)
    {
        setupMidiLearn(comp, handler, juce::String(paramID));
    }
    
    // [Legacy] Removed unsafe OwnedArray overloads to prevent crashes.
    // Callers should allow the Component to own the listener.

    // JunoLCD has been moved/updated.
}
