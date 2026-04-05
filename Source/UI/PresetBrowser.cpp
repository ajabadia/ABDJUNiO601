#include "PresetBrowser.h"
#include "../Core/PresetManager.h"

PresetBrowser::PresetBrowser(PresetManager& pm) : presetManager(pm)
{
    addAndMakeVisible(searchField);
    searchField.setTextToShowWhenEmpty("Search presets...", juce::Colours::grey);
    searchField.onTextChange = [this] { updateFilters(); };
    
    addAndMakeVisible(librarySelector);
    librarySelector.addItem("All Libraries", 1);
    librarySelector.addItem("Factory", 2);
    librarySelector.addItem("User", 3);
    librarySelector.setSelectedId(1, juce::dontSendNotification);
    librarySelector.onChange = [this] { updateFilters(); };
    
    addAndMakeVisible(categoryFilter);
    categoryFilter.addItem("All Categories", 1);
    // Add known categories from manager
    for(int i=0; i<presetManager.categories_.size(); ++i)
        categoryFilter.addItem(presetManager.categories_[i], i+2);
    categoryFilter.setSelectedId(1, juce::dontSendNotification);
    categoryFilter.onChange = [this] { updateFilters(); };
    
    addAndMakeVisible(favoritesToggle);
    favoritesToggle.onClick = [this] { updateFilters(); };
    
    addAndMakeVisible(presetList);
    presetList.setModel(this);
    presetList.setRowHeight(24);
    
    addAndMakeVisible(saveBtn);
    saveBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    saveBtn.onClick = [this] { if (onSaveClicked) onSaveClicked(); };
    
    addAndMakeVisible(saveAsBtn);
    saveAsBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkblue.withAlpha(0.5f));
    saveAsBtn.onClick = [this] { if (onSaveAsClicked) onSaveAsClicked(); };

    refresh();
}


void PresetBrowser::paint(juce::Graphics& g) 
{
    g.fillAll(juce::Colours::black.withAlpha(0.2f));
}

void PresetBrowser::resized() 
{
    auto area = getLocalBounds().reduced(5);
    auto topArea = area.removeFromTop(30);
    
    searchField.setBounds(topArea.removeFromLeft((int)(topArea.getWidth() * 0.4f)).reduced(2));
    librarySelector.setBounds(topArea.removeFromLeft((int)(topArea.getWidth() * 0.4f)).reduced(2));
    favoritesToggle.setBounds(topArea.reduced(2));
    
    area.removeFromTop(5);
    auto filterArea = area.removeFromTop(30);
    categoryFilter.setBounds(filterArea.reduced(2));
    
    area.removeFromTop(5);
    auto bottomArea = area.removeFromBottom(30);
    saveBtn.setBounds(bottomArea.removeFromLeft(bottomArea.getWidth() / 2).reduced(2));
    saveAsBtn.setBounds(bottomArea.reduced(2));
    
    area.removeFromTop(5);
    presetList.setBounds(area);
}

int PresetBrowser::getNumRows() 
{
    return (int)filteredItems.size();
}

void PresetBrowser::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) 
{
    if (rowNumber < (int)filteredItems.size())
    {
        auto& item = filteredItems[rowNumber];
        
        if (rowIsSelected)
            g.fillAll(juce::Colours::lightblue.withAlpha(0.2f));
            
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(height * 0.7f);
        
        // Star for favorites
        if (item.preset->isFavorite) {
            g.setColour(juce::Colours::yellow.withAlpha(0.8f));
            g.drawText("★", 5, 0, 20, height, juce::Justification::centred);
            g.setColour(juce::Colours::white);
        }
        
        g.drawText(item.preset->name, 25, 0, width - 100, height, juce::Justification::centredLeft, true);
        
        // heritage badge
        if (item.preset->originGroup >= 0) {
            juce::String badge;
            badge << (char)('A' + item.preset->originGroup) << "-" << item.preset->originBank << "-" << item.preset->originPatch;
            g.setColour(juce::Colours::orange.withAlpha(0.6f));
            g.setFont(height * 0.5f);
            g.drawText(badge, width - 75, 0, 70, height, juce::Justification::centredRight, true);
        }
    }
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent&) 
{
    if (row < (int)filteredItems.size()) {
        auto& item = filteredItems[row];
        presetManager.selectPreset(item.libIdx, item.presetIdx);
        if (onPresetSelected) onPresetSelected(item.libIdx, item.presetIdx);
    }
}

void PresetBrowser::refresh() 
{
    updateFilters();
}

void PresetBrowser::updateFilters() 
{
    filteredItems.clear();
    juce::String search = searchField.getText();
    juce::String category = categoryFilter.getText();
    if (category == "All Categories") category = "";
    
    juce::String libName = librarySelector.getText();
    if (libName == "All Libraries") libName = "";
    
    bool favOnly = favoritesToggle.getToggleState();
    
    for (int l = 0; l < (int)presetManager.libraries_.size(); ++l) {
        auto& lib = presetManager.libraries_[l];
        if (libName.isNotEmpty() && lib.name != libName) continue;
        
        for (int p = 0; p < (int)lib.patches.size(); ++p) {
            auto& pr = lib.patches[p];
            
            if (favOnly && !pr.isFavorite) continue;
            if (category.isNotEmpty() && pr.category != category) continue;
            if (search.isNotEmpty() && !pr.name.containsIgnoreCase(search)) continue;
            
            PresetRef ref;
            ref.libIdx = l;
            ref.presetIdx = p;
            ref.preset = &pr;
            filteredItems.push_back(ref);
        }
    }
    
    presetList.updateContent();
    presetList.repaint();
}
PresetManager& PresetBrowser::getPresetManager() { return presetManager; }
