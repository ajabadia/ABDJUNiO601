#include "PluginEditor.h"
#include "PresetManager.h"
#include "../ABD-SynthEngine/Protocol/Presets/PresetManagerBase.h"
#include "BuildVersion.h"
#include "../Core/JunoTapeEncoder.h" 

ABDSimpleJuno106AudioProcessorEditor::ABDSimpleJuno106AudioProcessorEditor (ABDSimpleJuno106AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      lookAndFeel(),
      menuBar(this),
      bankSection(*p.getPresetManager(), p.getAPVTS()),
      lcd(),
      sysExDisplay(),
      lfoSection(p.getAPVTS(), p.getMidiLearnHandler()),
      dcoSection(p.getAPVTS(), p.getMidiLearnHandler()),
      hpfSection(p.getAPVTS(), p.getMidiLearnHandler()),
      vcfSection(p.getAPVTS(), p.getMidiLearnHandler()),
      vcaSection(p.getAPVTS(), p.getMidiLearnHandler()),
      envSection(p.getAPVTS(), p.getMidiLearnHandler()),
      chorusSection(p, p.getAPVTS(), p.getMidiLearnHandler()),
      performanceSection(p.getAPVTS(), p.getMidiLearnHandler()),
      midiKeyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    DBG("ABDSimpleJuno106AudioProcessorEditor::Constructor START");
    addAndMakeVisible(sysExDisplay);

    addAndMakeVisible(menuBar);
    setResizable(true, true);
    setResizeLimits(1000, 600, 2000, 1200);
    setSize (1200, 750); 
    DBG("ABDSimpleJuno106AudioProcessorEditor::setSize DONE");

    addAndMakeVisible(bankSection); DBG("Editor: bankSection added");
    addAndMakeVisible(lcd); DBG("Editor: lcd added");
    addAndMakeVisible(lfoSection); DBG("Editor: lfoSection added");
    addAndMakeVisible(dcoSection); DBG("Editor: dcoSection added");
    addAndMakeVisible(hpfSection); DBG("Editor: hpfSection added");
    addAndMakeVisible(vcfSection); DBG("Editor: vcfSection added");
    addAndMakeVisible(vcaSection); DBG("Editor: vcaSection added");
    addAndMakeVisible(envSection); DBG("Editor: envSection added");
    addAndMakeVisible(chorusSection); DBG("Editor: chorusSection added");
    addAndMakeVisible(performanceSection); DBG("Editor: performanceSection added");
    addAndMakeVisible(midiKeyboard); DBG("Editor: midiKeyboard added");
    
    addAndMakeVisible(memoryProtectButton);
    memoryProtectButton.setStyle(true); // Beige style
    memoryProtectButton.setTooltip("Memory Protect (Internal RAM)");
    memoryProtectButton.setClickingTogglesState(true);
    memoryProtectAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getAPVTS(), "memoryProtect", memoryProtectButton);
    
    DBG("ABDSimpleJuno106AudioProcessorEditor::addAndMakeVisible DONE");
    midiKeyboard.setAvailableRange(36, 96); 
    
    setLookAndFeel(&lookAndFeel);
    
    // --- CONNECT LOGIC ---
    
    // Preset Load
    bankSection.presetBrowser.onPresetSelected = [this](int /*libIdx*/, int presetIdx) {
         audioProcessor.loadPreset(presetIdx);
    };
    
    // Bank Buttons (1-8) -> In Juno-106 terms, these are the PATCH buttons
    for(int i=0; i<8; ++i) {
        bankSection.bankButtons[i].onClick = [this, i] {
             if (isWriteArmed) {
                 writePatchTarget = i + 1;
                 
                 // Perform Write
                 auto& pm = *audioProcessor.getPresetManager();
                 bool protectedState = (float)*audioProcessor.getAPVTS().getRawParameterValue("memoryProtect") > 0.5f;
                 
                 if (protectedState) {
                     lcd.setText("MEMORY PROTECTED");
                     isWriteArmed = false;
                     return;
                 }
                 
                 auto res = pm.writeToInternalSlot(0, writeBankTarget, writePatchTarget, audioProcessor.getAPVTS().copyState());
                 if (res.wasOk()) {
                     lcd.setText("WRITTEN TO " + juce::String(writeBankTarget) + "-" + juce::String(writePatchTarget));
                     bankSection.presetBrowser.refresh();
                 } else {
                     lcd.setText("WRITE ERROR");
                 }
                 isWriteArmed = false;
                 return;
             }

             if (audioProcessor.isTestMode) {
                 audioProcessor.triggerTestProgram(i + 1);
                 lcd.setText("TEST FUNCTION " + juce::String(i + 1));
                 bankSection.updateBankLeds(i); // [Fix] Visual feedback in Test Mode
                 return;
             }

             auto& pm = *audioProcessor.getPresetManager();
             int currentBank = ((pm.getCurrentPresetIndex() % 64) / 8) + 1;
             int group = (pm.getCurrentPresetIndex() >= 64) ? 1 : 0; 
             pm.selectPresetByBankAndPatch(group, currentBank, i + 1);
             bankSection.presetBrowser.refresh();
             audioProcessor.loadPreset(pm.getCurrentPresetIndex());
        };
    }
    
    // NAVIGATION LOGIC
    auto navPatch = [this](int delta) {
        auto& pm = *audioProcessor.getPresetManager();
        int newIdx = pm.getCurrentPresetIndex() + delta;
        newIdx = juce::jlimit(0, 127, newIdx); 
        pm.setCurrentPreset(newIdx);
        bankSection.presetBrowser.refresh();
        audioProcessor.loadPreset(newIdx);
    };
    
    auto navBank = [this](int delta) {
        auto& pm = *audioProcessor.getPresetManager();
        int current = pm.getCurrentPresetIndex();
        int bank = (current % 64) / 8; // 0-7
        int group = current / 64; // 0-1
        int patch = current % 8; // 0-7
        
        bank += delta;
        if (bank > 7) { bank = 0; group = 1 - group; } 
        if (bank < 0) { bank = 7; group = 1 - group; }
        
        int newIdx = (group * 64) + (bank * 8) + patch;
        newIdx = juce::jlimit(0, 127, newIdx);
        
        pm.setCurrentPreset(newIdx);
        bankSection.presetBrowser.refresh();
        audioProcessor.loadPreset(newIdx);
    };

    bankSection.prevPatchButton.onClick = [this, navPatch] { 
        if (isWriteArmed) {
            writeBankTarget--;
            if (writeBankTarget < 1) writeBankTarget = 16;
            lcd.setText("WRITE TO? " + juce::String(writeBankTarget) + "-" + juce::String(writePatchTarget));
            return;
        }
        navPatch(-1); 
    };
    bankSection.nextPatchButton.onClick   = [this, navPatch] { 
        if (isWriteArmed) {
            writeBankTarget++;
            if (writeBankTarget > 16) writeBankTarget = 1;
            lcd.setText("WRITE TO? " + juce::String(writeBankTarget) + "-" + juce::String(writePatchTarget));
            return;
        }
        navPatch(1); 
    };
    bankSection.decBankButton.onClick  = [this, navBank] { 
        if (isWriteArmed) {
            writeBankTarget--;
            if (writeBankTarget < 1) writeBankTarget = 16;
            lcd.setText("WRITE TO? " + juce::String(writeBankTarget) + "-" + juce::String(writePatchTarget));
            return;
        }
        navBank(-1); 
    };
    bankSection.incBankButton.onClick     = [this, navBank] { 
        if (isWriteArmed) {
            writeBankTarget++;
            if (writeBankTarget > 16) writeBankTarget = 1;
            lcd.setText("WRITE TO? " + juce::String(writeBankTarget) + "-" + juce::String(writePatchTarget));
            return;
        }
        navBank(1); 
    };

    // Function Buttons
    bankSection.panicButton.onClick = [this] { audioProcessor.triggerPanic(); lcd.setText("PANIC"); };
    bankSection.randomButton.onClick = [this] { 
        handleRandomize();
    };
    bankSection.manualButton.onClick = [this] { 
        if (bankSection.manualButton.getToggleState()) {
            audioProcessor.updateParamsFromAPVTS();
            audioProcessor.sendManualMode();
            lcd.setText("MANUAL MODE ON");
        } else {
            lcd.setText("MANUAL MODE OFF (Load Preset to Reset)");
        }
    };
    bankSection.powerButton.onClick = [this] { // Test Mode
        bool state = bankSection.powerButton.getToggleState();
        audioProcessor.enterTestMode(state);
        lcd.setText(state ? "TEST MODE STARTED" : "TEST MODE ENDED");
    };
    
    // Connect new Menu-Centralized Actions to Section Buttons (Optional/Backwards compatibility)
    bankSection.onSave = [this] { handleSave(); };
    bankSection.onLoad = [this] { handleLoad(); };
    bankSection.onDump = [this] { handleExportBank(); };
    bankSection.onSysEx = [this] { handleImportSysex(); };
    bankSection.onTape = [this] { handleLoadTape(); };
    bankSection.onRandom = [this] { handleRandomize(); };
    bankSection.onPanic = [this] { handlePanic(); };

    // Initial Update
    audioProcessor.editor = this;
    
    // Phase 5: Register for all parameter changes
    auto& params = audioProcessor.getAPVTS().processor.getParameters();
    for (auto* paramPtr : params) {
        if (auto* param = dynamic_cast<juce::AudioProcessorParameterWithID*>(paramPtr)) {
            audioProcessor.getAPVTS().addParameterListener(param->paramID, this);
        }
    }

    stopTimer(); 
    startTimerHz(30); 
    DBG("ABDSimpleJuno106AudioProcessorEditor::Constructor END");
    
    // Initial LCD state
    lastPresetName = audioProcessor.getPresetManager()->getCurrentPreset().name;
    lcd.setText(lastPresetName);
}

