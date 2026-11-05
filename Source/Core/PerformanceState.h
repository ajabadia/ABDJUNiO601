#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
// Forward declarations
namespace ABD
{
class JunoVoiceManager;

struct PerformanceState
{
    PerformanceState();
    ~PerformanceState() = default;

    void handleSustain(int value);
    void handleNoteOn(int note);
    void handleNoteOff(int note, JunoVoiceManager& vm);
    void flushSustain(JunoVoiceManager& vm);
    void updateParams(const struct SynthParams& p);

    // 0 = Normal, 1 = SOS (sostenuto), 2 = Toggle
    void setMode(int m) { mode.store(juce::jlimit(0, 2, m)); }
    int getMode() const { return mode.load(); }

    juce::AbstractFifo noteOffFifo;
    std::array<int, 256> noteOffBuffer;

    bool isLegatoActive() const { return noteOffFifo.getNumReady() > 0; }

private:
    void pushSustained(int note);
    void latchHeld();
    void releaseLatched();

    std::atomic<bool> sustainPedalActive{false};
    std::atomic<int> mode{0};
    std::array<std::atomic<bool>, 128> heldNotes{};
    std::array<std::atomic<bool>, 128> latchedNotes{};
};
} // namespace ABD
