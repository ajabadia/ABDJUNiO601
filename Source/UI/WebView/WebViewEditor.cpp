#include "WebViewEditor.h"
#include "../../Core/PresetManager.h"
#include "BinaryData.h"
#include <memory>
#include <optional>
#include <string>
#include <cstddef>
#include <cstring>



WebViewEditor::WebViewEditor (SimpleJuno106AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Setup visual debug: No more persistence in Documents folder at user's request.
    auto resolveUserDataDir = []() -> juce::File {
        auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("JUNiO601_WebView_Cache");
        f.createDirectory();
        return f;
    };

    // Setup WebView with ResourceProvider to serve BinaryData
    auto options = juce::WebBrowserComponent::Options()
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2()
                                  .withUserDataFolder (resolveUserDataDir())
                                  .withBackgroundColour (juce::Colours::black))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
        {
            auto path = url.fromFirstOccurrenceOf ("/", false, false);
            if (path == "/" || path.isEmpty()) path = "/index.html";
            
            // Map path to BinaryData name (e.g. /css/vars.css -> vars_css)
            juce::String resourceName = path.substring(path.lastIndexOf("/") + 1)
                                            .replace(".", "_")
                                            .replace("-", "_")
                                            .replace(" ", "_");
            
            int size = 0;
            if (auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), size))
            {
                auto getMimeType = [] (const juce::String& p)
                {
                    if (p.endsWithIgnoreCase (".html")) return "text/html";
                    if (p.endsWithIgnoreCase (".css"))  return "text/css";
                    if (p.endsWithIgnoreCase (".js"))   return "application/javascript";
                    if (p.endsWithIgnoreCase (".png"))  return "image/png";
                    if (p.endsWithIgnoreCase (".ttf"))  return "font/ttf";
                    if (p.endsWithIgnoreCase (".woff")) return "font/woff";
                    if (p.endsWithIgnoreCase (".woff2")) return "font/woff2";
                    return "application/octet-stream";
                };

                std::vector<std::byte> dataVec (size);
                std::memcpy (dataVec.data(), data, (size_t)size);

                return juce::WebBrowserComponent::Resource {
                    std::move (dataVec),
                    getMimeType (path)
                };
            }
            return std::nullopt;
        });

    // Native Functions Bridge
    options = options.withNativeFunction ("setParameter", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) 
    {
        if (args.size() == 2)
        {
            auto paramID = args[0].toString();
            float value = static_cast<float>(args[1]);
            if (auto* p = audioProcessor.getAPVTS().getParameter (paramID)) {
                p->setValueNotifyingHost (value);
                audioProcessor.paramsAreDirty.store(true);
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
                if (auto* pm = audioProcessor.getPresetManager()) {
                    pm->randomizeCurrentParameters(audioProcessor.getAPVTS());
                    audioProcessor.paramsAreDirty.store(true);
                    audioProcessor.patchDumpRequested.store(true);
                }
            }
            else if (action == "handleManual") audioProcessor.sendManualMode();
            else if (action == "handleTest") audioProcessor.enterTestMode(true);
            else if (action == "handleLoad") {
                fileChooser = std::make_unique<juce::FileChooser> ("Load Patch (.jno, .syx)...", juce::File(), "*.jno;*.syx");
                fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager()) {
                            if (pm->importPresetsFromFile (fc.getResult()).wasOk()) {
                                audioProcessor.paramsAreDirty.store(true);
                                audioProcessor.patchDumpRequested.store(true);
                            }
                        }
                    });
            }
            else if (action == "handleSave") {
                fileChooser = std::make_unique<juce::FileChooser> ("Save Patch (.jno)...", juce::File(), "*.jno");
                fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager())
                            pm->exportCurrentPresetToJson (fc.getResult());
                    });
            }
            else if (action == "handleExportBank") {
                fileChooser = std::make_unique<juce::FileChooser> ("Export Bank (.json)...", juce::File(), "*.json");
                fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager())
                            pm->exportLibraryToJson (fc.getResult());
                    });
            }
            else if (action == "handleImportSysex") {
                fileChooser = std::make_unique<juce::FileChooser> ("Import SysEx (.syx, .jno)...", juce::File(), "*.syx;*.jno");
                fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager()) {
                            if (pm->importPresetsFromFile (fc.getResult()).wasOk()) {
                                audioProcessor.paramsAreDirty.store(true);
                                audioProcessor.patchDumpRequested.store(true);
                            }
                        }
                    });
            }
            else if (action == "handleLoadTape") {
                fileChooser = std::make_unique<juce::FileChooser> ("Load Tape (.wav)...", juce::File(), "*.wav");
                fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager()) {
                            if (pm->loadTape (fc.getResult()).wasOk()) {
                                audioProcessor.paramsAreDirty.store(true);
                                audioProcessor.patchDumpRequested.store(true);
                            }
                        }
                    });
            }
            else if (action == "handleSaveTape") {
                fileChooser = std::make_unique<juce::FileChooser> ("Save Tape (.wav)...", juce::File(), "*.wav");
                fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                    [this] (const juce::FileChooser& fc) {
                        if (auto* pm = audioProcessor.getPresetManager())
                            pm->exportCurrentPresetToTape (fc.getResult());
                    });
            }
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
            else if (action == "handleLfoTrig") audioProcessor.triggerLFO();
            else if (action == "exit") {
                if (auto* app = juce::JUCEApplication::getInstance())
                    app->systemRequestedQuit();
            }
            else if (action == "handleSettings") {
                dispatchToJS("showModal", "preferences");
            }
            else if (action == "handleAbout") {
                dispatchToJS("showModal", "about");
            }
            else if (action == "handleLoadTuning") {
                audioProcessor.loadTuningFile();
            }
            else if (action == "handleResetTuning") {
                audioProcessor.resetTuning();
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
        for (auto& p : audioProcessor.getParameters())
        {
            if (auto* param = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
                updateParameterInJS (param->paramID, param->getValue());
        }
        
        // [Audit Fix] Send initial patch dump on UI ready so mirror is initialized
        audioProcessor.sendPatchDump();
        
        completion (juce::var());
    });

    webComponent = std::make_unique<juce::WebBrowserComponent> (options);
    webComponent->setOpaque (true);
    addAndMakeVisible (*webComponent);
    
    if (webComponent != nullptr)
        webComponent->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // Register listeners
    const char* paramIDs[] = {
        "masterVolume", "lfoRate", "lfoDelay", "lfoToDCO", "pwm", "pwmMode",
        "hpfFreq", "vcfFreq", "resonance", "envAmount", "vcfPolarity", "kybdTracking", "lfoToVCF",
        "vcaMode", "vcaLevel", "attack", "decay", "sustain", "release", "chorus1", "chorus2",
        "benderToDCO", "benderToVCF", "benderToLFO", "portamentoTime", "portamentoOn", "portamentoLegato",
        "polyMode", "tune", "dcoRange", "sawOn", "pulseOn", "subOsc", "noise", "bender", "midiOut",
        "midiChannel", "benderRange", "velocitySens", "lcdBrightness", "numVoices", "sustainInverted",
        "chorusHiss", "midiFunction", "unisonWidth", "aftertouchToVCF"
    };
    for (auto id : paramIDs)
    {
        if (audioProcessor.getAPVTS().getParameter(id) != nullptr)
            audioProcessor.getAPVTS().addParameterListener (id, this);
    }

    startTimerHz(30); 
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colours::black);
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
        "midiChannel", "benderRange", "velocitySens", "lcdBrightness", "numVoices", "sustainInverted",
        "chorusHiss", "midiFunction", "unisonWidth", "aftertouchToVCF"
    };
    for (auto id : paramIDs)
        audioProcessor.getAPVTS().removeParameterListener (id, this);
}

void WebViewEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);
    if (auto* p = audioProcessor.getAPVTS().getParameter(parameterID)) {
        updateParameterInJS (parameterID, p->getValue());
    }
}

void WebViewEditor::updateParameterInJS (const juce::String& paramID, float value)
{
    if (webComponent != nullptr)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty ("id", paramID);
        obj->setProperty ("val", value);
        obj->setProperty ("value", value);
        dispatchToJS ("onParameterChanged", juce::var (obj.get()));
    }
}

void WebViewEditor::updateSysExInJS()
{
    auto msg = audioProcessor.getCurrentSysExData();
    if (msg.getRawDataSize() > 0)
    {
        if (msg.getRawDataSize() == lastSysEx.getRawDataSize() && 
            std::memcmp(msg.getRawData(), lastSysEx.getRawData(), msg.getRawDataSize()) == 0) 
            return;
        lastSysEx = msg;

        juce::String hex;
        hex.preallocateBytes(msg.getRawDataSize() * 3);
        for (int i=0; i < msg.getRawDataSize(); ++i)
             hex += juce::String::toHexString(msg.getRawData()[i]).toUpperCase().paddedLeft('0', 2) + " ";
        
        if (webComponent != nullptr)
            dispatchToJS ("onSysExUpdate", { hex.trim() });
    }
}

void WebViewEditor::updateLCDInJS(const juce::String& text)
{
    if (webComponent != nullptr)
        dispatchToJS ("onLCDUpdate", { text });
}

void WebViewEditor::timerCallback()
{
    updateSysExInJS();
    
    auto* pm = audioProcessor.getPresetManager();
    if (pm && pm->getCurrentPresetIndex() != lastPresetIndex)
    {
        lastPresetIndex = pm->getCurrentPresetIndex();
        updateLCDInJS("P: " + juce::String(lastPresetIndex + 1) + " " + pm->getCurrentPresetName());
        
        int bank = (lastPresetIndex / 8) + 1;
        int patch = (lastPresetIndex % 8) + 1;
        
        juce::DynamicObject::Ptr bpObj = new juce::DynamicObject();
        bpObj->setProperty ("bank", bank);
        bpObj->setProperty ("patch", patch);
        dispatchToJS ("onBankPatchUpdate", juce::var (bpObj.get()));
    }

    if (audioProcessor.getCurrentTuningName() != lastTuningName)
    {
        lastTuningName = audioProcessor.getCurrentTuningName();
        dispatchToJS ("onTuningUpdate", lastTuningName);
    }

    if (webComponent != nullptr)
    {
        juce::DynamicObject::Ptr visObj = new juce::DynamicObject();
        visObj->setProperty ("c1", audioProcessor.getChorusLfoPhase(1));
        visObj->setProperty ("c2", audioProcessor.getChorusLfoPhase(2));
        dispatchToJS ("onVisualUpdate", juce::var (visObj.get()));
    }
}

void WebViewEditor::dispatchToJS (const juce::Identifier& eventId, const juce::var& payload)
{
    if (webComponent != nullptr)
    {
        // [Fix] Foolproof event dispatch using evaluateJavaScript if emitEvent is missing/broken
        juce::String js = "if(window.juce && window.juce.onEvent) window.juce.onEvent('" 
                          + eventId.toString() + "', " + juce::JSON::toString(payload) + "); "
                          + "else if(window.onJuceEvent) window.onJuceEvent('" 
                          + eventId.toString() + "', " + juce::JSON::toString(payload) + ");";
        
        webComponent->evaluateJavascript(js);
    }
}

void WebViewEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }

void WebViewEditor::resized()
{
    if (webComponent != nullptr)
        webComponent->setBounds (getLocalBounds());
}
