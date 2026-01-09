#include "JunoControlSection.h"
#include "../../Core/PluginProcessor.h"
#include "../../Core/PresetManager.h"

JunoControlSection::JunoControlSection(juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& apvts, PresetManager& pm, MidiLearnHandler& mlh)
    : presetBrowser(pm), processor(p), apvtsRef(apvts)
{
    juce::ignoreUnused(mlh);

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

    addAndMakeVisible(modeCombo);
    modeCombo.addItem("POLY 1", 1);
    modeCombo.addItem("POLY 2", 2);
    modeCombo.addItem("UNISON", 3);
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "polyMode", modeCombo);
    
    JunoUI::setupLabel(modeLabel, "ASSIGN", *this);
    modeLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(presetBrowser);
    
    midiOutButton.setButtonText("MIDI OUT");
    midiOutButton.setClickingTogglesState(true); 
    addAndMakeVisible(midiOutButton);
    midiOutAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "midiOut", midiOutButton);

    addAndMakeVisible(powerButton); powerButton.setButtonText("TEST");
    powerButton.setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
    powerButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    
    auto setupFunc = [&](juce::TextButton& b, const juce::String& txt) {
        b.setButtonText(txt);
        b.getProperties().set("isFunctionButton", true);
        addAndMakeVisible(b);
    };
    setupFunc(saveButton, "SAVE");
    setupFunc(sysexButton, "SYSEX");
    setupFunc(loadTapeButton, "LOAD TAPE");
    setupFunc(loadButton, "LOAD");
    setupFunc(dumpButton, "EXPORT");
    setupFunc(prevPatchButton, "<");
    setupFunc(nextPatchButton, ">");
    setupFunc(randomButton, "RANDOM");
    randomButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff60a8d6));
    randomButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    setupFunc(manualButton, "MANUAL");
    manualButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
    
    setupFunc(groupAButton, "GROUP A");
    setupFunc(groupBButton, "GROUP B");
    groupAButton.setClickingTogglesState(true);
    groupBButton.setClickingTogglesState(true);
    groupAButton.setRadioGroupId(202);
    groupBButton.setRadioGroupId(202);

    for (int i = 0; i < 8; ++i) {
        bankSelectButtons[i].setButtonText(juce::String(i + 1));
        addAndMakeVisible(bankSelectButtons[i]);
        bankSelectButtons[i].onClick = [this, i] {
             auto& pmRef = presetBrowser.getPresetManager();
             pmRef.selectPresetByBankAndPatch(activeGroup, i + 1, (pmRef.getCurrentPresetIndex() % 8) + 1);
             presetBrowser.setPresetIndex(pmRef.getCurrentPresetIndex());
             if (onPresetLoad) onPresetLoad(pmRef.getCurrentPresetIndex());
        };

        bankButtons[i].setButtonText(juce::String(i + 1));
        addAndMakeVisible(bankButtons[i]);
        bankButtons[i].onClick = [this, i] {
            if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
                if (proc->isTestMode) proc->triggerTestProgram(i);
                else {
                    auto& pmRef = presetBrowser.getPresetManager();
                    int activeBank = ((pmRef.getCurrentPresetIndex() % 64) / 8) + 1;
                    pmRef.selectPresetByBankAndPatch(activeGroup, activeBank, i + 1);
                    presetBrowser.setPresetIndex(pmRef.getCurrentPresetIndex());
                    if (onPresetLoad) onPresetLoad(pmRef.getCurrentPresetIndex());
                }
            }
        };
    }

    addAndMakeVisible(decBankButton); decBankButton.setButtonText("< BK");
    addAndMakeVisible(incBankButton); incBankButton.setButtonText("BK >");
    decBankButton.onClick = [this] { 
        auto& pmRef = presetBrowser.getPresetManager(); 
        pmRef.selectLibrary(pmRef.getActiveLibraryIndex()-1); 
        presetBrowser.refreshPresetList(); 
    };
    incBankButton.onClick = [this] { 
        auto& pmRef = presetBrowser.getPresetManager(); 
        pmRef.selectLibrary(pmRef.getActiveLibraryIndex()+1); 
        presetBrowser.refreshPresetList(); 
    };

    addAndMakeVisible(lcd);
    presetBrowser.onPresetChanged = [&](const juce::String&) {
        if(onPresetLoad) onPresetLoad(presetBrowser.getPresetManager().getCurrentPresetIndex());
    };
    presetBrowser.onGetCurrentState = [&]() { return apvtsRef.copyState(); };

    connectButtons();
    updateGroupUI();
    startTimer(50);
}



