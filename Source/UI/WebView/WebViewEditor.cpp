#include <JuceHeader.h>
#include "WebViewEditor.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"
#include "../../Core/PresetManager.h"
#include "../../Core/JunoTapeDecoder.h"
#include "../../Core/BuildVersion.h"
#include <optional>
#include "JunoModelConfig.h"
#include "BridgeActions.h"
#include "BridgeImport.h"
#include "BridgeService.h"
#include "BridgeMenu.h"




WebViewEditor::WebViewEditor (ABDSimpleJuno106AudioProcessor& p)
    : juce::AudioProcessorEditor (&p), audioProcessor (p)
{
    auto options = juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withInitialisationData ("buildVersion", juce::String(JUNO_BUILD_VERSION))
        .withInitialisationData ("buildTimestamp", juce::String(JUNO_BUILD_TIMESTAMP))
        .withInitialisationData ("productName", juce::String(getJunoModelName()))
        .withInitialisationData ("targetModel", (int) JUNO_TARGET_MODEL)
        .withNativeIntegrationEnabled (true)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2()
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ABD_JUNiO_601_WebView2")))
        .withResourceProvider ([this] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource> {
            juce::String path = url;
            if (path == "/" || path.isEmpty()) path = "/index.html";
            if (path.startsWith("/")) path = path.substring(1);
            
            juce::File currentFile (__FILE__);
            juce::File webUiDir = currentFile.getParentDirectory().getParentDirectory().getChildFile("WebUI");
            if (!webUiDir.exists()) {
                webUiDir = juce::File("d:\\desarrollos\\ABDSynths\\ABDJUNiO601\\Source\\UI\\WebUI");
            }
            if (!webUiDir.exists()) {
                webUiDir = juce::File("d:\\desarrollos\\ABDJUNiO601\\Source\\UI\\WebUI");
            }
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
            if (path == "service.js")   return getResourceFromBinary (BinaryData::service_js, BinaryData::service_jsSize, "service.js");
            if (path == "service.css")  return getResourceFromBinary (BinaryData::service_css, BinaryData::service_cssSize, "service.css");
            if (path == "style.css")    return getResourceFromBinary (BinaryData::style_css, BinaryData::style_cssSize, "style.css");

            juce::String resourceName = path.replace("/", "_").replace(".", "_").replace("-", "_").replace(" ", "_");
            int binSize = 0;
            const char* binData = BinaryData::getNamedResource(resourceName.toRawUTF8(), binSize);
            
            // Fallback for numeric mangling (e.g. 0.png -> _0_png)
            if (binData == nullptr) {
                juce::String filename = path.fromLastOccurrenceOf("/", false, false);
                if (filename.isEmpty()) filename = path;
                juce::String flattenedName = filename.replace(".", "_").replace("-", "_").replace(" ", "_");
                if (juce::CharacterFunctions::isDigit(flattenedName[0])) flattenedName = "_" + flattenedName;
                binData = BinaryData::getNamedResource(flattenedName.toRawUTF8(), binSize);
            }

            if (binData != nullptr) return getResourceFromBinary(binData, binSize, path);

            return std::nullopt;
        })
                .withNativeFunction ("setParameter", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::setParameter(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("beginGesture", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::beginGesture(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("endGesture", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::endGesture(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("getCalibrationParams", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::getCalibrationParams(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("setCalibrationParam", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::setCalibrationParam(audioProcessor, args, std::move(completion));
        })
        .withNativeFunction ("serviceAction", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeService::serviceAction(audioProcessor, fileChooser, args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                std::move(completion));
        })
                .withNativeFunction ("loadPreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::loadPreset(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("loadLibraryPreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::loadLibraryPreset(audioProcessor, args, std::move(completion));
        })

                .withNativeFunction ("runSelfTest", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::runSelfTest(audioProcessor, args,
                [this]() { notifySelfTestState(); },
                std::move(completion));
        })
        .withNativeFunction ("menuAction", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            // Handle uiReady inline (tightly coupled to editor state)
            if (args.size() >= 1 && args[0].toString() == "uiReady") {
                writeLog("[JUNiO] UI READY - Performing full state dump");
                sendPresetListUpdate();
                if (auto* pm = audioProcessor.getPresetManager()) {
                    const auto& pr = pm->getCurrentPreset();
                    sendBankPatchUpdate(pr.originGroup, pr.originBank, pr.originPatch);
                    juce::String lcdString = "P: " + juce::String(audioProcessor.getCurrentProgram() + 1) + " " + audioProcessor.getProgramName(audioProcessor.getCurrentProgram());
                    dispatchToJS("onLCDUpdate", lcdString);
                    notifySelfTestState();
                }
                completion({});
                return;
            }
            BridgeMenu::menuAction(audioProcessor, fileChooser,
                pendingImportFile, pendingImportFormat,
                pendingTapeFile, pendingSmartResult, selectedDecoderIndex,
                args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                [this]() { sendPresetListUpdate(); },
                [this]() { showAboutCallback(); },
                [this]() { showSettingsCallback(); },
                [this]() { showServiceModeCallback(); },
                std::move(completion));
        })
        .withNativeFunction ("setBrowserData", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::setBrowserData(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("getSynthState", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::getSynthState(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("getBrowserData", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::getBrowserData(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("setFavorite", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::setFavorite(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("updateMetadata", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::updateMetadata(audioProcessor, args, std::move(completion));
        })
        .withNativeFunction ("exportBank", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::exportBank(audioProcessor, fileChooser, args, std::move(completion));
        })
        .withNativeFunction ("importBank", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::importBank(audioProcessor, fileChooser, args, std::move(completion));
        })
                .withNativeFunction ("savePresetDetailed", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::savePresetDetailed(audioProcessor, args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                [this]() { sendPresetListUpdate(); },
                std::move(completion));
        })
                .withNativeFunction ("saveAsNewPresetDetailed", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::saveAsNewPresetDetailed(audioProcessor, args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                [this]() { sendPresetListUpdate(); },
                std::move(completion));
        })
                .withNativeFunction ("confirmImportFile", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeImport::confirmImportFile(audioProcessor, pendingImportFile, pendingImportFormat, args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                [this]() { sendPresetListUpdate(); },
                [this]() { audioProcessor.requestPatchDump(); },
                std::move(completion));
        })
                .withNativeFunction ("confirmTapeImport", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeImport::confirmTapeImport(audioProcessor, pendingTapeFile, pendingSmartResult,
                selectedDecoderIndex, args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                std::move(completion));
        })
                .withNativeFunction ("pianoNoteOn", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::pianoNoteOn(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("pianoNoteOff", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::pianoNoteOff(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("chooseDirectory", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::chooseDirectory(audioProcessor, fileChooser, args, std::move(completion));
        })
                .withNativeFunction ("getLibraryPath", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::getLibraryPath(audioProcessor, args, std::move(completion));
        })
                .withNativeFunction ("setLibraryPath", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::setLibraryPath(audioProcessor, args, std::move(completion));
        })
        .withNativeFunction ("uiReady", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::ignoreUnused(args);
            writeLog("[JUNiO] JS UI is Ready. Sending Initial State...");
            
            // Mandatory completion for JUCE 8
            completion (juce::var::undefined());

            // Use a small delay to ensure JS listeners are fully active
            juce::Timer::callAfterDelay(100, [this]() {
                writeLog("[JUNiO] Sending delayed state dump...");
                
                // 1. Version Info
                juce::String versionStr = "1.3.0 (Build " + juce::String(JUNO_BUILD_VERSION) + ")";
                dispatchToJS ("onVersionUpdate", versionStr);
                
                // Send both product name and target model
                juce::DynamicObject::Ptr modelInfo = new juce::DynamicObject();
                modelInfo->setProperty("name", juce::String(getJunoModelName()));
                modelInfo->setProperty("targetModel", (int) JUNO_TARGET_MODEL);
                dispatchToJS ("onProductNameUpdate", juce::var(modelInfo.get()));
                
                int prog = audioProcessor.getCurrentProgram();
                int group = prog / 64;
                int rem = prog % 64;
                int bank = (rem / 8) + 1;
                int patch = (rem % 8) + 1;
                
                juce::String lcdString = "P: " + juce::String(prog + 1) + " " + audioProcessor.getProgramName(prog);
                dispatchToJS ("onLCDUpdate", lcdString);
                
                sendBankPatchUpdate(group, bank, patch);

                // Initial parameter sync
                for (auto* param : audioProcessor.getParameters()) {
                    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
                        updateParameterInJS(p->getParameterID(), p->getValue());
                    }
                }
                
                // Sync lastParams immediately
                audioProcessor.updateParamsFromAPVTS();
                audioProcessor.requestPatchDump();
                
                writeLog("[JUNiO] Initial State Sync Complete");
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
        "chorusHiss", "midiFunction", "unisonWidth", "aftertouchToVCF",
        "modelDCO", "modelHPF", "modelVCF", "modelADSR", "modelChorus", "modelArp", "modelPoly", "modelPorta", "modelUnison",
        "arpEnabled", "arpMode", "arpRange", "arpRate", "arpSync", "arpDivision",
        "delayEnabled", "delaySetting", "delayRepeatRate", "delayIntensity",
        "delayBass", "delayTreble", "delayReverbVol", "delayEchoVol",
        "delayEchoCancel", "delaySyncEnabled", "delaySyncDivision",
        "delayReverbType", "delayWowFlutter", "delayReverbDecay", "delayEchoIsolator"
    };
    for (int i = 0; i < paramIDs.size(); ++i)
        audioProcessor.getAPVTS().addParameterListener (paramIDs[i], this);

    startTimerHz(30); 
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colours::black);
    setSize (1240, 750);
}

WebViewEditor::~WebViewEditor()
{
    audioProcessor.editor = nullptr;
    const juce::StringArray paramIDs = {
        "masterVolume", "lfoRate", "lfoDelay", "lfoToDCO", "pwm", "pwmMode",
        "hpfFreq", "vcfFreq", "resonance", "envAmount", "vcfPolarity", "kybdTracking", "lfoToVCF",
        "vcaMode", "vcaLevel", "attack", "decay", "sustain", "release", "chorus1", "chorus2",
        "benderToDCO", "benderToVCF", "benderToLFO", "portamentoTime", "portamentoOn", "portamentoLegato",
        "polyMode", "tune", "dcoRange", "sawOn", "pulseOn", "subOsc", "noise", "bender", "midiOut",
        "midiChannel", "benderRange", "velocitySens", "lcdBrightness", "numVoices", "sustainInverted",
        "chorusHiss", "midiFunction", "unisonWidth", "aftertouchToVCF",
        "modelDCO", "modelHPF", "modelVCF", "modelADSR", "modelChorus", "modelArp", "modelPoly", "modelPorta", "modelUnison",
        "arpEnabled", "arpMode", "arpRange", "arpRate", "arpSync", "arpDivision",
        "delayEnabled", "delaySetting", "delayRepeatRate", "delayIntensity",
        "delayBass", "delayTreble", "delayReverbVol", "delayEchoVol",
        "delayEchoCancel", "delaySyncEnabled", "delaySyncDivision",
        "delayReverbType", "delayWowFlutter", "delayReverbDecay", "delayEchoIsolator"
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

#ifndef ABD_PROCESSOR_HAS_TELEMETRY
#error "Wrong ABDSimpleJuno106AudioProcessor.h included! Check your include paths."
#endif
    if (audioProcessor.popMidiTrafficFlag())
        dispatchToJS("onMidiTraffic", true);
    if (auto* pm = audioProcessor.getPresetManager()) {
        int absoluteIdx = (pm->getCurrentLibraryIndex() * 64) + pm->getCurrentPresetIndex();
        if (absoluteIdx != lastPresetIndex) {
            lastPresetIndex = absoluteIdx;
            
            int group = pm->getCurrentLibraryIndex();
            int localIdx = pm->getCurrentPresetIndex();
            int bank = (localIdx / 8) + 1;
            int patch = (localIdx % 8) + 1;
            sendBankPatchUpdate(group, bank, patch);
            
            juce::String libName = pm->getLibrary(group).name.replace("- ", "").toUpperCase();
            const auto& p = pm->getCurrentLibrary().patches[(size_t)localIdx];
            juce::String presetInfo = p.category + " " + p.name;
            
            juce::String lcdText = libName + " - P: " + juce::String(localIdx + 1) + " " + presetInfo;
            dispatchToJS("onLCDUpdate", lcdText);
        }
    }

    // Monitor Tuning Title changes
    juce::String currentTuning = audioProcessor.getCurrentTuningName();
    if (currentTuning != lastTuningName) {
        lastTuningName = currentTuning;
        dispatchToJS("onTuningUpdate", currentTuning);
    }

    // LCD Status Badges (LC, A/B, WIP)
    juce::DynamicObject::Ptr status = new juce::DynamicObject();
    status->setProperty("lc", audioProcessor.getMidiLearnHandler().getIsLearning());
    status->setProperty("ab", audioProcessor.getActiveABSlot() == 0 ? "A" : "B");
    status->setProperty("wip", audioProcessor.getWipCount());
    dispatchToJS("onLCDStatusUpdate", juce::var(status.get()));

    // Delay Sync BPM (only dispatched when non-zero — JS decides whether to display)
    double currentBPM = audioProcessor.getDelaySyncBPM();
    if (std::abs(currentBPM - lastDispatchedBPM) > 0.5) {
        lastDispatchedBPM = currentBPM;
        dispatchToJS("onDelayBPMUpdate", currentBPM);
    }
}

void WebViewEditor::dispatchToJS(const juce::Identifier& eventId, const juce::var& payload)
{
    if (webComponent) {
        // [OMEGA] Hardened JUCE 8 emission
        webComponent->emitEventIfBrowserIsVisible(eventId.toString(), payload);

        // Fallback for generic hostEvent listener
        juce::DynamicObject::Ptr fObj = new juce::DynamicObject();
        fObj->setProperty("id", eventId.toString());
        fObj->setProperty("payload", payload);
        webComponent->emitEventIfBrowserIsVisible("hostEvent", juce::var(fObj.get()));
    }
}

void WebViewEditor::showAboutCallback()       { dispatchToJS ("onShowAbout", {}); }
void WebViewEditor::showSettingsCallback()    { dispatchToJS ("onShowSettings", {}); }
void WebViewEditor::showServiceModeCallback() { dispatchToJS ("showModal", "serviceMode"); }

void WebViewEditor::sendPresetListUpdate()
{
    if (auto* pm = audioProcessor.getPresetManager())
    {
        juce::DynamicObject::Ptr root (new juce::DynamicObject());
        root->setProperty(juce::Identifier("currentLibrary"), juce::var(pm->getCurrentLibraryName()));
        root->setProperty(juce::Identifier("currentPresetIndex"), juce::var(pm->getCurrentPresetIndex()));

        juce::Array<juce::var> items;
        const auto& lib = pm->getCurrentLibrary();

        for (int i = 0; i < (int)lib.patches.size(); ++i)
        {
            const auto& p = lib.patches[(size_t)i];

            juce::DynamicObject::Ptr o (new juce::DynamicObject());
            o->setProperty(juce::Identifier("index"),    juce::var(i));
            o->setProperty(juce::Identifier("name"),     juce::var(p.name));
            o->setProperty(juce::Identifier("category"), juce::var(p.category));
            o->setProperty(juce::Identifier("author"),   juce::var(p.author));
            o->setProperty(juce::Identifier("tags"),     juce::var(p.tags));
            o->setProperty(juce::Identifier("favorite"), juce::var(p.isFavorite));

            o->setProperty(juce::Identifier("originGroup"), juce::var(p.originGroup));
            o->setProperty(juce::Identifier("originBank"),  juce::var(p.originBank));
            o->setProperty(juce::Identifier("originPatch"), juce::var(p.originPatch));

            items.add(juce::var(o.get()));
        }

        root->setProperty("items", juce::var(items));
        dispatchToJS("onPresetListUpdate", juce::var(root.get()));
    }
}

void WebViewEditor::sendBankPatchUpdate (int group, int bank, int patch)
{
    juce::DynamicObject::Ptr obj (new juce::DynamicObject());
    obj->setProperty(juce::Identifier("group"), juce::var(group));
    obj->setProperty(juce::Identifier("bank"),  juce::var(bank));
    obj->setProperty(juce::Identifier("patch"), juce::var(patch));

    dispatchToJS("onBankPatchUpdate", juce::var(obj.get()));
}

void WebViewEditor::updateLCDInJS (const juce::String& text)
{
    dispatchToJS("onLCDUpdate", text);
}

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

void WebViewEditor::writeLog(const juce::String& msg) {
    juce::Logger::writeToLog (msg);
}

void WebViewEditor::notifySelfTestState()
{
    auto res = audioProcessor.getLastSelfTestResult();
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("hasRun", res.hasRun);
    if (res.hasRun) {
        obj->setProperty("ok", res.ok);
        obj->setProperty("presetFailures", res.presetFailures);
        obj->setProperty("sysExOk", res.sysExOk);
        obj->setProperty("jsonOk", res.jsonOk);
        juce::Array<juce::var> failedArr;
        for (const auto& s : res.failedPresets) failedArr.add(s);
        obj->setProperty("failedPresets", failedArr);
    }

    dispatchToJS("selfTestState", juce::var(obj.get()));
}

// End of WebViewEditor.cpp
