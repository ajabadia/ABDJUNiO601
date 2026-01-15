/*
  ==============================================================================

    JunoKnob.h
    Created: 14 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "../DesignTokens.h"

namespace JunoUI
{
    class JunoKnob : public juce::Slider
    {
    public:
        JunoKnob()
        {
            setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            
            // Set default look and feel colors using DesignTokens
            // Note: Custom LookAndFeel injection is preferred for rendering,
            // but for now we set component-level properties or rely on the global LnF.
            // A better pattern for Atomization is to have a self-contained LookAndFeel
            // or a paint method override if we want strict control without global dependence.
            
            // For now, let's use a minimal correct configuration.
        }

        // We can override paint if needed, or rely on a custom LookAndFeel attached to this component.
        // To keep it "Atomic", let's give it a simple LookAndFeel method or expect the parent to style it.
        // However, the goal is *atomization*, so it should be self-contained.
        
        // Let's implement a simple custom LookAndFeel *inside* or *related* to this knob
        // to ensure it always looks like a Juno Knob regardless of global state.
        
        class KnobLookAndFeel : public juce::LookAndFeel_V4
        {
        public:
            void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                juce::Slider& slider) override
            {
                auto outline = slider.findColour(juce::Slider::rotarySliderOutlineColourId);
                auto fill    = slider.findColour(juce::Slider::rotarySliderFillColourId);

                auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2);
                auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
                auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
                
                auto centre = bounds.getCentre();
                
                // Shadow
                g.setColour(juce::Colours::black.withAlpha(0.3f));
                g.fillEllipse(centre.getX() - radius + 2, centre.getY() - radius + 2, radius * 2, radius * 2);

                // Body
                g.setColour(juce::Colours::white); // Juno knobs are often white or cream
                g.fillEllipse(centre.getX() - radius, centre.getY() - radius, radius * 2, radius * 2);
                
                // Border
                g.setColour(juce::Colours::grey);
                g.drawEllipse(centre.getX() - radius, centre.getY() - radius, radius * 2, radius * 2, 1.0f);
                
                // Pointer / Indicator
                juce::Path p;
                auto pointerLength = radius * 0.7f;
                auto pointerThickness = 3.0f;
                p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
                p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(centre));
                
                g.setColour(JunoUI::Colors::kTextDark);
                g.fillPath(p);
            }
        };

        // We can keep an instance or shared pointer if we want to enforce this LAF per instance
        // But for memory efficiency, usually LAF is shared.
        // For this refactor, let's create a static shared instance or just define the logic.
    };
}
