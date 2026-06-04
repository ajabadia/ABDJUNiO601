#pragma once
#include <JuceHeader.h>
#include "../../Core/JunoTapeDecoder.h"

class ABDSimpleJuno106AudioProcessor;

namespace BridgeMenu {

void menuAction(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    juce::File& pendingImportFile,
    juce::String& pendingImportFormat,
    juce::File& pendingTapeFile,
    JunoTapeDecoder::SmartDecodeResult& pendingSmartResult,
    int& selectedDecoderIndex,
    const juce::Array<juce::var>& args,
    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
    const std::function<void()>& sendPresetListUpdate,
    const std::function<void()>& showAboutCallback,
    const std::function<void()>& showSettingsCallback,
    const std::function<void()>& showServiceModeCallback,
    juce::WebBrowserComponent::NativeFunctionCompletion completion);

} // namespace BridgeMenu
