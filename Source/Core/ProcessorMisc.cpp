#include "ABDSimpleJuno106AudioProcessor.h"
#include "PresetManager.h"
#include "JunoTests.h"
#include "TestPrograms.h"
#include "../UI/WebView/WebViewEditor.h"

//==============================================================================
// Tuning
//==============================================================================
void ABDSimpleJuno106AudioProcessor::loadTuningFile()
{
    DBG("ABDSimpleJuno106AudioProcessor::loadTuningFile CALLED");
    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser = std::make_unique<juce::FileChooser>("Select Scala Tuning File...",
                                                      juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                      "*.scl");

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
            if (tuningManager.parseSCL(file)) {
                voiceManager.setTuningTable(tuningManager.getTuningTable());
                currentTuningName = file.getFileName();
                juce::Logger::writeToLog("[JUNiO] Custom Tuning Loaded: " + currentTuningName);
            }
        }
    });
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::resetTuning()
{
    DBG("ABDSimpleJuno106AudioProcessor::resetTuning CALLED");
    tuningManager.resetToStandard();
    voiceManager.setTuningTable(tuningManager.getTuningTable());
    currentTuningName = "Standard Tuning";
    DBG("Standard Tuning Restored");
}

//==============================================================================
bool ABDSimpleJuno106AudioProcessor::loadScalaTuning(const juce::File& file)
{
    if (file.existsAsFile() && tuningManager.parseSCL(file)) {
        voiceManager.setTuningTable(tuningManager.getTuningTable());
        currentTuningName = file.getFileName();
        juce::Logger::writeToLog("[JUNiO] SCL loaded via WebView: " + currentTuningName);
        return true;
    }
    juce::Logger::writeToLog("[JUNiO] SCL parse failed: " + file.getFullPathName());
    return false;
}

//==============================================================================
// User Settings Persistence
//==============================================================================
void ABDSimpleJuno106AudioProcessor::loadUserSettings()
{
    auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("ABD-IA")
                    .getChildFile("JUNiO-601")
                    .getChildFile("settings.xml");
    if (file.existsAsFile()) {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr) {
            auto vt = juce::ValueTree::fromXml(*xml);
            if (vt.hasType("Settings")) {
                userName = vt.getProperty("userName", "ABD USER").toString();
            }
        }
    }
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::saveUserSettings()
{
    auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("ABD-IA")
                    .getChildFile("JUNiO-601")
                    .getChildFile("settings.xml");
    if (!file.getParentDirectory().exists()) file.getParentDirectory().createDirectory();

    juce::ValueTree vt("Settings");
    vt.setProperty("userName", userName, nullptr);
    auto xml = vt.createXml();
    if (xml != nullptr) xml->writeTo(file);
}

//==============================================================================
// Fidelity Self-Test
//==============================================================================
ABDSimpleJuno106AudioProcessor::SelfTestResult ABDSimpleJuno106AudioProcessor::runSelfTest()
{
    SelfTestResult result;
    result.hasRun = true;
    
    // Test 1: 128 Factory Patches Roundtrip
    if (presetManager) {
        JunoTests::runJunoPatchRoundtripTest(*presetManager, result.presetFailures, result.failedPresets);
    } else {
        result.presetFailures = 128; // Si no hay manager, falla todo
    }

    // Test 2: SysEx Dump Protocol
    JunoTests::runSysExPatchDumpRoundtripTest(result.sysExOk);

    // Test 3: ValueTree/JSON Serialization
    if (presetManager) {
        JunoTests::runPresetJsonRoundtripTest(*presetManager, result.jsonOk);
    } else {
        result.jsonOk = false;
    }

    // Overall Certification Logic
    result.ok = (result.presetFailures == 0) && result.sysExOk && result.jsonOk;
    
    lastSelfTestResult = result;
    
    juce::Logger::writeToLog("[JUNiO] Fidelity Self-Test completed. Certified: " + juce::String(result.ok ? "YES" : "NO"));
    return result;
}

//==============================================================================
// UI Notifications
//==============================================================================
void ABDSimpleJuno106AudioProcessor::notifyUIOfStateChange()
{
    // Notify the host that the state has changed
    updateHostDisplay();
    
    // Custom trigger for WebViewEditor if needed
    if (editor != nullptr) {
        if (auto* wv = dynamic_cast<WebViewEditor*>(editor)) {
            wv->sendPresetListUpdate();
            if (presetManager) {
                const auto& p = presetManager->getCurrentPreset();
                wv->sendBankPatchUpdate(p.originGroup, p.originBank, p.originPatch);
            }
        }
    }
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::sendParamUpdateToUI()
{
    if (editor != nullptr) {
        if (auto* wv = dynamic_cast<WebViewEditor*>(editor)) {
            juce::DynamicObject::Ptr state = new juce::DynamicObject();
            for (auto* param : getParameters()) {
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
                    state->setProperty(juce::Identifier(p->getParameterID()), (double)p->getValue());
                }
            }
            wv->dispatchToJS("parameterSetUpdate", juce::var(state.get()));
            juce::Logger::writeToLog("[JUNiO] Parameter Set Update dispatched to UI");
        }
    }
}

