#pragma once
#include <JuceHeader.h>
#include "JunoUIHelpers.h"

class JunoBender : public juce::Component {
public:
    JunoBender();
    ~JunoBender() override = default;
    
    void attachToParameters(juce::AudioProcessorValueTreeState& apvts);
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    
    juce::Slider benderLever;
    std::unique_ptr<SliderAttachment> benderAttachment;
    
    juce::Slider dcoSlider, vcfSlider, lfoSlider;
    std::unique_ptr<SliderAttachment> dcoAttachment, vcfAttachment, lfoAttachment;
    juce::Label dcoLabel, vcfLabel, lfoLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JunoBender)
};