ABDSimpleJuno106AudioProcessorEditor::~ABDSimpleJuno106AudioProcessorEditor()
{
    stopTimer();
    
    // Unregister parameter listeners
    auto& params = audioProcessor.getAPVTS().processor.getParameters();
    for (auto* p : params) {
        if (auto* param = dynamic_cast<juce::AudioProcessorParameterWithID*>(p)) {
            audioProcessor.getAPVTS().removeParameterListener(param->paramID, this);
        }
    }

    setLookAndFeel(nullptr);
    audioProcessor.editor = nullptr; 
}

void ABDSimpleJuno106AudioProcessorEditor::timerCallback()
{
    auto& pm = *audioProcessor.getPresetManager();
    
    // 1. Detect Preset Change (to update display and state)
    juce::String currentPreset = pm.getCurrentPreset().name;
    if (currentPreset != lastPresetName) {
        lastPresetName = currentPreset;
        
        // If not currently displaying a parameter edit, update LCD
        if (lcdDisplayTimer == 0) {
            lcd.setText(lastPresetName);
        }
        
        // Update SysEx Display when patch changes
    }
    
    // Always update SysEx Display (real-time feedback for all param changes)
    auto dump = audioProcessor.getCurrentSysExData();
    if (dump.getRawDataSize() > 0) {
        std::vector<uint8_t> vec((const uint8_t*)dump.getRawData(), (const uint8_t*)dump.getRawData() + dump.getRawDataSize());
        sysExDisplay.setDumpData(vec);
    }

    // 2. Handle Phase 5: Interactive LCD Feedback Timer
    if (lcdDisplayTimer > 0) {
        lcdDisplayTimer--;
        if (lcdDisplayTimer == 0) {
            lcd.setText(lastPresetName);
        }
    }

    // 3. Update Procedural 7-Segment Display (Alphanumeric Context)
    auto libName = pm.getCurrentLibraryName();
    int idx = pm.getCurrentPresetIndex();
    const auto& p = pm.getCurrentPreset();
    
    juce::String bS, pS;
    if (libName == "Factory") {
        bS = juce::String(p.originBank);
        pS = juce::String(p.originPatch);
    } else {
        // Show Library Initial (U, L, etc.) and Patch Index (0-9, A-F in Hex)
        bS = libName.substring(0, 1).toUpperCase();
        int wrappedIdx = idx % 16;
        if (wrappedIdx < 10) pS = juce::String (wrappedIdx);
        else pS = juce::String (static_cast<juce::juce_wchar> ('A' + (wrappedIdx - 10)));
    }
    
    bankSection.updateDisplay(bS, pS);
}

