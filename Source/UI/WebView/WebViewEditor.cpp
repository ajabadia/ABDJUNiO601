#include "WebViewEditor.h"
#include "../../Core/PresetManager.h"
#include <BinaryData.h>

WebViewEditor::WebViewEditor (SimpleJuno106AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), lastLibraryIndex(-1)
{
    auto resolveLogDir = []() -> juce::File {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("JUNiO601").getChildFile("logs").getChildFile("webview_data");
    };

    const auto getMimeType = [] (const juce::String& path) -> juce::String
    {
        if (path.endsWith (".html")) return "text/html";
        if (path.endsWith (".css"))  return "text/css";
        if (path.endsWith (".js"))   return "text/javascript";
        if (path.endsWith (".png"))  return "image/png";
        if (path.endsWith (".ttf"))  return "font/ttf";
        if (path.endsWith (".woff")) return "font/woff";
        if (path.endsWith (".woff2")) return "font/woff2";
        if (path.endsWith (".svg"))  return "image/svg+xml";
        return "application/octet-stream";
    };

    // Setup WebView with Options
    auto options = juce::WebBrowserComponent::Options()
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2()
                                  .withUserDataFolder (resolveLogDir())
                                  .withBackgroundColour (juce::Colour (0xff111111))) // Layer 2: Windows specific
        .withNativeIntegrationEnabled()
        .withResourceProvider ([getMimeType] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource> 
        {
            auto path = url == "/" ? "index.html" : url.substring (1);
            juce::String filename = juce::File (path).getFileName();
            
            // Robust lookup: iterate through BinaryData to find the original filename
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                if (filename == BinaryData::originalFilenames[i])
                {
                    int dataSize = 0;
                    if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], dataSize))
                    {
                        std::vector<std::byte> dataVec (static_cast<size_t> (dataSize));
                        std::memcpy (dataVec.data(), data, static_cast<size_t> (dataSize));
                        return juce::WebBrowserComponent::Resource { std::move (dataVec), getMimeType (path) };
                    }
                }
            }

            juce::Logger::writeToLog ("WebView resource NOT FOUND in BinaryData: " + path + " (looked for filename: " + filename + ")");
            return std::nullopt;
        });

    // Native Functions Bridge
    options = options.withNativeFunction ("setParameter", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) 
    {
        if (args.size() == 2)
        {
            auto paramID = args[0].toString();
            float value = static_cast<float>(args[1]);
            
            juce::Logger::writeToLog ("JS -> setParameter: " + paramID + " = " + juce::String(value));

            if (auto* p = audioProcessor.getAPVTS().getParameter (paramID))
                p->setValueNotifyingHost (value);
            else
                juce::Logger::writeToLog ("WARNING: Parameter NOT FOUND: " + paramID);
            
            // Special LCD feedback for Manual interaction
            if (auto* p = audioProcessor.getAPVTS().getParameter (paramID))
            {
                auto label = p->getName(32) + ": " + p->getCurrentValueAsText();
                updateLCDInJS(label);
            }
        }
        completion (juce::var());
    });

    options = options.withNativeFunction ("pianoNoteOn", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) 
    {
        if (args.size() >= 1) {
            int note = (int)args[0];
            float vel = args.size() > 1 ? (float)args[1] : 0.8f;
            audioProcessor.keyboardState.noteOn(1, note, vel);
        }
        completion (juce::var());
    });

    options = options.withNativeFunction ("pianoNoteOff", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) 
    {
        if (args.size() >= 1) {
            int note = (int)args[0];
            audioProcessor.keyboardState.noteOff(1, note, 0.0f);
        }
        completion (juce::var());
    });

    options = options.withNativeFunction ("loadPreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) 
    {
        if (args.size() == 1) {
            int idx = (int)args[0];
            audioProcessor.loadPreset(idx);
        }
        completion (juce::var());
    });

    options = options.withNativeFunction ("menuAction", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) 
    {
        if (args.size() == 1) {
            juce::String action = args[0].toString();
            if (action == "panic") audioProcessor.triggerPanic();
            else if (action == "handleRandomize") {
                if (auto* pm = audioProcessor.getPresetManager())
                    pm->randomizeCurrentParameters(audioProcessor.getAPVTS());
            }
            else if (action == "handleManual") audioProcessor.sendManualMode();
            else if (action == "handleTest") audioProcessor.enterTestMode(true);
            else if (action == "handleTestProgram") {
                if (args.size() > 0) audioProcessor.triggerTestProgram(static_cast<int>(args[0]));
            }
            else if (action == "handleLfoTrig") audioProcessor.triggerLFO();
            else if (action == "handleBankInc") {
                if (auto* pm = audioProcessor.getPresetManager())
                    audioProcessor.loadPreset(juce::jlimit(0, 127, pm->getCurrentPresetIndex() + 8));
            }
            else if (action == "handleBankDec") {
                if (auto* pm = audioProcessor.getPresetManager())
                    audioProcessor.loadPreset(juce::jlimit(0, 127, pm->getCurrentPresetIndex() - 8));
            }
            else if (action == "handlePatchInc") {
                if (auto* pm = audioProcessor.getPresetManager())
                    audioProcessor.loadPreset(juce::jlimit(0, 127, pm->getCurrentPresetIndex() + 1));
            }
            else if (action == "handlePatchDec") {
                if (auto* pm = audioProcessor.getPresetManager())
                    audioProcessor.loadPreset(juce::jlimit(0, 127, pm->getCurrentPresetIndex() - 1));
            }
            else if (action == "undo") audioProcessor.undo();
            else if (action == "redo") audioProcessor.redo();
            else if (action == "toggleMidiOut") audioProcessor.toggleMidiOut();
            else if (action == "panic") audioProcessor.triggerPanic();
            else if (action == "handleSettings") emitJS ("onShowSettings", juce::var());
            else if (action == "handleExportBank") {
                fileChooser = std::make_unique<juce::FileChooser> ("Export Bank...", juce::File(), "*.json");
                fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager())
                            pm->exportAllLibrariesToJson (fc.getResult());
                    });
            }
            else if (action == "handleImportSysex") {
                fileChooser = std::make_unique<juce::FileChooser> ("Import SysEx / JNO...", juce::File(), "*.syx;*.jno");
                fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager()) {
                            pm->importPresetsFromFile (fc.getResult());
                            audioProcessor.loadPreset(0); 
                        }
                    });
            }
            else if (action == "handleLoadTape") {
                fileChooser = std::make_unique<juce::FileChooser> ("Load Tape (.wav)...", juce::File(), "*.wav");
                fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager()) {
                            pm->loadTape (fc.getResult());
                            audioProcessor.loadPreset(0); // Load first patch
                        }
                    });
            }
            else if (action == "exit") {
                if (auto* app = juce::JUCEApplication::getInstance())
                    app->systemRequestedQuit();
            }
            else if (action == "handleAbout") {
                emitJS ("onShowAbout", {});
            }
            else if (action == "handleLoad") {
                fileChooser = std::make_unique<juce::FileChooser> ("Load Patch...", juce::File(), "*.json;*.syx;*.jno");
                fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager())
                            pm->importPresetsFromFile (fc.getResult());
                    });
            }
            else if (action == "handleSave") {
                fileChooser = std::make_unique<juce::FileChooser> ("Save Patch...", juce::File(), "*.json;*.syx;*.jno");
                fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager())
                            pm->exportCurrentPresetToJson (fc.getResult());
                    });
            }
        }
        completion (juce::var());
    });

    options = options.withNativeFunction ("getPresetList", [this] (const juce::Array<juce::var>& /*args*/, juce::WebBrowserComponent::NativeFunctionCompletion completion) 
    {
        juce::Array<juce::var> list;
        if (auto* pm = audioProcessor.getPresetManager())
        {
            auto names = pm->getPresetNames();
            for (auto& name : names)
                list.add (name);
        }
        completion (list);
    });

    options = options.withNativeFunction ("uiReady", [this] (const juce::Array<juce::var>& /*args*/, juce::WebBrowserComponent::NativeFunctionCompletion completion) 
    {
        juce::Logger::writeToLog ("WebUI is READY - Syncing all parameters...");
        
        for (auto& p : audioProcessor.getParameters())
        {
            if (auto* param = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
            {
                // Push all parameters to ensure JS state is correct
                updateParameterInJS (param->paramID, param->getValue());
            }
        }
        
        // Send initial LCD and 7-segment
        if (auto* pm = audioProcessor.getPresetManager())
        {
            updateLCDInJS ("P: " + juce::String (pm->getCurrentPresetIndex() + 1) + " " + pm->getCurrentPresetName());
            
            int bank = (pm->getCurrentPresetIndex() / 8) + 1;
            int patch = (pm->getCurrentPresetIndex() % 8) + 1;
            
            juce::DynamicObject::Ptr bpObj = new juce::DynamicObject();
            bpObj->setProperty ("bank", bank);
            bpObj->setProperty ("patch", patch);
            emitJS ("onBankPatchUpdate", juce::var (bpObj.get()));
        }

        completion (juce::var());
    });
    
    // Set background color for the WebView2 backend on Windows
    options = options.withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2()
                                              .withBackgroundColour (juce::Colour (0xff111111)));

    webView = std::make_unique<juce::WebBrowserComponent> (options);
    webView->setOpaque (true);
    addAndMakeVisible (*webView);
    webView->setVisible (true);
    webView->toFront (false);
    
    // Initial Load
    if (webView != nullptr)
        webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // Register listeners
    const char* paramIDs[] = {
        "masterVolume", "lfoRate", "lfoDelay", "lfoToDCO", "pwm", "pwmMode",
        "hpfFreq", "vcfFreq", "resonance", "envAmount", "vcfPolarity", "kybdTracking", "lfoToVCF",
        "vcaMode", "vcaLevel", "attack", "decay", "sustain", "release", "chorus1", "chorus2",
        "benderToDCO", "benderToVCF", "benderToLFO", "portamentoTime", "portamentoOn", "portamentoLegato",
        "polyMode", "tune", "dcoRange", "sawOn", "pulseOn", "subOsc", "noise", "bender", "midiOut",
        "midiChannel", "benderRange", "velocitySens", "lcdBrightness", "numVoices"
    };
    for (auto id : paramIDs)
    {
        if (audioProcessor.getAPVTS().getParameter(id) != nullptr)
            audioProcessor.getAPVTS().addParameterListener (id, this);
        else
            juce::Logger::writeToLog("CRITICAL: Failed to attach listener to " + juce::String(id));
    }

    startTimerHz(30); // 30Hz for LEDs and SysEx Monitoring
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff111111));
    setSize (1200, 750);
}

