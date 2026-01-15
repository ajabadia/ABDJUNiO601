#pragma once
#include <JuceHeader.h>
#include "../JunoUIHelpers.h"
#include "../../UI/PresetBrowser.h"
#include "../Components/SevenSegmentDisplay.h"

class PresetManager;

/**
 * JunoBankSection (Sidebar)
 * Contains:
 * - Preset Browser (List)
 * - Bank / Group Buttons
 * - Data Buttons (Save ID, MIDI Out, etc.)
 */
class JunoBankSection : public juce::Component
{
public:
    JunoBankSection(PresetManager& pm, juce::AudioProcessorValueTreeState& apvts) 
        : presetBrowser(pm)
    {
        juce::ignoreUnused(apvts);
        // Setup Preset Browser
        addAndMakeVisible(presetBrowser);
        
        // Setup Bank Buttons
        // Setup Bank Buttons
        auto setupBtn = [&](JunoUI::JunoButton& b, const juce::String& txt, int /*group*/ = 0) {
            b.setButtonText(txt);
            // UX: Pulsadores (Momentary) - Logic controls visual state via updateBankLeds
            b.setClickingTogglesState(false); 
            addAndMakeVisible(b);
        };

        for(int i=0; i<8; ++i) {
            setupBtn(bankButtons[i], juce::String(i+1), 100); // Bank 1-8
        }
        
        setupBtn(manualButton, "MANUAL", 0); 
        manualButton.setClickingTogglesState(true); // Toggle behavior
        manualButton.setColour(juce::TextButton::buttonOnColourId, JunoUI::kStripBlue);

        setupBtn(panicButton, "PANIC");
        panicButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        
        setupBtn(powerButton, "TEST");
        powerButton.setClickingTogglesState(true); // Toggle behavior
        powerButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
        powerButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::grey);

        // Navigation Buttons
        setupBtn(decBankButton, "<< BK");
        setupBtn(incBankButton, "BK >>");
        setupBtn(prevPatchButton, "<< PTCH");
        setupBtn(nextPatchButton, "PTCH >>");

        // SysEx removed
        
        // Tape removed
        
        setupBtn(randomButton, "RANDOM");
        randomButton.setColour(juce::TextButton::buttonColourId, JunoUI::kStripBlue);
        randomButton.onClick = [this] { if (onRandom) onRandom(); };
        
        setupBtn(panicButton, "PANIC");
        panicButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        panicButton.onClick = [this] { if (onPanic) onPanic(); };
        
        // MIDI Out moved to Menu
        // Setup Manual Button in Footer (previously MidiOut spot or nearby)
        
        // SysEx Display Integration
        // (Moved to PluginEditor Header)
        
        // [Fidelidad] Authentic 7-Segment Procedural Display
        addAndMakeVisible(bankDigit);
        bankDigit.setColor(juce::Colours::red); // Classic Red LED
        bankDigit.setValue(1);
        
