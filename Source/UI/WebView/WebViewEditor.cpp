#include "WebViewEditor.h"
#include "../../Core/PresetManager.h"
#include "BinaryData.h"
#include <memory>

static std::unique_ptr<juce::FileLogger> webViewLogger;
static void log (const juce::String& msg) { if (webViewLogger) webViewLogger->logMessage (msg); juce::Logger::writeToLog (msg); }

WebViewEditor::WebViewEditor (SimpleJuno106AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), lastLibraryIndex(-1)
{
    auto resolveUserDataDir = []() -> juce::File {
        auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("JUNiO601_WebView_Cache");
        f.createDirectory();
        return f;
    };

    const auto getMimeType = [] (const juce::String& path) -> juce::String {
        if (path.endsWith (".html")) return "text/html";
        if (path.endsWith (".css"))  return "text/css";
        if (path.endsWith (".js"))   return "text/javascript";
        if (path.endsWith (".png"))  return "image/png";
        if (path.endsWith (".ttf"))  return "font/ttf";
        return "application/octet-stream";
    };

    auto options = juce::WebBrowserComponent::Options()
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2().withUserDataFolder (resolveUserDataDir()))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([getMimeType] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource> {
            auto path = (url == "/" || url.isEmpty()) ? "index.html" : url.substring (1);
            juce::String decodedPath = juce::URL::removeEscapeChars (path);
            juce::String filename = juce::File (decodedPath).getFileName();
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
                if (filename == BinaryData::originalFilenames[i]) {
                    int dataSize = 0;
                    if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], dataSize)) {
                        std::vector<std::byte> dataVec (static_cast<size_t> (dataSize));
                        std::memcpy (dataVec.data(), data, static_cast<size_t> (dataSize));
                        return juce::WebBrowserComponent::Resource { std::move (dataVec), getMimeType (filename) };
                    }
                }
            }
            return std::nullopt;
        });

    options = options.withNativeFunction ("setParameter", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() == 2) {
            auto paramID = args[0].toString();
            float value = static_cast<float>(args[1]);
            if (auto* p = audioProcessor.getAPVTS().getParameter (paramID)) p->setValueNotifyingHost (value);
        }
        completion (juce::var());
    });

    options = options.withNativeFunction ("pianoNoteOn", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() >= 1) audioProcessor.keyboardState.noteOn(1, (int)args[0], args.size() > 1 ? (float)args[1] : 0.8f);
        completion (juce::var());
    });

    options = options.withNativeFunction ("pianoNoteOff", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() >= 1) audioProcessor.keyboardState.noteOff(1, (int)args[0], 0.0f);
        completion (juce::var());
    });

    options = options.withNativeFunction ("loadPreset", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() == 1) audioProcessor.loadPreset((int)args[0]);
        completion (juce::var());
    });

    options = options.withNativeFunction ("menuAction", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() == 1) {
            juce::String action = args[0].toString();
            if (action == "panic") audioProcessor.triggerPanic();
            else if (action == "handleRandomize") { if (auto* pm = audioProcessor.getPresetManager()) pm->randomizeCurrentParameters(audioProcessor.getAPVTS()); }
            else if (action == "handleManual") audioProcessor.sendManualMode();
            else if (action == "handleTest") audioProcessor.enterTestMode(true);
            else if (action == "handleSettings") emitJS ("onShowSettings", juce::var());
            else if (action == "handleAbout") emitJS ("onShowAbout", {});
        }
        completion (juce::var());
    });

    options = options.withNativeFunction ("uiReady", [this] (const juce::Array<juce::var>& /*args*/, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        for (auto& p : audioProcessor.getParameters()) {
            if (auto* param = dynamic_cast<juce::AudioProcessorParameterWithID*>(p)) updateParameterInJS (param->paramID, param->getValue());
        }
        completion (juce::var());
    });

    webView = std::make_unique<juce::WebBrowserComponent> (options);
    webView->setOpaque (true);
    addAndMakeVisible (*webView);
    if (webView != nullptr) webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    startTimerHz(30);
    setSize (1200, 750);
}

WebViewEditor::~WebViewEditor() {}

void WebViewEditor::parameterChanged (const juce::String& parameterID, float newValue) {
    if (auto* p = audioProcessor.getAPVTS().getParameter(parameterID)) updateParameterInJS (parameterID, p->getValue());
}

void WebViewEditor::updateParameterInJS (const juce::String& paramID, float value) {
    if (webView != nullptr) {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty ("id", paramID);
        obj->setProperty ("value", value);
        emitJS ("onParameterChanged", juce::var (obj.get()));
    }
}

void WebViewEditor::updateSysExInJS() {
    auto msg = audioProcessor.getCurrentSysExData();
    if (msg.getRawDataSize() > 0) {
        juce::String hex;
        for (int i=0; i < msg.getRawDataSize(); ++i) hex += juce::String::toHexString(msg.getRawData()[i]).toUpperCase().paddedLeft('0', 2) + " ";
        if (webView != nullptr) emitJS ("onSysExUpdate", { hex.trim() });
    }
}

void WebViewEditor::timerCallback() {
    updateSysExInJS();
    if (webView != nullptr) {
        juce::DynamicObject::Ptr visObj = new juce::DynamicObject();
        visObj->setProperty ("c1", audioProcessor.getChorusLfoPhase(1));
        visObj->setProperty ("c2", audioProcessor.getChorusLfoPhase(2));
        emitJS ("onVisualUpdate", juce::var (visObj.get()));
    }
}

void WebViewEditor::emitJS (const juce::Identifier& eventId, const juce::var& payload) {
    if (webView != nullptr) {
        juce::String json = juce::JSON::toString (payload, true);
        juce::String escaped = json.replace ("\\", "\\\\").replace ("'", "\\'");
        webView->evaluateJavascript ("window.__JUCE__.backend.emitByBackend(" + eventId.toString().quoted() + ", " + escaped.quoted ('\'') + ");");
    }
}

void WebViewEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }
void WebViewEditor::resized() { if (webView != nullptr) webView->setBounds (getLocalBounds()); }
