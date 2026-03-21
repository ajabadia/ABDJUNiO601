#pragma once

#include "../ABD-SynthEngine/Protocol/Presets/PresetManagerBase.h"
#include "FactoryPresets.h"

/**
 * PresetManager - Manages factory and user presets.
 * Inherits from ABD::PresetManagerBase for modular architecture.
 */
class PresetManager : public ABD::PresetManagerBase
{
public:
    
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
    
    void selectPresetByBankAndPatch(int group, int bank, int patch); 
    
    // [reimplement.md] Path persistence / Tools
    juce::String getLastPath() const;
    void setLastPath(const juce::String& path);
    void randomizeCurrentParameters(juce::AudioProcessorValueTreeState& apvts);
    void triggerMemoryCorruption(juce::AudioProcessorValueTreeState& apvts); 
    void exportCurrentPresetToTape(const juce::File& file);

private:
    Preset createPresetFromJunoPatch(const struct JunoPatch& p);
    Preset createPresetFromJunoBytes(const juce::String& name, const unsigned char* bytes);

    void setP(juce::AudioProcessorValueTreeState& apvts, juce::String id, float v);
    void setI(juce::AudioProcessorValueTreeState& apvts, juce::String id, int v);
    void setB(juce::AudioProcessorValueTreeState& apvts, juce::String id, bool v);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
