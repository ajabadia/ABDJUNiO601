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
            if (onPresetChanged) onPresetChanged(juce::String(index));
        }
    };
    refreshPresetList();
    if (presetList.getNumItems() > 0) presetList.setSelectedId(1, juce::dontSendNotification);
}

void PresetBrowser::setPresetIndex(int index) {
    if (index >= 0 && index < presetList.getNumItems())
        presetList.setSelectedItemIndex(index, juce::sendNotification);
}

void PresetBrowser::nextPreset() {
    int current = presetList.getSelectedItemIndex();
    int next = (current + 1) % juce::jmax(1, presetList.getNumItems());
    presetList.setSelectedItemIndex(next, juce::sendNotification);
}

void PresetBrowser::prevPreset() {
    int current = presetList.getSelectedItemIndex();
    int prev = (current - 1 + presetList.getNumItems()) % juce::jmax(1, presetList.getNumItems());
    presetList.setSelectedItemIndex(prev, juce::sendNotification);
}

void PresetBrowser::savePreset() {
    if (!onGetCurrentState) return;
    juce::ValueTree currentState = onGetCurrentState();
    
    auto* w = new juce::AlertWindow("Save Preset", "Enter preset name:", juce::MessageBoxIconType::QuestionIcon);
    w->addTextEditor("name", "My Preset", "Preset Name:");
    w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    
    w->enterModalState(true, juce::ModalCallbackFunction::create([this, currentState, w](int result) {
        if (result == 1) {
            juce::String name = w->getTextEditorContents("name");
            if (name.isNotEmpty()) {
                presetManager.saveUserPreset(name, currentState);
                refreshPresetList();
                
                // Select the newly saved preset
                auto names = presetManager.getPresetNames();
                int idx = names.indexOf(name);
                if (idx >= 0) setPresetIndex(idx);
            }
        }
        delete w;
    }), true);
}

void PresetBrowser::loadPreset() {
    auto fileChooser = std::make_shared<juce::FileChooser>(
        "Load User Preset",
        juce::File(presetManager.getLastPath()),
        "*.json");

    auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
                 
    fileChooser->launchAsync(browserFlags, [this, fileChooser](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
            auto json = juce::JSON::parse(file);
            if (json.isObject()) {
                auto obj = json.getDynamicObject();
                if (obj && obj->hasProperty("state")) {
                     juce::ValueTree state = juce::ValueTree::fromXml(obj->getProperty("state").toString());
                     juce::String name = obj->getProperty("name").toString();
                     presetManager.saveUserPreset(name, state);
                     presetManager.setLastPath(file.getParentDirectory().getFullPathName());
                     refreshPresetList();
                     auto names = presetManager.getPresetNames();
                     int idx = names.indexOf(name);
                     if (idx >= 0) setPresetIndex(idx);
                }
            }
        }
    });
}

void PresetBrowser::paint(juce::Graphics& /*g*/) {}

void PresetBrowser::resized() {
    presetList.setBounds(0, 0, getWidth(), getHeight());
}

void PresetBrowser::refreshPresetList() {
    presetList.clear();
    auto names = presetManager.getPresetNames();
    for(int i=0; i<names.size(); ++i) presetList.addItem(names[i], i+1);
}

PresetBrowser::~PresetBrowser(){}
