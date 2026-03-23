#pragma once

#ifndef JUCE_USE_WIN_WEBVIEW2
#define JUCE_USE_WIN_WEBVIEW2 1
#endif
#ifndef JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING
#define JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING 1
#endif

#include <JuceHeader.h>
#include "../../Core/PluginProcessor.h"

// Minimal bridge declaration to avoid conflicts
class WebViewEditor : public juce::AudioProcessorEditor,
                    public juce::AudioProcessorValueTreeState::Listener,
                    public juce::Timer
{
public:
    WebViewEditor (SimpleJuno106AudioProcessor& p);
    ~WebViewEditor() override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void showAboutCallback();
    void showSettingsCallback();
    void showServiceModeCallback();
    void postMessage(const juce::String& json);

private:
    SimpleJuno106AudioProcessor& audioProcessor;
    
    std::unique_ptr<juce::WebBrowserComponent> webComponent;

    // Bridge helpers
    void updateParameterInJS(const juce::String& paramID, float value);
    void updateSysExInJS();
    void updateLCDInJS (const juce::String& text);
    void dispatchToJS(const juce::Identifier& eventId, const juce::var& payload);

    juce::MidiMessage lastSysEx;
    int lastPresetIndex = -1;
    int lastLibraryIndex = -1;
    juce::String lastTuningName = "";
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewEditor)
};
