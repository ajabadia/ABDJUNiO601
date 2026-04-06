#include "OmegaUiBridge.h"
#include "../../Core/CalibrationSettings.h"
#include "../../Core/ServiceModeManager.h"

namespace Omega {
namespace UI {

OmegaUiBridge::OmegaUiBridge(Core::Instrument& inst, juce::WebBrowserComponent& webComp)
    : instrument(inst), webComponent(webComp)
{
}

void OmegaUiBridge::registerNativeFunctions()
{
    // [OMEGA] Native functions are registered via WebBrowserComponent::Options
    // inside WebViewEditor.cpp to comply with JUCE 8 immutable options API.
}

void OmegaUiBridge::triggerEvent(const juce::String& eventName, const juce::var& parameters)
{
    // [OMEGA] JUCE 8 transition: emitEvent directly to trigger backend.addEventListener in JS.
    webComponent.emitEventIfBrowserIsVisible(eventName, parameters);
}

void OmegaUiBridge::updateParameter(const juce::String& paramId, float value)
{
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("id", paramId);
    payload->setProperty("value", value);
    triggerEvent("onParameterChanged", juce::var(payload));
}

void OmegaUiBridge::updateLCD(const juce::String& text)
{
    triggerEvent("onLCDUpdate", text);
}

void OmegaUiBridge::updateBankPatch(int bank, int patch)
{
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("bank", bank);
    payload->setProperty("patch", patch);
    triggerEvent("onBankPatchUpdate", juce::var(payload));
}

void OmegaUiBridge::updateSysEx(const juce::MidiMessage& msg)
{
    juce::String hex = juce::String::toHexString(msg.getRawData(), (int)msg.getRawDataSize());
    triggerEvent("onSysExUpdate", hex);
}

void OmegaUiBridge::handleParamChange(const juce::String& paramId, float value)
{
    if (auto* param = instrument.getAPVTS().getParameter(paramId)) {
        param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(value));
    }
}

void OmegaUiBridge::handleAction(const juce::String& action, const juce::Array<juce::var>& args)
{
    if (action == "panic") instrument.triggerPanic();
    else if (action == "lfo") instrument.triggerLFO();
    else if (action == "manual") instrument.sendManualMode();
    // Add more actions as needed
}

} // namespace UI
} // namespace Omega


