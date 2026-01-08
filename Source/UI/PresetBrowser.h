#pragma once

#include <JuceHeader.h>
#include "../Core/PresetManager.h"

class PresetBrowser : public juce::Component
{
public:
    PresetBrowser(PresetManager& pm);
    ~PresetBrowser() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void refreshPresetList();
    
    // External Control
    void setPresetIndex(int index);
    void nextPreset();
    void prevPreset();
    void savePreset();
    void loadPreset();
    
    // Callbacks
    
    PresetManager& getPresetManager() { return presetManager; }

    std::function<void(const juce::String&)> onPresetChanged;
    std::function<juce::ValueTree()> onGetCurrentState;
    
private:
    PresetManager& presetManager;
    juce::ComboBox presetList;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
