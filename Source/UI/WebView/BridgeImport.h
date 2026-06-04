#pragma once
#include <JuceHeader.h>
#include "../../Core/JunoTapeDecoder.h"

class ABDSimpleJuno106AudioProcessor;

namespace BridgeImport {

void confirmImportFile(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               juce::File& pendingImportFile,
               juce::String& pendingImportFormat,
               const juce::Array<juce::var>& args,
               const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
               const std::function<void()>& sendPresetListUpdate,
               const std::function<void()>& requestPatchDump,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void confirmTapeImport(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               juce::File& pendingTapeFile,
               JunoTapeDecoder::SmartDecodeResult& pendingSmartResult,
               int& selectedDecoderIndex,
               const juce::Array<juce::var>& args,
               const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

} // namespace BridgeImport
