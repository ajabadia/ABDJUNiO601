#include "JunoControlSection.h"
#include "../../Core/JunoTapeEncoder.h"
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
        int idx = presetBrowser.getPresetManager().getCurrentPresetIndex();
        activeGroup = (idx < 64) ? 0 : 1; 
        updateGroupUI();
        if(onPresetLoad) onPresetLoad(idx);
    };
    presetBrowser.onGetCurrentState = [&]() { return apvtsRef.copyState(); };

    // [Senior Audit] TUNE KNOB
    JunoUI::setupLabel(masterTuneLabel, "TUNE", *this);
    masterTuneLabel.setJustificationType(juce::Justification::centred);
    JunoUI::styleSmallSlider(masterTuneKnob);
    addAndMakeVisible(masterTuneKnob);
    masterTuneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvtsRef, "tune", masterTuneKnob);
    
    // [Senior Audit] MIDI LED
    addAndMakeVisible(midiLed);

    connectButtons();
    updateGroupUI();
    startTimer(50);
}

// ... existing code ...

// INSERT IN RESIZED
// We need to place it. I'll inject into resized via replace_file logic.
// But first let's handle the timer callback update.

// ... existing code ...



void JunoControlSection::connectButtons()
{
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
    
    dumpButton.onClick = [this] {
        juce::PopupMenu m;
        m.addItem(1, "Export Bank (JSON)");
        m.addItem(2, "Export All Libraries (JSON)");
        m.addItem(3, "Export Patch (Tape Audio .wav)");
        
        m.showMenuAsync(juce::PopupMenu::Options(), [this](int result) {
            PresetManager& pm = presetBrowser.getPresetManager();
            if (result == 1) {
                fileChooser = std::make_unique<juce::FileChooser>("Export Bank (JSON)...", juce::File(pm.getLastPath()), "*.json");
                fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [this](const juce::FileChooser& fc) {
                    if (fc.getResult() != juce::File()) {
                        presetBrowser.getPresetManager().exportLibraryToJson(fc.getResult());
                        lcd.setText("BANK EXPORTED");
                    }
                });
            } else if (result == 3) {
                 fileChooser = std::make_unique<juce::FileChooser>("Export Tape Audio (.wav)...", 
                    juce::File(pm.getLastPath()).getChildFile(pm.getCurrentPresetName() + ".wav"), "*.wav");
                 fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [this](const juce::FileChooser& fc) {
                     auto f = fc.getResult();
                     if (f != juce::File()) {
                         if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
                             uint8_t bytes[18];
                             auto msg = proc->getCurrentSysExData();
                             if (msg.getRawDataSize() >= 23) {
                                 const uint8_t* rd = msg.getRawData();
                                 memcpy(bytes, rd + 5, 16);
                                 bytes[16] = rd[21];
                                 bytes[17] = rd[22];
                                 JunoTapeEncoder::saveToWav(f, bytes);
                                 lcd.setText("TAPE EXPORTED");
                             }
                         }
                     }
                 });
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
             proc->triggerPanic();
             lcd.setText("PANIC (FADE EXITS)");
        }
    };

    addChildComponent(paramDisplay); // Hidden by default (shown by showParameter)
    paramDisplay.setAlwaysOnTop(true);
    
    addAndMakeVisible(sysExDisplay);
}

