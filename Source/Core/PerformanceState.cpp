#include "PerformanceState.h"
#include "JunoVoiceManager.h"

void PerformanceState::handleSustain (int value)
{
    const bool newState = (value >= 64);
    if (!newState && sustainPedalActive)
    {
        sustainPedalActive = false;
        // Notes are released from outside by calling flushSustain
    }
    else
    {
        sustainPedalActive = newState;
    }
}

void PerformanceState::handleNoteOff (int note, JunoVoiceManager& vm)
{
    if (sustainPedalActive)
    {
        pendingNoteOffs.push_back (note);
    }
    else
    {
        // 1 is the channel? PluginProcessor passed channel from midi message.
        // User snippet showed: vm.noteOff (1, note, 0.0f);
        // Better to verify if channel is needed. JunoVoiceManager::noteOff takes (midiChannel, note, velocity).
        // Let's assume channel 1 for now or if we can pass it?
        // The method signature in header only takes (note, vm). The user example hardcoded 1.
        vm.noteOff (1, note, 0.0f);
    }
}

void PerformanceState::flushSustain (JunoVoiceManager& vm)
{
    if (!sustainPedalActive)
    {
        for (int note : pendingNoteOffs)
            vm.noteOff (1, note, 0.0f);
        pendingNoteOffs.clear();
    }
}
