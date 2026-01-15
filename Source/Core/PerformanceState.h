#pragma once
#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <array>
// Forward declarations
class JunoVoiceManager;

struct PerformanceState
{
    PerformanceState();
    ~PerformanceState() = default;

    void handleSustain (int value);
    void handleNoteOff (int note, JunoVoiceManager& vm);
    void flushSustain (JunoVoiceManager& vm);
    void updateParams(const struct SynthParams& p);

    juce::AbstractFifo noteOffFifo; 
    std::array<int, 256> noteOffBuffer;

    bool isLegatoActive() const { return noteOffFifo.getNumReady() > 0; }

    
private:
    std::atomic<bool> sustainPedalActive { false };
};
