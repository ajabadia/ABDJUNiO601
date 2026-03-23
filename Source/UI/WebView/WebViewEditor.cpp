#include <JuceHeader.h>
#include "WebViewEditor.h"
#include "../../Core/PluginProcessor.h"
#include "../../Core/CalibrationManager.h"
#include "../../Core/ServiceModeManager.h"
#include "../../Core/PresetManager.h"
#include "../../Core/BuildVersion.h"
#include <optional>

using namespace juce;

WebViewEditor::WebViewEditor (SimpleJuno106AudioProcessor& p)
    : juce::AudioProcessorEditor (&p), audioProcessor (p)
{
    auto options = juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withInitialisationData ("buildVersion", juce::String(JUNO_BUILD_VERSION))
        .withInitialisationData ("buildTimestamp", juce::String(JUNO_BUILD_TIMESTAMP))
        .withNativeIntegrationEnabled (true)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2()
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ABD_JUNiO_601_WebView2")))
        .withResourceProvider ([this] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource> {
            juce::String path = url;
            if (path == "/" || path.isEmpty()) path = "/index.html";
            if (path.startsWith("/")) path = path.substring(1);
            
            juce::File webUiDir = juce::File("d:\\desarrollos\\ABDJUNiO601\\Source\\UI\\WebUI");
            if (!webUiDir.exists()) {
                juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
                webUiDir = exeDir.getChildFile("WebUI");
            }

            juce::File file = webUiDir.getChildFile(path.replace("/", "\\"));
            
            auto getMimeType = [](const juce::String& filename) {
                if (filename.endsWithIgnoreCase(".html")) return "text/html";
                if (filename.endsWithIgnoreCase(".css"))  return "text/css";
                if (filename.endsWithIgnoreCase(".js"))   return "application/javascript";
                if (filename.endsWithIgnoreCase(".png"))  return "image/png";
                if (filename.endsWithIgnoreCase(".svg"))  return "image/svg+xml";
                if (filename.endsWithIgnoreCase(".ttf"))  return "font/ttf";
                if (filename.endsWithIgnoreCase(".woff")) return "font/woff";
                if (filename.endsWithIgnoreCase(".woff2")) return "font/woff2";
                return "application/octet-stream";
            };

            if (file.existsAsFile())
            {
                juce::MemoryBlock mb;
                file.loadFileAsData(mb);
                std::vector<std::byte> data(mb.getSize());
                std::memcpy(data.data(), mb.getData(), mb.getSize());
                return juce::WebBrowserComponent::Resource { std::move(data), getMimeType(file.getFileName()) };
            }

            auto getResourceFromBinary = [getMimeType] (const char* data, int size, const juce::String& filename) -> std::optional<juce::WebBrowserComponent::Resource> {
                if (data == nullptr) return std::nullopt;
                std::vector<std::byte> bytes (size);
                std::memcpy (bytes.data(), data, (size_t)size);
                return juce::WebBrowserComponent::Resource { std::move (bytes), getMimeType(filename) };
            };

            if (path == "index.html")   return getResourceFromBinary (BinaryData::index_html, BinaryData::index_htmlSize, "index.html");
            if (path == "script.js")    return getResourceFromBinary (BinaryData::script_js, BinaryData::script_jsSize, "script.js");
            if (path == "service.js")   return getResourceFromBinary (BinaryData::service_js, BinaryData::service_jsSize, "service.js");
            if (path == "service.css")  return getResourceFromBinary (BinaryData::service_css, BinaryData::service_cssSize, "service.css");
            if (path == "style.css")    return getResourceFromBinary (BinaryData::style_css, BinaryData::style_cssSize, "style.css");

            juce::String resourceName = path.replace("/", "_").replace(".", "_").replace("-", "_").replace(" ", "_");
            int binSize = 0;
            const char* binData = BinaryData::getNamedResource(resourceName.toRawUTF8(), binSize);
            if (binData != nullptr) return getResourceFromBinary(binData, binSize, path);

            return std::nullopt;
        })
        .withNativeFunction ("setParameter", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) {
                juce::String paramID = args[0].toString();
                float val = (float)args[1];
                juce::Logger::writeToLog("[JUNiO] setParameter JS CALL: ID=" + paramID + ", Value=" + juce::String(val));
                if (auto* param = audioProcessor.getAPVTS().getParameter(paramID))
                {
                    param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float)val));
                    completion (juce::var::undefined());
                }
                else
                {
                    juce::Logger::writeToLog("[JUNiO] setParameter ERROR: Parameter not found: " + paramID);
                    completion({});
                }
            }
            else
            {
                completion({});
            }
        })
        .withNativeFunction ("getCalibrationParams", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::Array<juce::var> result;
            auto& manager = audioProcessor.getCalibrationManager();
            for (const auto& p : manager.getAllParams()) {
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("id", p.id);
                obj->setProperty("label", p.label);
                obj->setProperty("unit", p.unit);
                obj->setProperty("tooltip", p.tooltip);
                obj->setProperty("defaultValue", (double)p.defaultValue);
                obj->setProperty("currentValue", (double)p.currentValue);
                obj->setProperty("minValue", (double)p.minValue);
                obj->setProperty("maxValue", (double)p.maxValue);
                obj->setProperty("stepSize", (double)p.stepSize);
                result.add(juce::var(obj.get()));
            }
            completion(result);
        })
        .withNativeFunction ("setCalibrationParam", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) {
                juce::String id = args[0].toString();
                float val = (float)args[1];
                audioProcessor.getCalibrationManager().setValue(id, val);
            }
            completion({});
        })
        .withNativeFunction ("serviceAction", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                auto obj = args[0].getDynamicObject();
                if (obj != nullptr) {
                    juce::String action = obj->getProperty("action").toString();
                    auto& smm = audioProcessor.getServiceModeManager();
                    if (action == "testVoice") smm.setVoiceSolo((int)obj->getProperty("voice"));
                    else if (action == "stopVoiceTest") smm.clearVoiceSolo();
                    else if (action == "sweepVCF") smm.startVCFSweep();
                    else if (action == "playTestScale") {
                        if (smm.isTestScaleActive()) smm.stopAllTests();
                        else smm.startTestScale();
                    }
                    else if (action == "resetToFactory") audioProcessor.getCalibrationManager().resetToDefaults();
                    else if (action == "exportCalibration") audioProcessor.getCalibrationManager().save();
                }
            }
            completion({});
        })
        .withNativeFunction ("menuAction", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                juce::String action = args[0].toString();
                if (action == "panic") audioProcessor.triggerPanic();
                else if (action == "undo") audioProcessor.undo();
                else if (action == "redo") audioProcessor.redo();
                else if (action == "toggleMidiOut") audioProcessor.toggleMidiOut();
                else if (action == "handleManual") audioProcessor.sendManualMode();
                else if (action == "handleRandomize") {
                    if (auto* pm = audioProcessor.getPresetManager()) {
                        pm->randomizeCurrentParameters(audioProcessor.getAPVTS());
                    }
                }
                else if (action == "handleAbout") showAboutCallback();
                else if (action == "handleSettings") showSettingsCallback();
                else if (action == "handleServiceMode") showServiceModeCallback();
                else if (action == "exit") {
                    if (auto* app = juce::JUCEApplication::getInstance())
                        app->systemRequestedQuit();
                }
                else if (action == "handleLoad" || action == "handleImportSysex") {
                    fileChooser = std::make_unique<juce::FileChooser>("Load patch or bank...", 
                        juce::File(audioProcessor.getPresetManager()->getLastPath()),
                        "*.jno;*.syx;*.wav;*.bin");
                    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& chooser) {
                            auto result = chooser.getResult();
                            if (result.existsAsFile()) {
                                audioProcessor.getPresetManager()->importPresetsFromFile(result);
                                audioProcessor.requestPatchDump();
                            }
                        });
                }
                else if (action == "handleSave" || action == "handleExportBank") {
                    fileChooser = std::make_unique<juce::FileChooser>("Save patch...", 
                        juce::File(audioProcessor.getPresetManager()->getLastPath()),
                        "*.jno;*.json");
                    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& chooser) {
                            auto result = chooser.getResult();
                            if (result.exists()) { // allow overwrite
                                audioProcessor.getPresetManager()->exportCurrentPresetToJson(result);
                            } else if (result != juce::File()) {
                                audioProcessor.getPresetManager()->exportCurrentPresetToJson(result.withFileExtension(".jno"));
                            }
                        });
                }
                else if (action == "handleLoadTape") {
                    fileChooser = std::make_unique<juce::FileChooser>("Load Tape (.wav)...", 
                        juce::File(audioProcessor.getPresetManager()->getLastPath()),
                        "*.wav");
                    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& chooser) {
                            if (chooser.getResults().size() > 0)
                                audioProcessor.getPresetManager()->loadTape(chooser.getResults()[0]);
                        });
                }
                else if (action == "handleSaveTape") {
                    fileChooser = std::make_unique<juce::FileChooser>("Save Tape (.wav)...", 
                        juce::File(audioProcessor.getPresetManager()->getLastPath()),
                        "*.wav");
                    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& chooser) {
                            auto result = chooser.getResult();
                            if (result != juce::File()) {
                                audioProcessor.getPresetManager()->exportCurrentPresetToTape(result.withFileExtension(".wav"));
                            }
                        });
                }
                else if (action == "bank-inc" || action == "handleBankInc") audioProcessor.setCurrentProgram(std::min(127, audioProcessor.getCurrentProgram() + 8));
                else if (action == "bank-dec" || action == "handleBankDec") audioProcessor.setCurrentProgram(std::max(0, audioProcessor.getCurrentProgram() - 8));
                else if (action == "patch-inc" || action == "handlePatchInc") audioProcessor.setCurrentProgram(std::min(127, audioProcessor.getCurrentProgram() + 1));
                else if (action == "patch-dec" || action == "handlePatchDec") audioProcessor.setCurrentProgram(std::max(0, audioProcessor.getCurrentProgram() - 1));
            }
            completion({});
        })
        .withNativeFunction ("loadPreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) audioProcessor.loadPreset((int)args[0]);
            completion({});
        })
        .withNativeFunction ("copySysExData", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) juce::SystemClipboard::copyTextToClipboard(args[0].toString());
            completion({});
        })
        .withNativeFunction ("pianoNoteOn", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) audioProcessor.keyboardState.noteOn(1, (int)args[0], (float)args[1]);
            completion({});
        })
        .withNativeFunction ("pianoNoteOff", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) audioProcessor.keyboardState.noteOff(1, (int)args[0], 0.0f);
            completion({});
        })
        .withNativeFunction ("uiReady", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::Logger::writeToLog("[JUNiO] JS UI is Ready. Sending Initial State...");
            
            // Mandatory completion for JUCE 8
            completion (juce::var::undefined());

            // Use a small delay to ensure JS listeners are fully active
            juce::Timer::callAfterDelay(100, [this]() {
                juce::Logger::writeToLog("[JUNiO] Sending delayed state dump...");
                
                // 1. Version Info (Build 18: Correct formatting and value)
                juce::String versionStr = "1.2.0 (Build " + juce::String(JUNO_BUILD_VERSION) + ")";
                dispatchToJS ("onVersionUpdate", versionStr);
                
                int prog = audioProcessor.getCurrentProgram();
                dispatchToJS ("onLCDUpdate", audioProcessor.getProgramName(prog));
                
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("bank", (prog / 8) + 1);
                obj->setProperty("patch", (prog % 8) + 1);
                dispatchToJS("onBankPatchUpdate", juce::var(obj.get()));

                // Initial parameter sync
                for (auto* param : audioProcessor.getParameters()) {
                    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
                        updateParameterInJS(p->getParameterID(), p->getValue());
                    }
                }
                
                // Sync lastParams immediately
                audioProcessor.updateParamsFromAPVTS();
                audioProcessor.requestPatchDump();
                
                juce::Logger::writeToLog("[JUNiO] Initial State Sync Complete");
            });
        })