//==============================================================================
// Recording [Build 103]
//==============================================================================
void ABDSimpleJuno106AudioProcessor::toggleRecording()
{
    if (isRecording()) stopRecording();
    else startRecording();
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::startRecording()
{
    stopRecording();
    
    // 1. Create temporary file
    tempRecordingFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("junio_rec_tmp", ".wav");
    
    auto options = juce::AudioFormatWriterOptions()
                   .withSampleRate (getSampleRate())
                   .withNumChannels (2)
                   .withBitsPerSample (32);

    std::unique_ptr<juce::OutputStream> fileStream (tempRecordingFile.createOutputStream());
    if (fileStream != nullptr) {
        juce::WavAudioFormat wavFormat;
        if (auto writer = wavFormat.createWriterFor(fileStream, options)) {
            const juce::ScopedLock sl(writerLock);
            threadedWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (writer.release(), backgroundThread, 32768));
            juce::Logger::writeToLog("[JUNiO] Recording started (32-bit float): " + tempRecordingFile.getFullPathName());
        }
    }
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::stopRecording()
{
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writerToDestroy;
    {
        const juce::ScopedLock sl(writerLock);
        writerToDestroy.reset(threadedWriter.release());
    }
    
    if (writerToDestroy != nullptr) {
        // Wait for background thread to flush and destroy
        writerToDestroy.reset(); 
        juce::Logger::writeToLog("[JUNiO] Recording stopped. Finalizing file...");

        // Trigger File Chooser for official save
        juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        juce::String defaultName = "junio601_" + timestamp + ".wav";
        
        fileChooser = std::make_unique<juce::FileChooser>("Save Synthesizer Recording",
                                                          juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(defaultName),
                                                          "*.wav");
        
        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.getFullPathName().isNotEmpty()) {
                if (result.existsAsFile()) result.deleteFile();
                tempRecordingFile.moveFileTo(result);
                juce::Logger::writeToLog("[JUNiO] Recording saved to: " + result.getFullPathName());
            } else {
                tempRecordingFile.deleteFile();
                juce::Logger::writeToLog("[JUNiO] Recording discarded by user.");
            }
        });
    }
}

//==============================================================================
// Panic / LFO Trigger / Test Mode
//==============================================================================
void ABDSimpleJuno106AudioProcessor::triggerPanic()
{
    panicRequested.store(true);
    midiOutBuffer.addEvent(juce::MidiMessage::allNotesOff(1), 0);
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::triggerLFO()
{
    if (fmtLfoTrig) fmtLfoTrig->store(1.0f);
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::enterTestMode(bool enter)
{
    isTestMode = enter;
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::triggerTestProgram(int bankIndex)
{
    #if JUCE_DEBUG
    if (bankIndex == 99) {
        if (presetManager) JunoTests::runJunoPatchRoundtripTest(*presetManager);
        return;
    }
    if (bankIndex == 98) {
        JunoTests::runSysExPatchDumpRoundtripTest();
        return;
    }
    if (bankIndex == 97) {
        if (presetManager) JunoTests::runPresetJsonRoundtripTest(*presetManager);
        return;
    }
    #endif

    if (!isTestMode || bankIndex < 0 || bankIndex >= 8) return;
    const auto prog = getTestProgram(bankIndex);
    auto setVal = [&](juce::String id, float val) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(val); };
    auto setInt = [&](juce::String id, int val) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1((float)val)); };
    auto setBool = [&](juce::String id, bool val) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(val ? 1.0f : 0.0f); };
    setVal("lfoRate", prog.lfoRate); setVal("lfoDelay", prog.lfoDelay); setVal("lfoToDCO", prog.lfoToDCO);
    setInt("dcoRange", prog.dcoRange); setBool("sawOn", prog.sawOn); setBool("pulseOn", prog.pulseOn);
    setVal("pwm", prog.pwm); setInt("pwmMode", prog.pwmMode); setVal("subOsc", prog.subOsc); setVal("noise", prog.noise);
    setInt("hpfFreq", prog.hpfFreq); setVal("vcfFreq", prog.vcfFreq); setVal("resonance", prog.resonance);
    setVal("envAmount", prog.envAmount); setVal("lfoToVCF", prog.lfoToVCF); setVal("kybdTracking", prog.kybdTracking);
    setInt("vcfPolarity", prog.vcfPolarity); setInt("vcaMode", prog.vcaMode); setVal("vcaLevel", prog.vcaLevel);
    setVal("attack", prog.attack); setVal("decay", prog.decay); setVal("sustain", prog.sustain); setVal("release", prog.release);
    setBool("chorus1", prog.chorus1); setBool("chorus2", prog.chorus2);
}

//==============================================================================
int ABDSimpleJuno106AudioProcessor::getWipCount() const
{
    if (presetManager)
    {
        int wipIdx = presetManager->getLibraryIndex("WIP");
        if (wipIdx >= 0)
        {
            const auto& lib = presetManager->getLibrary(wipIdx);
            return (int)lib.patches.size();
        }
    }
    return 0; 
}