void JunoControlSection::connectButtons()
{
    // [FIXED] SAVE Button logic
    saveButton.onClick = [this] {
        auto currentState = apvtsRef.copyState();
        auto* w = new juce::AlertWindow("Save Patch", "Enter name:", juce::MessageBoxIconType::QuestionIcon);
        w->addTextEditor("name", "New Sound", "Patch Name:");
        w->addButton("SAVE", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, currentState, w](int result) {
            if (result == 1) {
                juce::String name = w->getTextEditorContents("name");
                if (name.isNotEmpty()) {
                    PresetManager& pm = presetBrowser.getPresetManager();
                    pm.saveUserPreset(name, currentState);
                    presetBrowser.refreshPresetList();
                    presetBrowser.setPresetIndex(pm.getCurrentPresetIndex());
                    lcd.setText("PATCH SAVED");
                }
            }
            delete w;
        }), true);
    };
    
    // [FIXED] EXPORT Button: Persistent instance
    dumpButton.onClick = [this] {
        PresetManager& pm = presetBrowser.getPresetManager();
        fileChooser = std::make_unique<juce::FileChooser>(
            "Export Current Bank to JSON...",
            juce::File(pm.getLastPath()), "*.json");

        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file != juce::File()) {
                    PresetManager& pmRef = presetBrowser.getPresetManager();
                    pmRef.exportLibraryToJson(file);
                    lcd.setText("BANK EXPORTED");
                }
            });
    };

    randomButton.onClick = [this] {
        presetBrowser.getPresetManager().randomizeCurrentParameters(apvtsRef);
        lcd.setText("RANDOM PATCH");
    };

    manualButton.onClick = [this] {
        if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
            proc->updateParamsFromAPVTS();
            proc->sendManualMode(); 
            lcd.setText("MANUAL MODE");
        }
    };

    groupAButton.onClick = [this] { activeGroup = 0; updateGroupUI(); };
    groupBButton.onClick = [this] { activeGroup = 1; updateGroupUI(); };

    loadButton.onClick = [this] { 
        PresetManager& pm = presetBrowser.getPresetManager();
        fileChooser = std::make_unique<juce::FileChooser>("Load Patch (.json)...",
            juce::File(pm.getLastPath()), "*.json");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) {
                    auto json = juce::JSON::parse(file);
                    if (json.isObject()) {
                        auto obj = json.getDynamicObject();
                        if (obj && obj->hasProperty("state")) {
                            PresetManager& pmRef = presetBrowser.getPresetManager();
                            pmRef.saveUserPreset(obj->getProperty("name").toString(), juce::ValueTree::fromXml(obj->getProperty("state").toString())); 
                            pmRef.setLastPath(file.getParentDirectory().getFullPathName());
                            presetBrowser.refreshPresetList();
                            lcd.setText("PATCH LOADED");
                        }
                    }
                }
            });
    };

    prevPatchButton.onClick = [this] { presetBrowser.prevPreset(); };
    nextPatchButton.onClick = [this] { presetBrowser.nextPreset(); };
    
    sysexButton.onClick = [this] {
        PresetManager& pm = presetBrowser.getPresetManager();
        fileChooser = std::make_unique<juce::FileChooser>("Import Juno Patches (.syx / .jno)...",
            juce::File(pm.getLastPath()), "*.syx;*.jno");

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) {
                     PresetManager& pmRef = presetBrowser.getPresetManager();
                     auto res = pmRef.importPresetsFromFile(file);
                     if (res.wasOk()) {
                         presetBrowser.refreshPresetList();
                         lcd.setText("IMPORTED");
                         if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor))
                             proc->loadPreset(pmRef.getCurrentPresetIndex());
                     } else {
                         juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Import Error", res.getErrorMessage(), "OK");
                     }
                }
            });
    };

    loadTapeButton.onClick = [this] {
        PresetManager& pm = presetBrowser.getPresetManager();
        fileChooser = std::make_unique<juce::FileChooser>("Load Tape WAV...",
             juce::File(pm.getLastPath()), "*.wav");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
             [this](const juce::FileChooser& fc) {
                 auto file = fc.getResult();
                 if (file.existsAsFile()) {
                     PresetManager& pmRef = presetBrowser.getPresetManager();
                     auto res = pmRef.loadTape(file);
                     if (res.wasOk()) {
                         presetBrowser.refreshPresetList();
                         lcd.setText("TAPE LOADED");
                     } else {
                         juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Tape Error", res.getErrorMessage(), "OK");
                     }
                 }
             });
    };
    
    powerButton.onClick = [this] {
        if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
            proc->enterTestMode(!proc->isTestMode);
            powerButton.setColour(juce::TextButton::buttonColourId, proc->isTestMode ? juce::Colours::red : juce::Colours::black);
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

    addChildComponent(paramDisplay); // Hidden by default (shown by showParameter)
    paramDisplay.setAlwaysOnTop(true);
    
    addAndMakeVisible(sysExDisplay);
}

