#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../UI/ScaledComponent.h"
#include "../UI/SkinManager.h"

// Sections
#include "../UI/Sections/JunoLFOSection.h"
#include "../UI/Sections/JunoDCOSection.h"
#include "../UI/Sections/JunoHPFSection.h"
#include "../UI/Sections/JunoVCFSection.h"
#include "../UI/Sections/JunoVCASection.h"
#include "../UI/Sections/JunoENVSection.h"
#include "../UI/Sections/JunoChorusSection.h"
#include "../UI/Sections/JunoPerformanceSection.h" 
#include "../UI/Sections/JunoBankSection.h"
#include "../UI/Sections/JunoSysExDisplay.h"
#include "../UI/Components/JunoLCD.h"
#include "../UI/JunoUIHelpers.h"

/**
 * SimpleJuno106AudioProcessorEditor
 * UX/UI Overhaul: "The J106 Experience"
 */
class SimpleJuno106AudioProcessorEditor : public juce::AudioProcessorEditor,
                                           public juce::Timer,
                                           public juce::MenuBarModel,
                                           public juce::AudioProcessorValueTreeState::Listener
{
public:
    SimpleJuno106AudioProcessorEditor(SimpleJuno106AudioProcessor&);
    ~SimpleJuno106AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    
    // Timer callback for polling Processor state (Thread-Safe)
    void timerCallback() override;
    
    // MenuBarModel Implementation
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;
    
    // Menu Actions
    void handleSave();
    void handleLoad();
    void handleImportSysex();
    void handleLoadTape();
    void handleExportBank();
    void handleRandomize();
    void handlePanic();
    void handleAbout();
    
    // APVTS Listener
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    SimpleJuno106AudioProcessor& audioProcessor;
    int localChangeCounter = 0; // For tracking updates
    JunoUI::JunoLookAndFeel lookAndFeel;
    
    // Layout Modules
    juce::MenuBarComponent menuBar;
    JunoBankSection bankSection; // Sidebar
    
    // Header
    JunoUI::JunoLCD lcd;
    JunoSysExDisplay sysExDisplay;
    
    // Synth Rows
    JunoLFOSection lfoSection;
    JunoDCOSection dcoSection;
    JunoHPFSection hpfSection;
    JunoVCFSection vcfSection;
    JunoVCASection vcaSection;
    JunoENVSection envSection;
    JunoChorusSection chorusSection;
    
    // Footer
    JunoPerformanceSection performanceSection;
    juce::MidiKeyboardComponent midiKeyboard;

    // [Medium Fix] Memory Management for Listeners
    juce::OwnedArray<JunoUI::MidiLearnMouseListener> midiLearnListeners;
    
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // Phase 5: LCD Interactive Feedback
    int lcdDisplayTimer = 0;
    juce::String lastPresetName = "--";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleJuno106AudioProcessorEditor)
};
