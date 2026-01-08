#pragma once
#include <vector>
// Forward declarations
class JunoVoiceManager;

struct PerformanceState
{
    bool sustainPedalActive = false;
    std::vector<int> pendingNoteOffs;

    void handleSustain (int value);
    void handleNoteOff (int note, JunoVoiceManager& vm);
    void flushSustain (JunoVoiceManager& vm);

    bool isLegatoActive() const { return !pendingNoteOffs.empty(); }
};
