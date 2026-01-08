#pragma once

#include <JuceHeader.h>

/**
 * PresetManager - Manages factory and user presets
 */
class PresetManager {
public:
    struct Preset {
        juce::String name;
        juce::String category; 
        juce::ValueTree state;
        
        Preset() = default;
        Preset(const juce::String& n, const juce::String& cat, const juce::ValueTree& s)
            : name(n), category(cat), state(s) {}
    };

    struct Library {
        juce::String name;
        std::vector<Preset> patches; 
        
        Library() = default;
    };
    
    PresetManager();
    ~PresetManager();
    
    void addLibrary(const juce::String& name);
    void selectLibrary(int index);
    int getActiveLibraryIndex() const { return currentLibraryIndex; }
    int getNumLibraries() const { return static_cast<int>(libraries.size()); }
    const Library& getLibrary(int index) const { return libraries[juce::jlimit(0, getNumLibraries() - 1, index)]; }
    
    juce::Result loadTape(const juce::File& wavFile);

    void loadFactoryPresets();
    void loadUserPresets();
    void saveUserPreset(const juce::String& name, const juce::ValueTree& state);
    void deleteUserPreset(const juce::String& name);

    // [Refactored] Generic File Import
    void addLibraryFromSysEx(const uint8_t* data, int size);
    juce::Result importPresetsFromFile(const juce::File& file);
    
    // [reimplement.md] Export features
    void exportLibraryToJson(const juce::File& file);
    void exportAllLibrariesToJson(const juce::File& file);
    
    juce::StringArray getPresetNames() const;
    const Preset* getPreset(int index) const; 
    
    void setCurrentPreset(int flatIndex);
    void selectPresetByBankAndPatch(int group, int bank, int patch); 
    juce::ValueTree getCurrentPresetState() const;

    int getCurrentPresetIndex() const { return currentPresetIndex; }
    juce::String getCurrentPresetName() const;
    
    juce::File getUserPresetsDirectory() const;
    
    // [reimplement.md] Path persistence
    juce::String getLastPath() const;
    void setLastPath(const juce::String& path);

    void randomizeCurrentParameters(juce::AudioProcessorValueTreeState& apvts);
    
private:
    std::vector<Library> libraries;
    int currentLibraryIndex = 0;
    int currentPresetIndex = 0;
    
    Preset createPresetFromJunoBytes(const juce::String& name, const unsigned char* bytes);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
