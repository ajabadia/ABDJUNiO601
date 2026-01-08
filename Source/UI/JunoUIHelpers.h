#pragma once
#include <JuceHeader.h>
#include "../Core/MidiLearnHandler.h"

namespace JunoUI
{
    // Forward declaration for OwnedArray usage
    class MidiLearnMouseListener;

    // Colour palette
    const juce::Colour kPanelGrey = juce::Colour(0xff3b3b3b); 
    const juce::Colour kBenderBox = juce::Colour(0xff2a2a2a); 
    const juce::Colour kSliderCapWhite = juce::Colour(0xffe8e8e3); 
    const juce::Colour kStripRed = juce::Colour(0xffcf3838); 
    const juce::Colour kStripBlue = juce::Colour(0xff60a8d6); 
    const juce::Colour kTextWhite = juce::Colour(0xffffffff); 
 
    const juce::Colour kTextGrey = juce::Colour(0xffaaaaaa); 
    const juce::Colour kPanelDarkGrey = juce::Colour(0xff222222); // Added for TextButtons 

    // LookAndFeel placeholder for Juno UI
    class JunoLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        JunoLookAndFeel()
        {
            setColour(juce::ResizableWindow::backgroundColourId, kPanelGrey);
        }
    };

    inline void drawJunoSection(juce::Graphics& g, juce::Rectangle<int> b, const juce::String& title, bool isBlue = false)
    {
        g.setColour(kPanelGrey);
        g.fillRect(b);
        auto header = b.removeFromTop(28);
        g.setColour(isBlue ? kStripBlue : kStripRed);
        g.fillRect(header);
        g.setColour(kTextWhite);
        g.setFont(juce::FontOptions("Arial", 14.0f, juce::Font::bold));
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
    inline void setupLabel(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, kTextWhite);
    }

    inline void setupLabel(juce::Label& label, const char* text, juce::Component& parent)
    {
        label.setText(juce::String(text), juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, kTextWhite);
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
        comp.addMouseListener(new MidiLearnMouseListener(handler, paramID), true);
    }

    // 4-argument overload: STRICT usage of OwnedArray for ownership
    inline void setupMidiLearn(juce::Component& comp,
                               MidiLearnHandler& handler,
                               const juce::String& paramID,
                               juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection>& listenerArray)
    {
        // User requested OwnedArray usage.
        // We create the listener, add it to the array (which owns it),
        // and add it to the component WITHOUT taking ownership (false/default).
        auto* newListener = new MidiLearnMouseListener(handler, paramID);
        listenerArray.add(newListener); 
        comp.addMouseListener(newListener, false);
    }

    // Forwarding for const char*
    inline void setupMidiLearn(juce::Component& comp,
                               MidiLearnHandler& handler,
                               const char* paramID,
                               juce::OwnedArray<JunoUI::MidiLearnMouseListener, juce::DummyCriticalSection>& listenerArray)
    {
        setupMidiLearn(comp, handler, juce::String(paramID), listenerArray);
    }

    // LCD component
    class JunoLCD : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override
        {
            g.setColour(juce::Colours::black);
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 2.0f);
            g.setColour(juce::Colour(0xffff3030)); // Rojo LED
            g.setFont(juce::FontOptions("Courier New", 22.0f, juce::Font::bold));
            g.drawText(text, getLocalBounds(), juce::Justification::centred);
        }
        void setText(const juce::String& t) { text = t; repaint(); }
    private:
        juce::String text = "--";
    };
}