void ABDSimpleJuno106AudioProcessorEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Fix shadowing and unused parameter warnings while keeping async UI updates
    juce::MessageManager::callAsync([this, parameterID, newValue]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter(parameterID)) {
             juce::String label = param->getName(32);
             juce::String valueStr = param->getText(newValue, 16);
             
             lcd.setText(label + ": " + valueStr);
             lcdDisplayTimer = 45; // ~1.5 seconds at 30Hz
        }
    });
}

void ABDSimpleJuno106AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(JunoUI::kPanelGrey);
    
    auto bounds = getLocalBounds();
    
    // Bottom Stripes (Juno Style)
    auto footer = bounds.removeFromBottom(20);
    g.setColour(juce::Colours::black);
    g.fillRect(footer);
    
    float stripeW = bounds.getWidth() / 3.0f;
    g.setColour(JunoUI::kStripRed);
    g.fillRect(0.0f, (float)footer.getY(), stripeW, 4.0f);
    g.setColour(JunoUI::kStripBlue);
    g.fillRect(stripeW, (float)footer.getY(), stripeW, 4.0f);
    g.setColour(JunoUI::kStripOrange);
    g.fillRect(stripeW * 2, (float)footer.getY(), stripeW, 4.0f);
    
    // Header Name "ABD JUNiO 601"
    g.setColour(JunoUI::kTextWhite);
    g.setFont(juce::FontOptions("Arial", 28.0f, juce::Font::bold));
    g.drawText("ABD JUNiO 601", bounds.getRight() - 260, 30, 240, 40, juce::Justification::centredRight);
}

