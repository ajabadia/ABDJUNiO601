#pragma once
#include <JuceHeader.h>
#include <cstdint>
#include <vector>
#include "BaseClass/PresetManagerBase.h"
#include "FactoryPresets.h"

/**
 * JUNiO 601 Implementation of Preset Management.
 */
class PresetManager : public ABD::PresetManagerBase
{
public:
    static constexpr int kMaxPatchesPerLibrary = 64;
    static constexpr int kMaxLibraries = 26;
    static constexpr int kFactoryALibIdx = 0;
    static constexpr int kFactoryBLibIdx = 1;
    static constexpr int kInternalRamLibIdx = 2;

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
    void            setCurrentPreset(int index) noexcept override;

    // Juno Specifics
    // Import Result structure to carry both status and detail message
    struct ImportResult {
        bool success = false;
        juce::String message;
    };

    ImportResult loadTape(const juce::File& wavFile);
    ImportResult importPresetsFromFile(const juce::File& file, bool ignoreSelection = false);
    void addLibraryFromSysEx(const uint8_t* data, int size);
    void randomizeCurrentParameters(juce::AudioProcessorValueTreeState& apvts);
    void triggerMemoryCorruption(juce::AudioProcessorValueTreeState& apvts);
    void exportCurrentPresetToJson(const juce::File& file);
    void exportCurrentPresetToTape(const juce::File& file);
    void exportLibraryToJson(const juce::File& file);
    void exportLibraryToCSV(const juce::File& file);
    
    // Bit-mapping internal helpers (Public for testing)
    ABD::Preset createPresetFromJunoPatch(const struct JunoPatch& p);
    ABD::Preset createPresetFromJunoBytes(const juce::String& name, const unsigned char* bytes);
    
    // [Advanced Browser] API
    int getCurrentLibraryIndex() const noexcept { return this->currentLibIdx_; }
    int getCurrentPresetIndex() const noexcept { return this->currentPresetIdx_; }
    juce::String getCurrentLibraryName() const { return currentLibIdx_ >= 0 && currentLibIdx_ < (int)libraries_.size() ? libraries_[currentLibIdx_].name : ""; }
    const ABD::Preset& getCurrentPreset() const 
    { 
        if (currentLibIdx_ >= 0 && currentLibIdx_ < (int)libraries_.size())
        {
            const auto& patches = libraries_[currentLibIdx_].patches;
            if (currentPresetIdx_ >= 0 && currentPresetIdx_ < (int)patches.size())
                return patches[currentPresetIdx_];
        }
        
        // Static safety fallback to prevent crashes
        static ABD::Preset safetyPreset;
        safetyPreset.name = "RECOVERY PATCH";
        return safetyPreset;
    }
    
    void selectPreset(int libraryIndex, int presetIndex);
    void selectPresetByBankAndPatch(int group, int bank, int patch); 
    
    // Internal Memory (128 slots)
    void loadUserRam();
    void saveUserRam();
    juce::Result writeToInternalSlot(int group, int bank, int patch, 
                                     const juce::ValueTree& state,
                                     const juce::String& name = "",
                                     const juce::String& author = "");
    bool isUserRamActive() const;

    // Navigation
    void nextBank();
    void prevBank();
    void nextPatch();
    void prevPatch();

    // Mode State
    void setWriteArmed(bool b) noexcept { isWriteArmed_ = b; }
    bool isWriteArmed() const noexcept { return isWriteArmed_; }

    // Persistence 
    juce::Result saveCurrentPresetFromState(juce::AudioProcessorValueTreeState& apvts);
    juce::Result saveAsNewPresetFromState(juce::AudioProcessorValueTreeState& apvts, 
                                          const juce::String& newName,
                                          const juce::String& category = {},
                                          const juce::String& author = {},
                                          const juce::String& tags = {},
                                          const juce::String& notes = {});

    void loadBrowserData() override;
    void saveBrowserData() override;

    // [0006.txt] State persistence for PluginProcessor
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& vt);
    
    // [Advanced Browser]
    std::vector<const Preset*> getFilteredPresets(const juce::String& category, 
                                            const juce::String& searchText, 
                                            bool favoritesOnly) const;
    
    void setFavorite(int libIdx, int presetIdx, bool isFav);
    void updateMetadata(int libIdx, int presetIdx, const juce::String& newName, 
                        const juce::String& author, 
                        const juce::String& tags, 
                        const juce::String& notes);

    // Bank & Slot Management Helpers
    int findFirstEmptySlot(int startLib = 2);
    std::vector<int> findEmptyBankIndices(int count);
    juce::String getLibraryLetter(int index);
    void ensureValidLibraryName(int index);

    juce::StringArray categories_;

private:
    void setP(juce::AudioProcessorValueTreeState& apvts, juce::String id, float v);
    void setI(juce::AudioProcessorValueTreeState& apvts, juce::String id, int v);
    void setB(juce::AudioProcessorValueTreeState& apvts, juce::String id, bool v);

    bool isWriteArmed_ = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
