#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../ABD-SynthEngine/Protocol/Presets/PresetManagerBase.h"
class PresetManager;

class PresetBrowser : public juce::Component, public juce::ListBoxModel
{
public:
    PresetBrowser(PresetManager& pm);
    ~PresetBrowser() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
    
    void refresh();
    void setPresetIndex(int idx) { refresh(); } // Legacy compatibility
    
    PresetManager& getPresetManager();
    
    // Callbacks
    std::function<void(int libraryIndex, int presetIndex)> onPresetSelected;
    std::function<void()> onSaveClicked;
    std::function<void()> onSaveAsClicked;

private:
    PresetManager& presetManager;
    
    juce::TextEditor searchField;
    juce::ComboBox   librarySelector;
    juce::ComboBox   categoryFilter;
    juce::ToggleButton favoritesToggle { "★" };
    
    juce::ListBox    presetList;
    
    juce::TextButton saveBtn   { "SAVE" };
    juce::TextButton saveAsBtn { "SAVE AS" };
    
    struct PresetRef {
        int libIdx;
        int presetIdx;
        const ABD::Preset*  preset;
    };
    
    std::vector<PresetRef> filteredItems;
    void updateFilters();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
