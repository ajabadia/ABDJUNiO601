#pragma once
#include <JuceHeader.h>

class ABDSimpleJuno106AudioProcessor;

namespace BridgeService {

void serviceAction(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    const juce::Array<juce::var>& args,
    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
    juce::WebBrowserComponent::NativeFunctionCompletion completion);

} // namespace BridgeService
