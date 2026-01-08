#include "JunoControlSection.h"
#include "../../Core/PluginProcessor.h"
#include "../../Core/PresetManager.h"

JunoControlSection::JunoControlSection(juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& apvts, PresetManager& pm, MidiLearnHandler& mlh)
    : presetBrowser(pm), processor(p)
{
    juce::ignoreUnused(mlh);

    // --- Portamento ---
    portSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    portSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    portSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, 
                                   juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible(portSlider);
    portAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "portamentoTime", portSlider);
    
    JunoUI::setupLabel(portLabel, "PORT", *this);
    portLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(portButton);
    portButton.setButtonText("ON");
    portButtonAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "portamentoOn", portButton);

    // --- Assign Mode ---
    addAndMakeVisible(modeCombo);
    modeCombo.addItem("POLY 1", 1);
    modeCombo.addItem("POLY 2", 2);
    modeCombo.addItem("UNISON", 3);
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "polyMode", modeCombo);
    
    JunoUI::setupLabel(modeLabel, "ASSIGN", *this);
    modeLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(presetBrowser);
    
    // --- MIDI OUT ---
    midiOutButton.setButtonText("MIDI OUT");
    midiOutButton.setClickingTogglesState(true); 
    addAndMakeVisible(midiOutButton);
    midiOutAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "midiOut", midiOutButton);

    // --- TEST Mode ---
    addAndMakeVisible(powerButton); powerButton.setButtonText("TEST");
    powerButton.setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
    powerButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    
    // Functions
    auto setupFunc = [&](juce::TextButton& b, const juce::String& txt) {
        b.setButtonText(txt);
        b.getProperties().set("isFunctionButton", true);
        addAndMakeVisible(b);
    };
    setupFunc(saveButton, "SAVE");
    setupFunc(sysexButton, "SYSEX");
    setupFunc(loadTapeButton, "LOAD TAPE");
    setupFunc(loadButton, "LOAD");
    setupFunc(dumpButton, "EXPORT"); // [reimplement.md] EXPORT Button
    setupFunc(prevPatchButton, "<");
    setupFunc(nextPatchButton, ">");

    // Banks & Patches
    for (int i = 0; i < 8; ++i) {
        bankSelectButtons[i].setButtonText(juce::String(i + 1));
        addAndMakeVisible(bankSelectButtons[i]);
        bankSelectButtons[i].onClick = [this, i] {
             if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
                 auto* pm = proc->getPresetManager();
                 int currentIdx = pm->getCurrentPresetIndex();
                 int activeGroup = currentIdx / 64; 
                 int activePatch = (currentIdx % 8) + 1;
                 pm->selectPresetByBankAndPatch(activeGroup, i + 1, activePatch);
                 int newIndex = pm->getCurrentPresetIndex();
                 presetBrowser.setPresetIndex(newIndex);
                 if (onPresetLoad) onPresetLoad(newIndex);
             }
        };

        bankButtons[i].setButtonText(juce::String(i + 1));
        addAndMakeVisible(bankButtons[i]);
        bankButtons[i].onClick = [this, i] {
            if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
                if (proc->isTestMode) proc->triggerTestProgram(i);
                else {
                    auto* pm = proc->getPresetManager();
                    int currentIdx = pm->getCurrentPresetIndex();
                    int activeGroup = currentIdx / 64;
                    int activeBank = ((currentIdx % 64) / 8) + 1;
                    pm->selectPresetByBankAndPatch(activeGroup, activeBank, i + 1);
                    int newIndex = pm->getCurrentPresetIndex();
                    presetBrowser.setPresetIndex(newIndex);
                    if (onPresetLoad) onPresetLoad(newIndex);
                }
            }
        };
    }

    addAndMakeVisible(decBankButton); decBankButton.setButtonText("< BK");
    addAndMakeVisible(incBankButton); incBankButton.setButtonText("BK >");
    decBankButton.onClick = [this] { 
        if(auto* p=dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) { 
            auto* pm=p->getPresetManager(); 
            pm->selectLibrary(pm->getActiveLibraryIndex()-1); 
            presetBrowser.refreshPresetList(); 
        }
    };
    incBankButton.onClick = [this] { 
        if(auto* p=dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) { 
            auto* pm=p->getPresetManager(); 
            pm->selectLibrary(pm->getActiveLibraryIndex()+1); 
            presetBrowser.refreshPresetList(); 
        }
    };

    addAndMakeVisible(lcd);
    presetBrowser.onPresetChanged = [&](const juce::String&) {
        if(auto* p=dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
             if(onPresetLoad) onPresetLoad(p->getPresetManager()->getCurrentPresetIndex());
        }
    };
    presetBrowser.onGetCurrentState = [&]() { return apvts.copyState(); };

    connectButtons();
    startTimer(50);
}

void JunoControlSection::paint(juce::Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour(JunoUI::kPanelGrey);
    g.fillRect(b);

    auto header = b.removeFromTop(24);
    g.setColour(JunoUI::kStripBlue);
    g.fillRect(header);

    g.setColour(JunoUI::kTextWhite);
    g.setFont(juce::FontOptions("Arial", 12.0f, juce::Font::bold));
    
    g.drawText("PORTAMENTO / ASSIGN", 0, 0, 160, 24, juce::Justification::centred);
    g.drawText("MEMORY / BANKS", getWidth() - 400, 0, 400, 24, juce::Justification::centred);

    g.setFont(10.0f);
    if (bankSelectButtons[0].isVisible()) {
        auto bb = bankSelectButtons[0].getBounds();
        g.drawText("BANK", bb.getX() - 35, bb.getY(), 30, bb.getHeight(), juce::Justification::centredRight);
    }
    if (bankButtons[0].isVisible()) {
        auto pb = bankButtons[0].getBounds();
        g.drawText("PATCH", pb.getX() - 35, pb.getY(), 30, pb.getHeight(), juce::Justification::centredRight);
    }
}

