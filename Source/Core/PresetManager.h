#pragma once

#include "../ABD-SynthEngine/Protocol/Presets/PresetManagerBase.h"
#include "FactoryPresets.h"

/**
 * JUNiO 601 Implementation of Preset Management.
 * Restored to Global Namespace for project-wide compatibility.
 */
class PresetManager : public ABD::PresetManagerBase
{
public:
    using Preset = ABD::Preset;
    using Library = ABD::Library;
    
    PresetManager();
    ~PresetManager() override;
    
    // ABD::PresetManagerBase Overrides
    juce::String    getSynthType()   const override { return "Juno106"; }
    void            loadFactoryPresets()   override;
    juce::ValueTree bytesToState(const uint8_t* data, int size) const override;
    std::vector<uint8_t> stateToBytes(const juce::ValueTree& state) const override;
    juce::File      getUserPresetsDirectory() const override;

    // Juno Specifics
    juce::Result loadTape(const juce::File& wavFile);
    void addLibraryFromSysEx(const uint8_t* data, int size);
    juce::Result importPresetsFromFile(const juce::File& file);
    void randomizeCurrentParameters(juce::AudioProcessorValueTreeState& apvts);
    void triggerMemoryCorruption(juce::AudioProcessorValueTreeState& apvts);
    void exportCurrentPresetToJson(const juce::File& file);
    void exportCurrentPresetToTape(const juce::File& file);
    void exportLibraryToJson(const juce::File& file);
    
    // [Advanced Browser] API
    int getCurrentLibraryIndex() const noexcept { return currentLibIdx_; }
    int getCurrentPresetIndex() const noexcept { return currentPresetIdx_; }
    juce::String getCurrentLibraryName() const { return currentLibIdx_ >= 0 && currentLibIdx_ < (int)libraries_.size() ? libraries_[currentLibIdx_].name : ""; }
    const ABD::Preset& getCurrentPreset() const { return libraries_[currentLibIdx_].patches[currentPresetIdx_]; }
    
    void selectPreset(int libraryIndex, int presetIndex);
    void selectPresetByBankAndPatch(int group, int bank, int patch); 

    // Navigation
    void nextBank();
    void prevBank();
    void nextPatch();
    void prevPatch();

    // Persistence 
    juce::Result saveCurrentPresetFromState(juce::AudioProcessorValueTreeState& apvts);
    juce::Result saveAsNewPresetFromState(juce::AudioProcessorValueTreeState& apvts, 
                                          const juce::String& newName,
                                          const juce::String& category = {},
                                          const juce::String& author = {},
                                          const juce::String& tags = {},
                                          const juce::String& notes = {});

    // [0006.txt] State persistence for PluginProcessor
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& vt);
    
    // Persistence to disk
    void saveBrowserData();
    void loadBrowserData();

    // [Advanced Browser]
    std::vector<const Preset*> getFilteredPresets(const juce::String& category, 
                                            const juce::String& searchText, 
                                            bool favoritesOnly) const;
    
    void setFavorite(int libIdx, int presetIdx, bool isFav);
    void updateMetadata(int libIdx, int presetIdx, const juce::String& newName, 
                        const juce::String& author, 
                        const juce::String& tags, 
                        const juce::String& notes);

    juce::StringArray categories_;
private:
    ABD::Preset createPresetFromJunoPatch(const struct JunoPatch& p);
    ABD::Preset createPresetFromJunoBytes(const juce::String& name, const unsigned char* bytes);

    void setP(juce::AudioProcessorValueTreeState& apvts, juce::String id, float v);
    void setI(juce::AudioProcessorValueTreeState& apvts, juce::String id, int v);
    void setB(juce::AudioProcessorValueTreeState& apvts, juce::String id, bool v);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
