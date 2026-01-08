#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../UI/Sections/JunoLFOSection.h"
#include "../UI/Sections/JunoDCOSection.h"
#include "../UI/Sections/JunoHPFSection.h"
#include "../UI/Sections/JunoVCFSection.h"
#include "../UI/Sections/JunoVCASection.h"
#include "../UI/Sections/JunoENVSection.h"
#include "../UI/Sections/JunoChorusSection.h"
#include "../UI/Sections/JunoControlSection.h"
#include "../UI/Sections/JunoPerformanceSection.h"
#include "../UI/JunoUIHelpers.h"

/**
 * SimpleJuno106AudioProcessorEditor
 * 
 * Modular Interface Refactor
 */
class SimpleJuno106AudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    SimpleJuno106AudioProcessorEditor(SimpleJuno106AudioProcessor&);
    ~SimpleJuno106AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SimpleJuno106AudioProcessor& audioProcessor;
    JunoUI::JunoLookAndFeel lookAndFeel;
    
    // Modular Sections
    JunoLFOSection lfoSection;
    JunoDCOSection dcoSection;
    JunoHPFSection hpfSection;
    JunoVCFSection vcfSection;
    JunoVCASection vcaSection;
    JunoENVSection envSection;
    JunoChorusSection chorusSection;
    JunoControlSection controlSection;
    
    // Side Panel (Bender)
    JunoPerformanceSection performanceSection;
    
    // Virtual MIDI Keyboard
    juce::MidiKeyboardComponent midiKeyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleJuno106AudioProcessorEditor)
};
