#include "ABDSimpleJuno106AudioProcessor.h"
#include "../UI/WebView/WebViewEditor.h"

//==============================================================================
// SysEx Communication
//==============================================================================
void ABDSimpleJuno106AudioProcessor::sendSysEx(const juce::MidiMessage& msg)
{
    if (currentParams.midiOut) {
        midiOutBuffer.addEvent(msg, 0);
        lastSentSysExMessage = msg;
    }
    lastSysExMessage = msg; // Update for UI display

    // Notify WebUI for real-time stream display
    if (editor != nullptr) {
        if (auto* wv = dynamic_cast<WebViewEditor*>(editor)) {
            wv->dispatchToJS("sysexLog", juce::String::toHexString(msg.getRawData(), msg.getRawDataSize()));
        }
    }
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::sendPatchDump()
{
    sendSysEx(sysExEngine.makePatchDump(currentParams.midiChannel - 1, currentParams));
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::sendManualMode()
{
    sendSysEx(JunoSysEx::createManualMode(currentParams.midiChannel - 1));
    
    // [Manual Mode Logic] Force immediate snapshot from APVTS to DSP
    updateParamsFromAPVTS();
    paramsAreDirty.store(true);
    patchDumpRequested.store(true); // Broadcast current physical state to MIDI out
    needsVoiceReset.store(true);
}
