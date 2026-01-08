#pragma once

#include <JuceHeader.h>

/**
 * PresetManager - Manages factory and user presets
 * 
 * Features:
 * - Library concepts (Factory, User, Tape)
 * - Authentic Juno-106 Selection Logic:
 *   - Each Library has 128 patches (Group A and Group B)
 *   - Each Group has 8 Banks (1-8)
 *   - Each Bank has 8 Patches (1-8)
 * - Total patches per library: 2 * 8 * 8 = 128
 */
class PresetManager {
public:
    struct Preset {
        juce::String name;
        juce::String category; // "Factory", "User", or Tape name
        juce::ValueTree state;
        
        Preset() = default;
        Preset(const juce::String& n, const juce::String& cat, const juce::ValueTree& s)
            : name(n), category(cat), state(s) {}
    };

    struct Library {
        juce::String name;
        std::vector<Preset> patches; // Flat list of presets
        
        Library() = default;
    };
    
    PresetManager();
    ~PresetManager();
    
    // Library Management
    void addLibrary(const juce::String& name);
    void selectLibrary(int index);
    int getActiveLibraryIndex() const { return currentLibraryIndex; }
    int getNumLibraries() const { return static_cast<int>(libraries.size()); }
    const Library& getLibrary(int index) const { return libraries[juce::jlimit(0, getNumLibraries() - 1, index)]; }
    
    // Tape Loading
    juce::Result loadTape(const juce::File& wavFile);

    // Preset management
    void loadFactoryPresets();
    void loadUserPresets();
    void saveUserPreset(const juce::String& name, const juce::ValueTree& state);
    void deleteUserPreset(const juce::String& name);

    // Phase 6: Sync/Export Features
    void addLibraryFromSysEx(const uint8_t* data, int size);
    void exportLibraryToJson(const juce::File& file);
    
    // Preset access (for current library)
    juce::StringArray getPresetNames() const;
    const Preset* getPreset(int index) const; // Flat index
    
    // Selection Logic (Bank 1-8, Patch 1-8)
    // Juno-106 has Group A/B switch (128 patches total).
    // Bank 1..8 and Patch 1..8 select mapped index.
    // Index = (Group * 64) + ((Bank-1) * 8) + (Patch-1)
    
    void setCurrentPreset(int flatIndex);
    void selectPresetByBankAndPatch(int group, int bank, int patch); 
    juce::ValueTree getCurrentPresetState() const;

    int getCurrentPresetIndex() const { return currentPresetIndex; }
    juce::String getCurrentPresetName() const;
    
    // File paths
    juce::File getUserPresetsDirectory() const;
    
private:
    std::vector<Library> libraries;
    int currentLibraryIndex = 0;
    int currentPresetIndex = 0;
    
    Preset createPresetFromJunoBytes(const juce::String& name, const unsigned char* bytes);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