void JunoControlSection::mouseDown(const juce::MouseEvent& e) {
    if (lcd.getBounds().contains(e.getPosition())) {
        if (e.mods.isShiftDown()) {
            presetBrowser.getPresetManager().triggerMemoryCorruption(apvtsRef);
            lcd.setText("MEM ERROR");
        } else if (e.mods.isAltDown()) {
             presetBrowser.getPresetManager().randomizeCurrentParameters(apvtsRef);
             lcd.setText("RANDOMIZED");
        }
    }
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
    int margin = 10;
    int leftX = margin;
    
    // PORT & ASSIGN - Compacted
    portLabel.setBounds(leftX, 35, 50, 20);
    portSlider.setBounds(leftX, 55, 50, 50);
    portButton.setBounds(leftX, 110, 50, 25);
    
    int assignX = leftX + 60;
    modeLabel.setBounds(assignX, 35, 70, 20);
    modeCombo.setBounds(assignX, 60, 80, 25);
    
    masterTuneLabel.setBounds(assignX + 90, 35, 40, 20);
    masterTuneKnob.setBounds(assignX + 90, 60, 40, 40);

    // CENTRAL BLOCK (LCD, Browser, Buttons)
    int lcdX = assignX + 150;
    int lcdW = 400; // Reduced from 500
    lcd.setBounds(lcdX, 35, lcdW, 50);

    // Browser under LCD
    int browserY = 95;
    prevPatchButton.setBounds(lcdX, browserY, 30, 25);
    presetBrowser.setBounds(lcdX + 35, browserY, lcdW - 70, 25);
    nextPatchButton.setBounds(lcdX + lcdW - 30, browserY, 30, 25);

    // Function Buttons under Browser
    int btnY = 125;
    // int btnW = 80;
    panicButton.setBounds(lcdX, btnY, 80, 25);
    powerButton.setBounds(lcdX + 90, btnY, 50, 25);
    randomButton.setBounds(lcdX + 150, btnY, 70, 25);

    // RIGHT SIDE GRID (Banks) - Anchored to Right Edge
    int rightMargin = 10;
    int rightBlockW = 450; // Enough for buttons
    int gridX = getWidth() - rightBlockW - rightMargin;

    // SysEx & Param Display in the gap
    int gapX = lcdX + lcdW + 20;
    int gapW = gridX - gapX - 10; // Fill available space
    
    // SysEx Display (Expanded)
    sysExDisplay.setBounds(gapX, 35, gapW, 40);
    
    // Param Display (Below SysEx)
    paramDisplay.setBounds(gapX, 80, gapW, 20); 
    paramDisplay.setJustificationType(juce::Justification::centredLeft); // Readable

    
    int gridY = 55;
    int bankBtnW = 50; 
    
    // Top Functions
    int funcY = 20;
    int fGap = 5;
    int fW = 70;
    saveButton.setBounds(gridX, funcY, fW, 25);
    sysexButton.setBounds(gridX + fW + fGap, funcY, fW, 25);
    loadTapeButton.setBounds(gridX + (fW + fGap)*2, funcY, fW + 20, 25);
    loadButton.setBounds(gridX + (fW + fGap)*3 + 20, funcY, fW, 25);
    dumpButton.setBounds(gridX + (fW + fGap)*4 + 30, funcY, fW, 25);

    for(int i=0; i<8; ++i) {
        bankButtons[i].setBounds(gridX + (i * bankBtnW), gridY, bankBtnW - 4, 25);
        bankSelectButtons[i].setBounds(gridX + (i * bankBtnW), gridY + 28, bankBtnW - 4, 25);
    }
    
    int bottomY = 125;
    decBankButton.setBounds(gridX, bottomY, 40, 25);
    incBankButton.setBounds(gridX + 45, bottomY, 40, 25);
    groupAButton.setBounds(gridX + 100, bottomY, 60, 25);
    groupBButton.setBounds(gridX + 165, bottomY, 60, 25);
    manualButton.setBounds(gridX + 240, bottomY, 70, 25);
    midiOutButton.setBounds(gridX + 320, bottomY, 80, 25);
    midiLed.setBounds(gridX + 405, bottomY + 5, 15, 15);
}

void JunoControlSection::timerCallback()
{
    PresetManager& pmRef = presetBrowser.getPresetManager();
    if (auto* proc = dynamic_cast<SimpleJuno106AudioProcessor*>(&processor)) {
        if (proc->lastParamsChangeCounter != lastSeenParamsChange) {
             lastSeenParamsChange = proc->lastParamsChangeCounter;
             // [FIX] Use the new API for JunoSysExDisplay
             auto dump = proc->getCurrentSysExData(); 
             std::vector<uint8_t> vec(dump.getRawData(), dump.getRawData() + dump.getRawDataSize());
             sysExDisplay.setDumpData(vec);
             
             // Flash MIDI LED on activity
             if (midiOutButton.getToggleState()) {
                 midiFlashCounter = 3; 
             }
        }

        if (proc->isTestMode) lcd.setText("TEST MODE");
        else {
            int patchIdx = pmRef.getCurrentPresetIndex();
            juce::String groupName = (patchIdx < 64) ? "A" : "B";
            int b = ((patchIdx % 64) / 8) + 1;
            int p = (patchIdx % 8) + 1;
            lcd.setText(groupName + "-" + juce::String(b) + "-" + juce::String(p) + "  " + pmRef.getCurrentPresetName());
        }
        
        // Blink logic
        if (midiFlashCounter > 0) {
            midiFlashCounter--;
            midiLed.on = true;
        } else {
            midiLed.on = false;
        }
        midiLed.repaint();
    }
}

void JunoControlSection::showParameter(const juce::String& name, const juce::String& value)
{
    paramDisplay.showParameter(name, value);
}