void ABDSimpleJuno106AudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    
    // 1. TOP HEADER (Menu + LCD)
    menuBar.setBounds(b.removeFromTop(24));
    auto header = b.removeFromTop(80);
    
    // Left side of Header: SysEx Display (Restored)
    // Left side of Header: SysEx Display (Restored)
    // UX: Show complete display (wider)
    sysExDisplay.setBounds(header.removeFromLeft(400).reduced(10, 5));
    
    // Center LCD in header
    auto lcdBounds = header.reduced(20, 10).withWidth(400).withCentre(header.getCentre());
    lcd.setBounds(lcdBounds);
    
    // Memory Protect to the left of LCD
    memoryProtectButton.setBounds(lcdBounds.getX() - 90, lcdBounds.getY() + 15, 80, 25);
    
    // 2. LEFT SIDEBAR (Bank + Performance) - SWAPPED SIDE
    auto sidebar = b.removeFromLeft(240);
    bankSection.setBounds(sidebar.removeFromTop(380)); // Reduced Bank section height
    performanceSection.setBounds(sidebar); // Performance takes the rest
    
    // 3. BOTTOM KEYBOARD - EXPANDED TO FILL
    auto kbdArea = b.removeFromBottom(150); // Slightly taller keyboard
    midiKeyboard.setBounds(kbdArea.reduced(5, 5)); // Full width expansion
    midiKeyboard.setKeyWidth(40.0f); // Wider keys
    
    // 4. MAIN SYNTH ROWS (Remaining Space)
    auto mainArea = b.reduced(5);
    
    juce::FlexBox mainRows;
    mainRows.flexDirection = juce::FlexBox::Direction::column;
    mainRows.flexWrap = juce::FlexBox::Wrap::noWrap;
    
    // Row 1: LFO, DCO, HPF
    juce::FlexBox row1;
    row1.flexDirection = juce::FlexBox::Direction::row;
    row1.items.add(juce::FlexItem(lfoSection).withFlex(1.2f).withMargin(2));
    row1.items.add(juce::FlexItem(dcoSection).withFlex(2.8f).withMargin(2));
    row1.items.add(juce::FlexItem(hpfSection).withFlex(0.8f).withMargin(2));
    
    // Row 2: VCF, VCA, ENV, Chorus
    juce::FlexBox row2;
    row2.flexDirection = juce::FlexBox::Direction::row;
    row2.items.add(juce::FlexItem(vcfSection).withFlex(2.2f).withMargin(2));
    row2.items.add(juce::FlexItem(vcaSection).withFlex(1.0f).withMargin(2));
    row2.items.add(juce::FlexItem(envSection).withFlex(1.8f).withMargin(2));
    row2.items.add(juce::FlexItem(chorusSection).withFlex(1.0f).withMargin(2));
    
    mainRows.items.add(juce::FlexItem(row1).withFlex(1.0f));
    mainRows.items.add(juce::FlexItem(row2).withFlex(1.0f));
    
    mainRows.performLayout(mainArea);
}