WebViewEditor::~WebViewEditor()
{
    const char* paramIDs[] = {
        "masterVolume", "lfoRate", "lfoDelay", "lfoToDCO", "pwm", "pwmMode",
        "hpfFreq", "vcfFreq", "resonance", "envAmount", "vcfPolarity", "kybdTracking", "lfoToVCF",
        "vcaMode", "vcaLevel", "attack", "decay", "sustain", "release", "chorus1", "chorus2",
        "benderToDCO", "benderToVCF", "benderToLFO", "portamentoTime", "portamentoOn", "portamentoLegato",
        "polyMode", "tune", "dcoRange", "sawOn", "pulseOn", "subOsc", "noise", "bender", "midiOut",
        "midiChannel", "benderRange", "velocitySens", "lcdBrightness", "numVoices"
    };
    for (auto id : paramIDs)
        audioProcessor.getAPVTS().removeParameterListener (id, this);
}

void WebViewEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);
    if (auto* p = audioProcessor.getAPVTS().getParameter(parameterID)) {
        // Use the normalized value directly from the parameter
        updateParameterInJS (parameterID, p->getValue());
    }
}

void WebViewEditor::updateParameterInJS (const juce::String& paramID, float value)
{
    if (webView != nullptr)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty ("id", paramID);
        obj->setProperty ("value", value);
        emitJS ("onParameterChanged", juce::var (obj.get()));
    }
}

