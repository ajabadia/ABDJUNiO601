#include "PresetBrowser.h"
#include "JunoUIHelpers.h"

PresetBrowser::PresetBrowser(PresetManager& pm) : presetManager(pm)
{
    addAndMakeVisible(presetList);
    presetList.setTextWhenNothingSelected("Select Preset...");
    presetList.onChange = [this] {
        int id = presetList.getSelectedId();
        if (id > 0) {
            int index = id - 1;
            presetManager.setCurrentPreset(index);
            
            // Call the callback to notify ControlSection which will call audioProcessor.loadPreset()
            if (onPresetChanged) {
                onPresetChanged(juce::String(index));
            }
        }
    };

    refreshPresetList();
    
    // Select first preset by default
    if (presetList.getNumItems() > 0) {
        presetList.setSelectedId(1, juce::dontSendNotification);
    }
}

void PresetBrowser::setPresetIndex(int index) {
    if (index >= 0 && index < presetList.getNumItems())
        presetList.setSelectedItemIndex(index, juce::sendNotification);
}

void PresetBrowser::nextPreset() {
    int current = presetList.getSelectedItemIndex();
    if (current < 0) current = -1; // Safe start
    int next = current + 1;
    if (next < presetList.getNumItems())
        presetList.setSelectedItemIndex(next, juce::sendNotification);
    else if (presetList.getNumItems() > 0)
        presetList.setSelectedItemIndex(0, juce::sendNotification); // Wrap around
}

void PresetBrowser::prevPreset() {
    int current = presetList.getSelectedItemIndex();
    if (current < 0) current = 1; // Safe start
    int prev = current - 1;
    if (prev >= 0)
        presetList.setSelectedItemIndex(prev, juce::sendNotification);
    else if (presetList.getNumItems() > 0)
        presetList.setSelectedItemIndex(presetList.getNumItems() - 1, juce::sendNotification); // Wrap around
}

void PresetBrowser::savePreset() {
    // Get current state from callback
    if (!onGetCurrentState) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, 
            "Error", "Cannot save: no state callback configured.");
        return;
    }
    
    juce::ValueTree currentState = onGetCurrentState();
    
    // Ask for preset name using async modal
    auto* w = new juce::AlertWindow("Save Preset", "Enter preset name:", juce::MessageBoxIconType::QuestionIcon);
    w->addTextEditor("name", "My Preset", "Preset Name:");
    w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    
    w->enterModalState(true, juce::ModalCallbackFunction::create([this, currentState, w](int result) {
        if (result == 1) {
            juce::String name = w->getTextEditorContents("name");
            if (name.isNotEmpty()) {
                presetManager.saveUserPreset(name, currentState);
                refreshPresetList(); // Refresh list to show new preset
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, 
                    "Success", "Preset '" + name + "' saved successfully!");
            }
        }
        delete w;
    }), true);
}

void PresetBrowser::loadPreset() {
    // Open file chooser for User Preset (.json)
    auto fileChooser = std::make_shared<juce::FileChooser>(
        "Load User Preset",
        presetManager.getUserPresetsDirectory(),
        "*.json");

    auto flags = juce::FileBrowserComponent::openMode | 
                 juce::FileBrowserComponent::canSelectFiles;
                 
    fileChooser->launchAsync(flags, [this, fileChooser](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
            // Parse JSON manually here or add a method to PresetManager?
            // Let's reload all user presets to ensure it's in the list, then select it.
            // But to be safe, let's just parse it and apply it.
            
            auto json = juce::JSON::parse(file);
            if (json.isObject()) {
                auto obj = json.getDynamicObject();
                if (obj && obj->hasProperty("state")) {
                     juce::ValueTree state = juce::ValueTree::fromXml(obj->getProperty("state").toString());
                     
                     // We need to apply this state.
                     // The best way is to add it to the manager temporarily or just "load" it.
                     // But our callbacks are ID based. 
                     // Let's just create a temporary preset and apply it via parameters directly?
                     // No, better to add it to User Bank and select it.
                     
                     juce::String name = obj->getProperty("name").toString();
                     presetManager.saveUserPreset(name, state); // Ensure it's in the system
                     
                     // FIX: Switch to User Data Library automatically
                     for (int i=0; i < presetManager.getNumLibraries(); ++i) {
                         if (presetManager.getLibrary(i).name == "User") {
                             presetManager.selectLibrary(i);
                             break;
                         }
                     }
                     
                     refreshPresetList();
                     
                     // Find index
                     auto names = presetManager.getPresetNames();
                     int idx = names.indexOf(name);
                     if (idx >= 0) {
                         setPresetIndex(idx);
                         juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, 
                            "Loaded", "Preset '" + name + "' loaded and added to User Bank.");
                     }
                }
            }
        }
    });
}

void PresetBrowser::paint(juce::Graphics& g) {
    // Solo texto, fondo transparente (lo pone el padre)
    // Removed "PRESET SELECT" text as requested
}

void PresetBrowser::resized() {
    presetList.setBounds(0, 0, getWidth(), getHeight());
}

void PresetBrowser::refreshPresetList() {
    presetList.clear();
    auto names = presetManager.getPresetNames();
    for(int i=0; i<names.size(); ++i) presetList.addItem(names[i], i+1);
}

// Destructor y demás omitidos por brevedad, pero necesarios en .cpp completo.
PresetBrowser::~PresetBrowser(){}