juce::StringArray ABDSimpleJuno106AudioProcessorEditor::getMenuBarNames()
{
    return { "File", "Edit", "View", "Help" };
}

juce::PopupMenu ABDSimpleJuno106AudioProcessorEditor::getMenuForIndex (int menuIndex, const juce::String& /*menuName*/)
{
    juce::PopupMenu menu;
    
    if (menuIndex == 0) { // File
        menu.addItem(1, "Load Juno Patch...", true);
        menu.addItem(2, "Save Current Patch...", true);
        menu.addSeparator();
        menu.addItem(3, "Export Bank as JSON...", true);
        menu.addItem(4, "Import SysEx / JNO...", true);
        menu.addItem(5, "Load Tape (.wav)...", true);
        menu.addSeparator();
        // Standalone Exit
        if (juce::JUCEApplication::isStandaloneApp())
            menu.addItem(6, "Exit", true);
    }
    else if (menuIndex == 1) { // Edit
        menu.addItem(12, "Undo", true); // Todo: Connect to UndoManager
        menu.addItem(13, "Redo", true);
        menu.addSeparator();
        menu.addItem(10, "Randomize Sound", true);
        menu.addItem(11, "Panic (All Notes Off)", true);
        menu.addSeparator();
        
        bool midiTxState = (float)*audioProcessor.getAPVTS().getRawParameterValue("midiOut") > 0.5f;
        menu.addItem(14, "MIDI TX", true, midiTxState);
        
        menu.addItem(15, "Options...", true); // Moved from Header
    }
    else if (menuIndex == 2) { // View
        menu.addItem(20, "Show/Hide Sidebar", true, true);
    }
    else if (menuIndex == 3) { // Help
        menu.addItem(30, "About JUNiO 601...", true);
    }
    
    return menu;
}

void ABDSimpleJuno106AudioProcessorEditor::menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/)
{
    switch (menuItemID) {
        case 1:  handleLoad(); break;
        case 2:  handleSave(); break;
        case 3:  handleExportBank(); break;
        case 4:  handleImportSysex(); break;
        case 5:  handleLoadTape(); break;
        
        case 6:  juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
        
        case 10: handleRandomize(); break;
        case 11: handlePanic(); break;
        case 12: audioProcessor.undo(); break; // Implemented in Processor
        case 13: audioProcessor.redo(); break;
        case 14: audioProcessor.toggleMidiOut(); break; 
        case 15: /* handleOptions */ break;
        
        case 30: handleAbout(); break;
    }
}

