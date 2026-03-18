#pragma once
#include <JuceHeader.h>

/**
 * ScaledComponent
 * A base class for components that need to respond to a global scale factor
 * or handle high-DPI awareness explicitly aside from JUCE's desktop scaling.
 * 
 * In this specific architecture, it helps child components calculate standard sizes.
 */
class ScaledComponent : public juce::Component
{
public:
    ScaledComponent() = default;
    ~ScaledComponent() override = default;

    void setScale(float newScale)
    {
        if (std::abs(scale - newScale) > 0.001f)
        {
            scale = newScale;
            // Optionally trigger a resize or repaint if logic depends on it
            // Typically in JUCE setTransform is used on the parent, 
            // but if we do manual layout calcs:
            resized(); 
            repaint();
        }
    }

    float getScale() const { return scale; }

    // Helper to scale values (e.g. font sizes, standard margins)
    int s(int pixels) const { return (int)(pixels * scale); }
    float sf(float pixels) const { return pixels * scale; }

private:
    float scale = 1.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScaledComponent)
};
