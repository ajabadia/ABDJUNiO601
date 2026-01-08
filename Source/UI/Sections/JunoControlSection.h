#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"
#include "../../UI/PresetBrowser.h"
#include "../../UI/JunoBender.h"

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

    void connectButtons(); 
    
    PresetBrowser presetBrowser;
    juce::TextButton dumpButton;
    juce::ToggleButton midiOutButton;
    
    std::function<void(int)> onPresetLoad;
    std::function<void()> onDump;

private:
    juce::TextButton bankSelectButtons[8]; 
    juce::TextButton bankButtons[8];       
    juce::TextButton powerButton, midiButton, tapeButton, loadTapeButton;
    juce::TextButton transposeButton, saveButton, loadButton, sysexButton, prevPatchButton, nextPatchButton;
    juce::TextButton incBankButton, decBankButton;
    juce::TextButton panicButton; 
    
    // [reimplement.md] Random Button
    juce::TextButton randomButton;

    JunoUI::JunoLCD lcd;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiOutAtt;
    
    juce::Slider portSlider;
    juce::ToggleButton portButton;
    juce::Label portLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> portAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> portButtonAtt;

    juce::ComboBox modeCombo;
    juce::Label modeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;

    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvtsRef;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::OwnedArray<JunoUI::MidiLearnMouseListener> midiLearnListeners;
};
