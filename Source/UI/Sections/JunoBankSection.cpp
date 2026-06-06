#include "JunoBankSection.h"
#include "../../Core/PresetManager.h"
#include "../../Core/BaseClass/PresetManagerBase.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"

JunoBankSection::JunoBankSection(PresetManager& pm, juce::AudioProcessorValueTreeState& apvts) 
    : presetBrowser(pm)
{
    juce::ignoreUnused(apvts);
    // Setup Preset Browser
    addAndMakeVisible(presetBrowser);
    
    // Setup Components
    addAndMakeVisible(bankDigit);
    addAndMakeVisible(patchDigit1);
    addAndMakeVisible(patchDigit2);
    addAndMakeVisible(midiActivityLed);

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

    // Browser toggle
    setupBtn(browserToggle, "BROWSER");
    browserToggle.setClickingTogglesState(true);
    browserToggle.setToggleState(true, juce::dontSendNotification);
    browserToggle.setColour(juce::TextButton::buttonOnColourId, JunoUI::kStripBlue);
    browserToggle.onClick = [this] {
        presetBrowser.setVisible(browserToggle.getToggleState());
    };
}

JunoBankSection::~JunoBankSection() {}

void JunoBankSection::updateDisplay(char bank, char patch1, char patch2) {
    bankDigit.setCharacter(bank);
    patchDigit1.setCharacter(patch1);
    patchDigit2.setCharacter(patch2);
}

void JunoBankSection::setMidiActivity(bool active) {
    if (midiActivityLed.active != active) {
        midiActivityLed.active = active;
        midiActivityLed.repaint();
    }
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
    
    // 1. LCD Area (7x3 Display)
    auto rLcd = r.removeFromTop(32);
    int digitsCenter = rLcd.getCentreX();
    int digitW = 28;
    int digitH = 22;
    int totalW = (digitW * 3) + 4; // 3 digits + gaps
    int x = digitsCenter - totalW/2;
    int y = rLcd.getY() + 4;

    bankDigit.setBounds(x, y, digitW, digitH);
    patchDigit1.setBounds(x + digitW + 2, y, digitW, digitH);
    patchDigit2.setBounds(x + (digitW + 2) * 2, y, digitW, digitH);

    // MIDI LED placement (top right of LCD area)
    midiActivityLed.setBounds(rLcd.getRight() - 15, rLcd.getY() + 8, 8, 8);

    r.removeFromTop(5);

    // 2. BROWSER TOGGLE
    auto rToggle = r.removeFromTop(rowH);
    browserToggle.setBounds(rToggle.reduced(2));
    r.removeFromTop(gap);

    // 3. PRESET BROWSER (List)
    presetBrowser.setBounds(r.removeFromTop(140).reduced(2));
    r.removeFromTop(gap);

    // 4. NAVIGATION (BK/PTCH)
    auto rNav = r.removeFromTop(rowH * 2 + gap);
    auto rRow1 = rNav.removeFromTop(rowH);
    decBankButton.setBounds(rRow1.removeFromLeft(rRow1.getWidth()/2).reduced(1));
    incBankButton.setBounds(rRow1.reduced(1));
    
    rNav.removeFromTop(2);
    auto rRow2 = rNav;
    prevPatchButton.setBounds(rRow2.removeFromLeft(rRow2.getWidth()/2).reduced(1));
    nextPatchButton.setBounds(rRow2.reduced(1));
    
    r.removeFromTop(gap);

    // 5. BANK SELECTOR 1-8
    auto rBanks = r.removeFromTop(rowH * 2 + 5);
    int btnW = rBanks.getWidth() / 4;
    for (int i = 0; i < 4; ++i) bankButtons[i].setBounds(rBanks.getX() + i * btnW, rBanks.getY(), btnW - 2, rowH);
    for (int i = 4; i < 8; ++i) bankButtons[i].setBounds(rBanks.getX() + (i - 4) * btnW, rBanks.getY() + rowH + 3, btnW - 2, rowH);

    r.removeFromTop(gap);

    // 6. UTILITY BUTTONS
    auto rUtils = r.removeFromTop(rowH * 3 + gap * 2);
    auto uRow2 = rUtils.removeFromTop(rowH);
    manualButton.setBounds(uRow2.removeFromLeft(uRow2.getWidth()/2).reduced(1));
    randomButton.setBounds(uRow2.reduced(1));
    
    rUtils.removeFromTop(gap);
    auto uRow3 = rUtils;
    powerButton.setBounds(uRow3.removeFromLeft(uRow3.getWidth()/2).reduced(1));
    panicButton.setBounds(uRow3.reduced(1));
}