void ABDSimpleJuno106AudioProcessorEditor::handleSave()
{
    if (!isWriteArmed) {
        isWriteArmed = true;
        auto& pm = *audioProcessor.getPresetManager();
        int idx = pm.getCurrentPresetIndex();
        writeBankTarget = (idx / 8) + 1; // 1-16
        writePatchTarget = (idx % 8) + 1; // 1-8
        lcd.setText("WRITE TO? " + juce::String(writeBankTarget) + "-" + juce::String(writePatchTarget));
    } else {
        isWriteArmed = false;
        lcd.setText(audioProcessor.getPresetManager()->getCurrentPreset().name);
    }
}

void ABDSimpleJuno106AudioProcessorEditor::handleLoad()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load Patch...", 
        audioProcessor.getPresetManager()->getLastPath(), "*.jno");
        
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile()) {
                bankSection.presetBrowser.getPresetManager().setLastPath(file.getParentDirectory().getFullPathName());
                // Logic to load .jno
                audioProcessor.getPresetManager()->importPresetsFromFile(file);
                bankSection.presetBrowser.refresh();
            }
        });
}

void ABDSimpleJuno106AudioProcessorEditor::handleImportSysex()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Import SysEx / JNO...", 
        audioProcessor.getPresetManager()->getLastPath(), "*.syx;*.jno");
        
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile()) {
                audioProcessor.getPresetManager()->setLastPath(file.getParentDirectory().getFullPathName());
                auto res = audioProcessor.getPresetManager()->importPresetsFromFile(file);
                if (res.wasOk()) {
                    bankSection.presetBrowser.refresh();
                    lcd.setText("IMPORT SUCCESSFUL");
                } else {
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Import Error", res.getErrorMessage());
                }
            }
        });
}

void ABDSimpleJuno106AudioProcessorEditor::handleLoadTape()
{
     fileChooser = std::make_unique<juce::FileChooser> ("Load Tape (.wav)...", 
        audioProcessor.getPresetManager()->getLastPath(), "*.wav");
        
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile()) {
                audioProcessor.getPresetManager()->setLastPath(file.getParentDirectory().getFullPathName());
                auto res = audioProcessor.getPresetManager()->loadTape(file);
                if (res.wasOk()) {
                    bankSection.presetBrowser.refresh();
                    lcd.setText("TAPE LOADED");
                } else {
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Tape Error", res.getErrorMessage());
                }
            }
        });
}

void ABDSimpleJuno106AudioProcessorEditor::handleExportBank()
{
     fileChooser = std::make_unique<juce::FileChooser> ("Export Bank...", 
        bankSection.presetBrowser.getPresetManager().getLastPath(), "*.json");
        
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File()) {
                audioProcessor.getPresetManager()->setLastPath(file.getParentDirectory().getFullPathName());
                audioProcessor.getPresetManager()->exportLibraryToJson(file);
                lcd.setText("BANK EXPORTED");
            }
        });
}

void ABDSimpleJuno106AudioProcessorEditor::handleRandomize()
{
    audioProcessor.randomizeSound();
    lcd.setText("RANDOMIZED");
}

void ABDSimpleJuno106AudioProcessorEditor::handlePanic()
{
    audioProcessor.triggerPanic();
    lcd.setText("PANIC");
}

void ABDSimpleJuno106AudioProcessorEditor::handleAbout()
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, 
        "About ABD JUNiO 601", 
        "ABD JUNiO 601\nVersion " + juce::String(ProjectInfo::versionString) + "\n\nAuthentic Juno-106 Emulation\n(c) 2026 SynthOps Laboratory", 
        "OK");
}
