#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"
#include "../../UI/PresetBrowser.h"
#include "../../UI/JunoBender.h"

// Forward declarations
class PresetManager;
class MidiLearnHandler;

class JunoControlSection : public juce::Component, public juce::Timer
{
public:
    JunoControlSection(juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& apvts, PresetManager& pm, MidiLearnHandler& mlh);
    ~JunoControlSection() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void connectButtons(); // declaration only
    
    // Public members for PluginEditor callbacks
    PresetBrowser presetBrowser;
    juce::TextButton dumpButton;
    juce::ToggleButton midiOutButton;
    
    // Callbacks
    std::function<void(int)> onPresetLoad;
    std::function<void()> onDump;

private:
    juce::TextButton bankSelectButtons[8]; // NEW: Bank selection buttons
    juce::TextButton bankButtons[8];       // Patch selection buttons
    juce::TextButton powerButton, midiButton, tapeButton, loadTapeButton;
    juce::TextButton transposeButton, saveButton, loadButton, sysexButton, prevPatchButton, nextPatchButton;
    juce::TextButton incBankButton, decBankButton;
    juce::TextButton panicButton; // New "ALL NOTES OFF" button
    JunoUI::JunoLCD lcd;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiOutAtt;
    
    // NEW Phase 9: Portamento & Assign (Moved from SidePanel)
    juce::Slider portSlider;
    juce::ToggleButton portButton;
    juce::Label portLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> portAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> portButtonAtt;

    juce::ComboBox modeCombo;
    juce::Label modeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;

    juce::AudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::OwnedArray<JunoUI::MidiLearnMouseListener> midiLearnListeners;
};