void JunoControlSection::resized()
{
    // [reimplement.md] Fixed coordinates to avoid crowding
    auto b = getLocalBounds();
    int margin = 10;
    
    // Left: Portamento/Assign
    int leftX = margin;
    portLabel.setBounds(leftX, 35, 60, 20);
    portSlider.setBounds(leftX, 55, 60, 60);
    portButton.setBounds(leftX, 120, 60, 25);
    
    int assignX = leftX + 70;
    modeLabel.setBounds(assignX, 35, 70, 20);
    modeCombo.setBounds(assignX, 60, 75, 25);

    // Center: LCD & Browser
    int centerW = 400;
    int centerX = (getWidth() - centerW) / 2;
    lcd.setBounds(centerX, 35, centerW, 50);
    
    int browserY = 105;
    prevPatchButton.setBounds(centerX, browserY, 35, 30);
    nextPatchButton.setBounds(centerX + centerW - 35, browserY, 35, 30);
    presetBrowser.setBounds(centerX + 40, browserY, centerW - 80, 30);

    // Right: Memory/Banks
    int rightX = getWidth() - 410;
    int funcY = 35;
    int funcW = 75;
    saveButton.setBounds(rightX, funcY, funcW, 30);
    sysexButton.setBounds(rightX + 80, funcY, funcW, 30);
    loadTapeButton.setBounds(rightX + 160, funcY, funcW, 30);
    loadButton.setBounds(rightX + 240, funcY, funcW, 30);
    dumpButton.setBounds(rightX + 320, funcY, funcW, 30); // EXPORT
    
    int gridX = rightX + 40;
    int gridY = 75;
    int btnW = 45;
    for(int i=0; i<8; ++i) {
        bankButtons[i].setBounds(gridX + (i * btnW), gridY, btnW - 2, 28);
        bankSelectButtons[i].setBounds(gridX + (i * btnW), gridY + 32, btnW - 2, 28);
    }

    int bottomY = 145;
    decBankButton.setBounds(gridX, bottomY, 40, 25);
    incBankButton.setBounds(gridX + 45, bottomY, 40, 25);
    
    midiOutButton.setBounds(gridX + 120, bottomY, 90, 25);
    panicButton.setBounds(gridX + 220, bottomY, 90, 25);
    powerButton.setBounds(gridX + 320, bottomY, btnW, 25);
}

void JunoControlSection::timerCallback()
{
    auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor);
    if (proc) {
        if (proc->isTestMode) lcd.setText("TEST MODE");
        else {
            auto* pm = proc->getPresetManager();
            int patchIdx = pm->getCurrentPresetIndex();
            int b = (patchIdx / 8) + 1;
            int p = (patchIdx % 8) + 1;
            lcd.setText(juce::String(b) + "-" + juce::String(p) + "  " + pm->getCurrentPresetName());
        }
    }
}

void JunoControlSection::connectButtons()
{
    saveButton.onClick = [this] {
        // [reimplement.md] SAVE: Save only current preset to user folder
        presetBrowser.savePreset();
        lcd.setText("PRESET SAVED");
    };
    
    dumpButton.onClick = [this] {
        // [reimplement.md] EXPORT: Export complete bank
        fileChooser = std::make_unique<juce::FileChooser>("Export Bank JSON...",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.json");
        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                 if (fc.getResult() != juce::File()) {
                     presetBrowser.getPresetManager().exportLibraryToJson(fc.getResult());
                     lcd.setText("BANK EXPORTED");
                 }
            });
    };

    loadButton.onClick = [this] { presetBrowser.loadPreset(); };
    prevPatchButton.onClick = [this] { presetBrowser.prevPreset(); };
    nextPatchButton.onClick = [this] { presetBrowser.nextPreset(); };
    
    sysexButton.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Load SysEx Bank...",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.syx");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) {
                     juce::MemoryBlock mb;
                     file.loadFileAsData(mb);
                     if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
                         proc->getPresetManager()->addLibraryFromSysEx((const uint8_t*)mb.getData(), (int)mb.getSize());
                         presetBrowser.refreshPresetList();
                         lcd.setText("SYSEX LOADED");
                     }
                }
            });
    };

    loadTapeButton.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Load Tape WAV...",
             juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
             [this](const juce::FileChooser& fc) {
                 auto file = fc.getResult();
                 if (file.existsAsFile()) {
                     if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
                         auto res = proc->getPresetManager()->loadTape(file);
                         if (res.wasOk()) {
                             presetBrowser.refreshPresetList();
                             lcd.setText("TAPE LOADED");
                         } else {
                             juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, 
                                "Tape Load Error", res.getErrorMessage());
                         }
                     }
                 }
             });
    };
    
    powerButton.onClick = [this] {
        if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
            proc->enterTestMode(!proc->isTestMode);
            powerButton.setColour(juce::TextButton::textColourOffId, 
                proc->isTestMode ? juce::Colours::red : juce::Colours::black);
        }
    };
    
    addAndMakeVisible(panicButton);
    panicButton.setButtonText("ALL OFF");
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    panicButton.onClick = [this] {
        if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
             proc->getVoiceManagerNC().resetAllVoices();
             lcd.setText("ALL NOTES OFF");
        }
    };
}