;

    webComponent = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (webComponent.get());
    webComponent->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // Listen to parameter changes
    const juce::StringArray paramIDs = {
        "masterVolume", "lfoRate", "lfoDelay", "lfoToDCO", "pwm", "pwmMode",
        "hpfFreq", "vcfFreq", "resonance", "envAmount", "vcfPolarity", "kybdTracking", "lfoToVCF",
        "vcaMode", "vcaLevel", "attack", "decay", "sustain", "release", "chorus1", "chorus2",
        "benderToDCO", "benderToVCF", "benderToLFO", "portamentoTime", "portamentoOn", "portamentoLegato",
        "polyMode", "tune", "dcoRange", "sawOn", "pulseOn", "subOsc", "noise", "bender", "midiOut",
        "midiChannel", "benderRange", "velocitySens", "lcdBrightness", "numVoices", "sustainInverted",
        "chorusHiss", "midiFunction", "unisonWidth", "aftertouchToVCF"
    };
    for (int i = 0; i < paramIDs.size(); ++i)
        audioProcessor.getAPVTS().addParameterListener (paramIDs[i], this);

    startTimerHz(30); 
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colours::black);
    setSize (1200, 750);
}

WebViewEditor::~WebViewEditor()
{
    const juce::StringArray paramIDs = {
        "masterVolume", "lfoRate", "lfoDelay", "lfoToDCO", "pwm", "pwmMode",
        "hpfFreq", "vcfFreq", "resonance", "envAmount", "vcfPolarity", "kybdTracking", "lfoToVCF",
        "vcaMode", "vcaLevel", "attack", "decay", "sustain", "release", "chorus1", "chorus2",
        "benderToDCO", "benderToVCF", "benderToLFO", "portamentoTime", "portamentoOn", "portamentoLegato",
        "polyMode", "tune", "dcoRange", "sawOn", "pulseOn", "subOsc", "noise", "bender", "midiOut",
        "midiChannel", "benderRange", "velocitySens", "lcdBrightness", "numVoices", "sustainInverted",
        "chorusHiss", "midiFunction", "unisonWidth", "aftertouchToVCF"
    };
    for (int i = 0; i < paramIDs.size(); ++i)
        audioProcessor.getAPVTS().removeParameterListener (paramIDs[i], this);
}

void WebViewEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // [Build 18] Ensure engine refreshes its mirror parameters
    audioProcessor.paramsAreDirty.store(true);
    
    // [Build 21] Always normalize to 0..1 for UI sliders
    float normalizedValue = newValue;
    if (auto* param = audioProcessor.getAPVTS().getParameter(parameterID)) {
        normalizedValue = param->getValue(); // AudioProcessorParameter::getValue() is always 0..1
    }
    
    updateParameterInJS (parameterID, normalizedValue);
}

void WebViewEditor::updateParameterInJS (const juce::String& paramID, float value)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", paramID);
    obj->setProperty("value", value);
    dispatchToJS("onParameterChanged", juce::var(obj.get()));
}

void WebViewEditor::updateSysExInJS()
{
    auto msg = audioProcessor.getCurrentSysExData();
    if (msg.getRawDataSize() > 0 && msg.getRawData() != lastSysEx.getRawData()) {
        juce::String hex;
        for (int i=0; i < msg.getRawDataSize(); ++i) {
            hex += juce::String::toHexString((int)msg.getRawData()[i]).toUpperCase().paddedLeft('0', 2);
            if (i < msg.getRawDataSize() - 1) hex += " ";
        }
        dispatchToJS("onSysExUpdate", hex);
        lastSysEx = msg;
    }
}

