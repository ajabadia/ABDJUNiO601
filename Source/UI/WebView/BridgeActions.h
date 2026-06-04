#pragma once
#include <JuceHeader.h>

class ABDSimpleJuno106AudioProcessor;

namespace BridgeActions {

void beginGesture(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void chooseDirectory(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               std::unique_ptr<juce::FileChooser>& fileChooser,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void endGesture(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void getBrowserData(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void getCalibrationParams(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void getLibraryPath(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void getSynthState(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void loadLibraryPreset(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void loadPreset(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void pianoNoteOff(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void pianoNoteOn(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void runSelfTest(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               const std::function<void()>& notifySelfTestState,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void saveAsNewPresetDetailed(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
               const std::function<void()>& sendPresetListUpdate,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void savePresetDetailed(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
               const std::function<void()>& sendPresetListUpdate,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void setCalibrationParam(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void setFavorite(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void setLibraryPath(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void setParameter(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void updateMetadata(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void setBrowserData(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void exportBank(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               std::unique_ptr<juce::FileChooser>& fileChooser,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

void importBank(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               std::unique_ptr<juce::FileChooser>& fileChooser,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion);

} // namespace BridgeActions