void JunoControlSection::updateGroupUI()
{
    groupAButton.setToggleState(activeGroup == 0, juce::dontSendNotification);
    groupBButton.setToggleState(activeGroup == 1, juce::dontSendNotification);
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
    g.drawText("MEMORY / BANKS", getWidth() - 500, 0, 500, 24, juce::Justification::centred);
    g.setFont(10.0f);
    if (bankSelectButtons[0].isVisible()) {
        auto bb = bankSelectButtons[0].getBounds();
        g.drawText("BANK", bb.getX() - 40, bb.getY(), 35, bb.getHeight(), juce::Justification::centredRight);
    }
    if (bankButtons[0].isVisible()) {
        auto pb = bankButtons[0].getBounds();
        g.drawText("PATCH", pb.getX() - 40, pb.getY(), 35, pb.getHeight(), juce::Justification::centredRight);
    }
}

void JunoControlSection::resized()
{
    auto b = getLocalBounds();
    int margin = 15;
    int leftX = margin;
    portLabel.setBounds(leftX, 35, 60, 20);
    portSlider.setBounds(leftX, 55, 60, 60);
    portButton.setBounds(leftX, 120, 60, 25);
    int assignX = leftX + 75;
    modeLabel.setBounds(assignX, 35, 70, 20);
    modeCombo.setBounds(assignX, 60, 80, 25);
    int centerW = 500;
    int centerX = (getWidth() - centerW) / 2;
    lcd.setBounds(centerX, 35, centerW, 50);

    int gapX = assignX + 90; // End of Assign section (approx)
    int gapW = centerX - gapX - 10; // Width available before LCD
    
    // Parameter Display in the gap
    paramDisplay.setBounds(gapX, 35, gapW, 50); 
    
    // SysEx Display under Param Display
    sysExDisplay.setBounds(gapX, 90, gapW, 40); 

    int browserY = 108; // Shifted down slightly to fit SysEx
    prevPatchButton.setBounds(centerX, browserY, 35, 30);
    nextPatchButton.setBounds(centerX + centerW - 35, browserY, 35, 30);
    presetBrowser.setBounds(centerX + 45, browserY, centerW - 90, 30);
    int centerBtnY = 145;
    panicButton.setBounds(centerX + 50, centerBtnY, 100, 25);
    powerButton.setBounds(centerX + 160, centerBtnY, 60, 25);
    randomButton.setBounds(centerX + 230, centerBtnY, 80, 25);
    int rightW = 550;
    int rightX = getWidth() - rightW - margin;
    int gridX = rightX + 45;
    int gridY = 75;
    int btnW = 60; 
    int funcY = 35;
    int funcW = 85;
    int fGap = 5;
    saveButton.setBounds(gridX, funcY, funcW, 30);
    sysexButton.setBounds(gridX + funcW + fGap, funcY, funcW, 30);
    loadTapeButton.setBounds(gridX + (funcW + fGap)*2, funcY, funcW + 20, 30);
    loadButton.setBounds(gridX + (funcW + fGap)*3 + 25, funcY, funcW, 30);
    dumpButton.setBounds(gridX + (funcW + fGap)*4 + 35, funcY, funcW, 30);
    for(int i=0; i<8; ++i) {
        bankButtons[i].setBounds(gridX + (i * btnW), gridY, btnW - 4, 28);
        bankSelectButtons[i].setBounds(gridX + (i * btnW), gridY + 32, btnW - 4, 28);
    }
    int bottomY = 145;
    decBankButton.setBounds(gridX, bottomY, 45, 25);
    incBankButton.setBounds(gridX + 50, bottomY, 45, 25);
    groupAButton.setBounds(gridX + 110, bottomY, 70, 25);
    groupBButton.setBounds(gridX + 185, bottomY, 70, 25);
    manualButton.setBounds(gridX + 265, bottomY, 80, 25);
    midiOutButton.setBounds(gridX + 355, bottomY, 90, 25);
}

void JunoControlSection::timerCallback()
{
    PresetManager& pmRef = presetBrowser.getPresetManager();
    if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
        // [New] Update SysEx Display
        // We need a way to get the current SysEx data from Processor without triggering a dump
        if (proc->lastParamsChangeCounter != lastSeenParamsChange) {
             lastSeenParamsChange = proc->lastParamsChangeCounter;
             sysExDisplay.updateData(proc->getCurrentSysExData());
        }

        if (proc->isTestMode) lcd.setText("TEST MODE");
        else {
            int patchIdx = pmRef.getCurrentPresetIndex();
            juce::String groupName = (patchIdx < 64) ? "A" : "B";
            int b = ((patchIdx % 64) / 8) + 1;
            int p = (patchIdx % 8) + 1;
            lcd.setText(groupName + "-" + juce::String(b) + "-" + juce::String(p) + "  " + pmRef.getCurrentPresetName());
        }
    }
}

void JunoControlSection::showParameter(const juce::String& name, const juce::String& value)
{
    paramDisplay.showParameter(name, value);
}