        addAndMakeVisible(patchDigit);
        patchDigit.setColor(juce::Colours::red);
        patchDigit.setValue(1);
    }
    
    // [Fidelidad] Update Procedural Display
    void updateDisplay(int bank, int patch) {
        bankDigit.setValue(bank);
        patchDigit.setValue(patch);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(JunoUI::kPanelGrey.darker(0.2f)); // Darker Sidebar
        g.setColour(juce::Colours::black);
        g.drawVerticalLine(getWidth()-1, 0.0f, (float)getHeight());
        
        // "BANK" Header
        g.setColour(JunoUI::kStripBlue);
        g.fillRect(0, 0, getWidth(), 28);
        g.setColour(JunoUI::kTextWhite);
        g.setColour(JunoUI::kTextWhite);
        g.setFont(juce::FontOptions("Arial", 14.0f, juce::Font::bold));
        g.drawText("BANK / PATCH", 10, 0, getWidth() - 80, 28, juce::Justification::centredLeft);
        
        // Bezel for LED - Positioned consistently
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle((float)getWidth() - 65.0f, 4.0f, 60.0f, 22.0f, 4.0f);
        
        
        // [Fidelidad] Bank LEDs (Audit Fix)
        // Show which bank (1-8) is active based on current preset index
        // ... (Handled by updateBankLeds)
        
        // [Fidelidad] Bank LEDs (Audit Fix)
        // Handled by updateBankLeds() called from PluginEditor::timerCallback
        // to toggle button states visually.
    }
    
    void updateBankLeds(int bankIdx) {
        for(int i=0; i<8; ++i) {
             bankButtons[i].setToggleState(i == bankIdx, juce::dontSendNotification);
             // Force repaint to show state immediately
             bankButtons[i].repaint();
        }
    }
    

    void resized() override
    {
        auto r = getLocalBounds().reduced(5);
        int rowH = 26;
        int gap = 5;

        // 1. Header with Banks/Patch Display
        auto rHeader = r.removeFromTop(30);
        
        // LED Positioning (Inside Bezel) - Compensating for internal slant
        int digitsCenter = getWidth() - 35;
        int pairW = 50; 
        int digitsX = digitsCenter - pairW/2;
        bankDigit.setBounds(digitsX, 6, 24, 18);
        patchDigit.setBounds(digitsX + 26, 6, 24, 18);

        r.removeFromTop(5);

        // 2. PRESET BROWSER (List) - Reduced Height
        presetBrowser.setBounds(r.removeFromTop(35).reduced(2));
        r.removeFromTop(gap);

        // 3. NAVIGATION (BK/PTCH)
        auto rNav = r.removeFromTop(rowH * 2 + gap);
        auto rRow1 = rNav.removeFromTop(rowH);
        decBankButton.setBounds(rRow1.removeFromLeft(rRow1.getWidth()/2).reduced(1));
        incBankButton.setBounds(rRow1.reduced(1));
        
        rNav.removeFromTop(2);
        auto rRow2 = rNav;
        prevPatchButton.setBounds(rRow2.removeFromLeft(rRow2.getWidth()/2).reduced(1));
        nextPatchButton.setBounds(rRow2.reduced(1));
        
        r.removeFromTop(gap);

        // 4. BANK SELECTOR 1-8 (2 Rows of 4)
        auto rBanks = r.removeFromTop(rowH * 2 + 5);
        int btnW = rBanks.getWidth() / 4;
        for (int i = 0; i < 4; ++i) bankButtons[i].setBounds(rBanks.getX() + i * btnW, rBanks.getY(), btnW - 2, rowH);
        for (int i = 4; i < 8; ++i) bankButtons[i].setBounds(rBanks.getX() + (i - 4) * btnW, rBanks.getY() + rowH + 3, btnW - 2, rowH);

        r.removeFromTop(gap);
        r.removeFromTop(gap);
        // manualButton moved to Utilities
        
        r.removeFromTop(gap);

        // 5. UTILITY BUTTONS (SYSEX, TAPE, MIDI TX, etc.)
        auto rUtils = r.removeFromTop(rowH * 3 + gap * 2);
        
        auto uRow1 = rUtils.removeFromTop(rowH);
        // SysEx and Tape removed
        
        rUtils.removeFromTop(gap);
        auto uRow2 = rUtils.removeFromTop(rowH);
        manualButton.setBounds(uRow2.removeFromLeft(uRow2.getWidth()/2).reduced(1)); // Moved Manual here
        randomButton.setBounds(uRow2.reduced(1));
        
        rUtils.removeFromTop(gap);
        auto uRow3 = rUtils;
        powerButton.setBounds(uRow3.removeFromLeft(uRow3.getWidth()/2).reduced(1));
        panicButton.setBounds(uRow3.reduced(1));

        // Pre-calculation for buttons done
    }
    
    // Public Listeners for connecting to Logic
    std::function<void()> onSave, onLoad, onDump, onSysEx, onTape, onRandom, onPanic, onPower;
    std::function<void(int)> onBankSelect; // 1-8
    std::function<void()> onManual;
    
    // New Nav Listeners
    // std::function<void(bool)> onBankNav; // Old one
    // We can just expose the buttons publicly or add specific listeners
    
    PresetBrowser presetBrowser; // Added missing member

    // ... buttons ...
    // [Refactor] Using Atomic Components - Preserving original names where possible or updating impl
    // It seems the original file had `bankBtns`, `manualBtn` etc.
    // BUT my previous big replace replaced a block that had `decBankButton`, `incBankButton` etc.
    // Let's look at the FILE CONTENT returned in step 1372 if possible? 
    // Wait, step 1372 failed.
    
    // Let's assume the names I just wrote: `decBankButton`, `incBankButton` etc are the ones I WANT.
    // I need to ensure the implementation uses them.
    // Check if JunoBankSection.cpp exists.
    
    JunoUI::JunoButton decBankButton { "< BK" };
    JunoUI::JunoButton incBankButton { "BK >" };
    
    JunoUI::JunoButton groupAButton { "GROUP A" };
    JunoUI::JunoButton groupBButton { "GROUP B" };

    JunoUI::JunoButton bankSelectButtons[8] = { {"1"}, {"2"}, {"3"}, {"4"}, {"5"}, {"6"}, {"7"}, {"8"} };
    JunoUI::JunoButton bankButtons[8]       = { {"1"}, {"2"}, {"3"}, {"4"}, {"5"}, {"6"}, {"7"}, {"8"} };
    
    JunoUI::JunoButton saveButton { "SAVE" };
    // Removed SysEx, Tape, Load, Dump duplicates
    
    JunoUI::JunoButton loadButton { "LOAD" };
    JunoUI::JunoButton dumpButton { "EXPORT" };
    JunoUI::JunoButton randomButton { "RANDOM" };
    JunoUI::JunoButton manualButton { "MANUAL" };
    JunoUI::JunoButton panicButton { "ALL OFF" };
    JunoUI::JunoButton prevPatchButton { "<" };
    JunoUI::JunoButton nextPatchButton { ">" };
    
    JunoUI::JunoButton portButton { "ON" }; 
    // Removed midiOut button
    JunoUI::JunoButton powerButton { "TEST" };
    
    JunoUI::JunoKnob portSlider;
    JunoUI::JunoKnob masterTuneKnob;
    
    // Removed masterSlider from here if it wasn't here.
    
    // midiOutAtt removed
    
    // Procedural LCDs
    SevenSegmentDisplay bankDigit;
    SevenSegmentDisplay patchDigit;
};
