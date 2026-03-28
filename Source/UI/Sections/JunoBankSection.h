#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"
#include "../../UI/PresetBrowser.h"
#include "../Components/SevenSegmentDisplay.h"

class PresetManager;

/**
 * JunoBankSection (Sidebar)
 */
class JunoBankSection : public juce::Component
{
public:
    JunoBankSection(PresetManager& pm, juce::AudioProcessorValueTreeState& apvts);
    ~JunoBankSection() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void updateDisplay(int bank, int patch);
    void updateBankLeds(int bankIdx);

    // Public Listeners
    std::function<void()> onSave, onLoad, onDump, onSysEx, onTape, onRandom, onPanic, onPower;
    std::function<void(int)> onBankSelect;
    std::function<void()> onManual;
    
    PresetBrowser presetBrowser;

    JunoUI::JunoButton decBankButton { "<< BK" };
    JunoUI::JunoButton incBankButton { "BK >>" };
    JunoUI::JunoButton groupAButton { "GROUP A" };
    JunoUI::JunoButton groupBButton { "GROUP B" };

    JunoUI::JunoButton bankSelectButtons[8]; 
    JunoUI::JunoButton bankButtons[8];       
    
    JunoUI::JunoButton saveButton { "SAVE" };
    JunoUI::JunoButton loadButton { "LOAD" };
    JunoUI::JunoButton dumpButton { "EXPORT" };
    JunoUI::JunoButton randomButton { "RANDOM" };
    JunoUI::JunoButton manualButton { "MANUAL" };
    JunoUI::JunoButton panicButton { "ALL OFF" };
    JunoUI::JunoButton prevPatchButton { "<" };
    JunoUI::JunoButton nextPatchButton { ">" };
    JunoUI::JunoButton portButton { "ON" }; 
    JunoUI::JunoButton powerButton { "TEST" };
    
    JunoUI::JunoKnob portSlider;
    JunoUI::JunoKnob masterTuneKnob;
    
    SevenSegmentDisplay bankDigit;
    SevenSegmentDisplay patchDigit;
};
