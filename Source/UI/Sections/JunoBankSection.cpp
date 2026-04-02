#include "JunoBankSection.h"
#include "../../Core/PresetManager.h"
#include "../../ABD-SynthEngine/Protocol/Presets/PresetManagerBase.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"

JunoBankSection::JunoBankSection(PresetManager& pm, juce::AudioProcessorValueTreeState& apvts) 
    : presetBrowser(pm)
{
    juce::ignoreUnused(apvts);
    // Setup Preset Browser
    addAndMakeVisible(presetBrowser);
    
    // Setup Bank Buttons
    auto setupBtn = [&](JunoUI::JunoButton& b, const juce::String& txt) {
        b.setButtonText(txt);
        b.setClickingTogglesState(false); 
        addAndMakeVisible(b);
    };

    for(int i=0; i<8; ++i) {
        setupBtn(bankButtons[i], juce::String(i+1));
        setupBtn(bankSelectButtons[i], juce::String(i+1));
    }
    
    setupBtn(manualButton, "MANUAL"); 
    manualButton.setClickingTogglesState(true);
    manualButton.setColour(juce::TextButton::buttonOnColourId, JunoUI::kStripBlue);

    setupBtn(panicButton, "ALL OFF");
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    
    setupBtn(powerButton, "TEST");
    powerButton.setClickingTogglesState(true);
    powerButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    powerButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::grey);

    setupBtn(decBankButton, "<< BK");
    setupBtn(incBankButton, "BK >>");
    setupBtn(prevPatchButton, "<<");
    setupBtn(nextPatchButton, ">>");
    
    setupBtn(saveButton, "SAVE");
    setupBtn(loadButton, "LOAD");
    setupBtn(dumpButton, "EXPORT");
    setupBtn(randomButton, "RANDOM");
    setupBtn(portButton, "ON");
}

JunoBankSection::~JunoBankSection() {}

void JunoBankSection::updateDisplay(int bank, int patch) {
    bankDigit.setValue(bank);
    patchDigit.setValue(patch);
}

void JunoBankSection::updateBankLeds(int bankIdx) {
    for(int i=0; i<8; ++i) {
         bankButtons[i].setToggleState(i == bankIdx, juce::dontSendNotification);
         bankButtons[i].repaint();
    }
}

void JunoBankSection::paint(juce::Graphics& g) {
    auto b = getLocalBounds();
    JunoUI::drawJunoSectionPanel(g, b, "BROWSER / BANK");
}

void JunoBankSection::resized() {
    auto r = getLocalBounds();
    r.removeFromTop(32); // Header
    
    int gap = 4;
    int rowH = 26;
    
    // 1. LCD Area (Bank/Patch display)
    auto rLcd = r.removeFromTop(30);
    int digitsCenter = rLcd.getCentreX();
    int pairW = 54;
    int digitsX = digitsCenter - pairW/2;
    bankDigit.setBounds(digitsX, rLcd.getY() + 6, 24, 18);
    patchDigit.setBounds(digitsX + 26, rLcd.getY() + 6, 24, 18);

    r.removeFromTop(5);

    // 2. PRESET BROWSER (List)
    presetBrowser.setBounds(r.removeFromTop(140).reduced(2));
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

    // 4. BANK SELECTOR 1-8
    auto rBanks = r.removeFromTop(rowH * 2 + 5);
    int btnW = rBanks.getWidth() / 4;
    for (int i = 0; i < 4; ++i) bankButtons[i].setBounds(rBanks.getX() + i * btnW, rBanks.getY(), btnW - 2, rowH);
    for (int i = 4; i < 8; ++i) bankButtons[i].setBounds(rBanks.getX() + (i - 4) * btnW, rBanks.getY() + rowH + 3, btnW - 2, rowH);

    r.removeFromTop(gap);

    // 5. UTILITY BUTTONS
    auto rUtils = r.removeFromTop(rowH * 3 + gap * 2);
    auto uRow2 = rUtils.removeFromTop(rowH);
    manualButton.setBounds(uRow2.removeFromLeft(uRow2.getWidth()/2).reduced(1));
    randomButton.setBounds(uRow2.reduced(1));
    
    rUtils.removeFromTop(gap);
    auto uRow3 = rUtils;
    powerButton.setBounds(uRow3.removeFromLeft(uRow3.getWidth()/2).reduced(1));
    panicButton.setBounds(uRow3.reduced(1));
}