void WebViewEditor::timerCallback()
{
    updateSysExInJS();

    // Monitor for Preset/Index changes that might happen internally
    if (auto* pm = audioProcessor.getPresetManager()) {
        int currentIdx = pm->getCurrentPresetIndex();
        if (currentIdx != lastPresetIndex) {
            lastPresetIndex = currentIdx;
            
            juce::DynamicObject::Ptr bp = new juce::DynamicObject();
            bp->setProperty("bank", (currentIdx / 8) + 1);
            bp->setProperty("patch", (currentIdx % 8) + 1);
            dispatchToJS("onBankPatchUpdate", juce::var(bp.get()));
            
            dispatchToJS("onLCDUpdate", pm->getCurrentPresetName());
        }
    }

    // Monitor Tuning Title changes
    juce::String currentTuning = audioProcessor.getCurrentTuningName();
    if (currentTuning != lastTuningName) {
        lastTuningName = currentTuning;
        dispatchToJS("onTuningUpdate", currentTuning);
    }
}

void WebViewEditor::dispatchToJS(const juce::Identifier& eventId, const juce::var& payload)
{
    if (webComponent) {
        juce::Logger::writeToLog("[JUNiO] Dispatching event: " + eventId.toString());

        // 1. Emit direct event (JUCE 8 Style)
        webComponent->emitEventIfBrowserIsVisible(eventId.toString(), payload);

        // 2. Fallbacks for older/different JS listeners
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("id", eventId.toString());
        obj->setProperty("payload", payload);
        webComponent->emitEventIfBrowserIsVisible("hostEvent", juce::var(obj.get()));
        
        juce::Array<juce::var> eventParams;
        eventParams.add(eventId.toString());
        eventParams.add(payload);
        webComponent->emitEventIfBrowserIsVisible("onJuceEvent", eventParams);
    }
}

void WebViewEditor::showAboutCallback()       { dispatchToJS ("onShowAbout", {}); }
void WebViewEditor::showSettingsCallback()    { dispatchToJS ("onShowSettings", {}); }
void WebViewEditor::showServiceModeCallback() { dispatchToJS ("showModal", "serviceMode"); }

void WebViewEditor::postMessage (const juce::String& json)
{
    if (webComponent) webComponent->emitEventIfBrowserIsVisible ("hostEvent", json);
}

void WebViewEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void WebViewEditor::resized()
{
    if (webComponent) webComponent->setBounds (getLocalBounds());
}
