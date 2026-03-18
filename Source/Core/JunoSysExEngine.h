#pragma once

#include <JuceHeader.h>
#include "SynthParams.h"
#include "JunoSysEx.h"

class JunoSysExEngine
{
public:
    // Procesa un mensaje SysEx entrante y actualiza los parámetros del motor.
    void handleIncomingSysEx (const juce::MidiMessage& msg, SynthParams& params);

    // Construye un 0x32 (param change) listo para enviar.
    juce::MidiMessage makeParamChange (int channel, int paramId, int value);

    // Construye un 0x30 (patch dump) a partir del estado actual.
    juce::MidiMessage makePatchDump  (int channel, const SynthParams& params);

    void setDeviceId (int id) { deviceId = id; }
    int getDeviceId() const { return deviceId; }

private:
    int deviceId = 0x18; // [Fidelidad] Default Device ID
    // Helpers internos
    void applyParamChange (int paramId, int value7bit, SynthParams& params);
    void applyPatchDump   (const uint8_t* dumpData, SynthParams& params);
};
