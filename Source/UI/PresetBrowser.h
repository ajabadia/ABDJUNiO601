#pragma once

#include <JuceHeader.h>
#include "../Core/BaseClass/PresetManagerBase.h"
#include "PresetBrowserHeaderBar.h"
class PresetManager;

class PresetBrowser : public juce::Component,
                      public juce::ListBoxModel,
                      private juce::KeyListener
{
public:
    PresetBrowser(PresetManager& pm);
    ~PresetBrowser() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Keyboard (Component-level)
    bool keyPressed (const juce::KeyPress& key) override;
    // Keyboard (KeyListener — intercepts keys from presetList when it has focus)
    bool keyPressed (const juce::KeyPress& key, juce::Component*) override { return keyPressed(key); }
    
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g,
                           int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;
    
    void refresh();
    void setPresetIndex(int /*idx*/) { refresh(); }
    
    PresetManager& getPresetManager();
    
    // Callbacks
    std::function<void(int libraryIndex, int presetIndex)> onPresetSelected;
    std::function<void()> onSaveClicked;
    std::function<void()> onSaveAsClicked;
    std::function<void()> onCloseRequested;

    // Column header IDs for click-to-sort (mirrors PresetBrowserHeaderBar for backward compat)
    enum SortColumn { SortByName, SortByCategory, SortByLibrary, SortByFavorite };

    // Column width constants (for backward compat with tests)
    static constexpr int kColFavW     = 24;
    static constexpr int kColCatWMin  = 80;
    static constexpr int kColLibWMin  = 70;
    static constexpr int kColGapAdj   = 8;
    static constexpr int kColLeftMarg = 4;
    static constexpr int kColGap      = 2;

    // Layout constants
    static constexpr int kOuterMargin   = 5;   // Outer margin around entire component
    static constexpr int kRowH          = 30;  // Height of filter/button rows
    static constexpr int kInnerPad      = 2;   // Inner padding for child widgets within rows
    static constexpr int kSectionGap    = 5;   // Vertical gap between layout sections
    static constexpr int kHeaderBarH    = 22;  // Column header row height
    static constexpr int kHeaderListGap = 2;   // Gap between header bar and preset list

    // Column proportion divisor (contentW / kColProportionDivisor = catW/libW)
    static constexpr int kColProportionDivisor = 5;

    // List row height (presetList rows)
    static constexpr int kListRowH = 24;

    // Font scales relative to row height
    static constexpr float kNameFontScale     = 0.65f;  // Preset name font
    static constexpr float kDetailFontScale   = 0.55f;  // Category font
    static constexpr float kSmallFontScale    = 0.50f;  // Library font
    static constexpr float kStarFontScale     = 0.65f;  // Favorite star font

    // Visual constants (alpha/color)
    static constexpr float kSelectedBgAlpha  = 0.25f;  // Selection highlight background
    static constexpr float kNameTextAlpha    = 0.85f;  // Preset name text opacity
    static constexpr float kDetailTextAlpha  = 0.50f;  // Category text opacity
    static constexpr float kLibTextAlpha     = 0.50f;  // Library name text opacity
    static constexpr float kBgAlpha          = 0.20f;  // Default component background

    // Layout ratios
    static constexpr float kFieldWidthRatio  = 0.40f;  // Proportional width for search/library fields

private:
    void loadPresetAt(int filteredIndex);
    
    PresetManager& presetManager;
    
    PresetBrowserHeaderBar headerBar;
    
    juce::TextEditor searchField;
    juce::ComboBox   librarySelector;
    juce::ComboBox   categoryFilter;
    juce::ToggleButton favoritesToggle { "★" };
    
    juce::ListBox    presetList;
    
    juce::TextButton saveBtn   { "SAVE" };
    juce::TextButton saveAsBtn { "SAVE AS" };
    
    // Sort state
    SortColumn activeSortCol = SortByName;
    bool       sortAscending  = true;

    // Last list content width used to detect scrollbar visibility changes
    int lastContentWidth = 0;
    
    struct PresetRef {
        int libIdx;
        int presetIdx;
        const ABD::Preset*  preset;
    };
    
    std::vector<PresetRef> filteredItems;
    void updateFilters();
    int  getSelectedFilteredIndex() const;
    void setSelectedFilteredIndex(int idx);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
