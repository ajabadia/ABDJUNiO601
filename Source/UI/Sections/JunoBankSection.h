#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"
#include "../../UI/PresetBrowser.h"
#include "../Components/JunoLEDDigit.h"

class PresetManager;

/**
 * [Aesthetic Upgrade] JunoBankSection (Sidebar)
 * Now features 7x3 Display (Bank A-Z, Patch 11-88) and MIDI Activity LED.
 */
class JunoBankSection : public juce::Component
{
public:
    JunoBankSection(PresetManager& pm, juce::AudioProcessorValueTreeState& apvts);
    ~JunoBankSection() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    /**
     * Updates the 3-digit display.
     * @param bank Alphanumeric bank (A-Z)
     * @param patch1 First patch digit (1-8)
     * @param patch2 Second patch digit (1-8)
     */
    void updateDisplay(char bank, char patch1, char patch2);
    
    /** Sets the MIDI activity LED state */
    void setMidiActivity(bool active);

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
    JunoUI::JunoButton browserToggle { "BROWSER" };
    
    JunoUI::JunoKnob portSlider;
    JunoUI::JunoKnob masterTuneKnob;
    
    JunoLEDDigit bankDigit;
    JunoLEDDigit patchDigit1;
    JunoLEDDigit patchDigit2;

    struct MidiLed : public juce::Component {
        bool active = false;
        void paint(juce::Graphics& g) override {
            auto b = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(active ? juce::Colour(0xff00ff00) : juce::Colours::darkgrey.darker());
            g.fillEllipse(b);
            if (active) {
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                g.fillEllipse(b.reduced(b.getWidth() * 0.3f));
            }
        }
    } midiActivityLed;
};

