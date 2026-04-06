#pragma once

#include <JuceHeader.h>
#include "SynthParams.h"

namespace Omega {
namespace Core {


/**
 * EngineConfig - Structural snapshot of the audio engine state.
 * Passed from UI/Processor to Audio Thread via Lock-Free FIFO.
 */
struct EngineConfig {
    SynthParams parameters;
    double sampleRate = 44100.0;
    int bufferSize = 512;
    int numVoices = 16;
    bool isSoloMode = false;
    int soloVoiceIndex = -1;
    
    // Telemetry configuration
    bool oscilloscopeEnabled = true;
    int telemetryIntervalMs = 15;
};

} // namespace Core
} // namespace Omega