void WebViewEditor::updateSysExInJS()
{
    auto msg = audioProcessor.getCurrentSysExData();
    if (msg.getRawDataSize() > 0)
    {
        juce::String hex;
        for (int i=0; i < msg.getRawDataSize(); ++i)
             hex += juce::String::toHexString(msg.getRawData()[i]).toUpperCase().paddedLeft('0', 2) + " ";
        
        if (webView != nullptr)
            emitJS ("onSysExUpdate", { hex.trim() });
    }
}

void WebViewEditor::updateLCDInJS(const juce::String& text)
{
    if (webView != nullptr)
        emitJS ("onLCDUpdate", { text });
}

void WebViewEditor::timerCallback()
{
    // Poll for SysEx changes
    updateSysExInJS();
    
    // Poll for Preset/Library changes to update LCD
    auto* pm = audioProcessor.getPresetManager();
    if (pm && (pm->getCurrentPresetIndex() != lastPresetIndex || pm->getActiveLibraryIndex() != lastLibraryIndex))
    {
        lastPresetIndex = pm->getCurrentPresetIndex();
        lastLibraryIndex = pm->getActiveLibraryIndex();
        updateLCDInJS("P: " + juce::String(lastPresetIndex + 1) + " " + pm->getCurrentPresetName());
        
        int bank = (lastPresetIndex / 8) + 1;
        int patch = (lastPresetIndex % 8) + 1;
        
        juce::DynamicObject::Ptr bpObj = new juce::DynamicObject();
        bpObj->setProperty ("bank", bank);
        bpObj->setProperty ("patch", patch);
        emitJS ("onBankPatchUpdate", juce::var (bpObj.get()));
    }

    // Chorus LEDs pulsing (single emission per tick)
    if (webView != nullptr)
    {
        juce::DynamicObject::Ptr visObj = new juce::DynamicObject();
        visObj->setProperty ("c1", audioProcessor.getChorusLfoPhase(1));
        visObj->setProperty ("c2", audioProcessor.getChorusLfoPhase(2));
        emitJS ("onVisualUpdate", juce::var (visObj.get()));
    }
}

void WebViewEditor::emitJS (const juce::Identifier& eventId, const juce::var& payload)
{
    if (webView != nullptr)
    {
        juce::String json = juce::JSON::toString (payload, true);
        juce::String escaped = json.replace ("\\", "\\\\").replace ("'", "\\'");
        webView->evaluateJavascript ("window.__JUCE__.backend.emitByBackend(" + eventId.toString().quoted() + ", "
                                    + escaped.quoted ('\'') + ");");
    }
}

void WebViewEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }

void WebViewEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}
