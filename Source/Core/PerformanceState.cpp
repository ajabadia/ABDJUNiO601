#include "PerformanceState.h"
#include "JunoVoiceManager.h"
#include "SynthParams.h"

PerformanceState::PerformanceState() : noteOffFifo(256)
{
    sustainPedalActive = false;
}

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
    if (sustainPedalActive.load())
    {
        // Push to FIFO (Audio Thread Safe)
        int start1, size1, start2, size2;
        noteOffFifo.prepareToWrite(1, start1, size1, start2, size2);
        if (size1 > 0) noteOffBuffer[start1] = note;
        else if (size2 > 0) noteOffBuffer[start2] = note;
        noteOffFifo.finishedWrite(size1 + size2);
    }
    else
    {
        vm.noteOff (1, note, 0.0f);
    }
}

void PerformanceState::flushSustain (JunoVoiceManager& vm)
{
    if (!sustainPedalActive.load())
    {
        // Determine how many items to read
        int numReady = noteOffFifo.getNumReady();
        if (numReady > 0) {
            int start1, size1, start2, size2;
            noteOffFifo.prepareToRead(numReady, start1, size1, start2, size2);
            
            for (int i = 0; i < size1; ++i) vm.noteOff(1, noteOffBuffer[start1 + i], 0.0f);
            for (int i = 0; i < size2; ++i) vm.noteOff(1, noteOffBuffer[start2 + i], 0.0f);
            
            noteOffFifo.finishedRead(size1 + size2);
        }
    }
}
// [Fix] Implementation of updateParams
#include "SynthParams.h"
void PerformanceState::updateParams(const SynthParams& p)
{
    juce::ignoreUnused(p);
    // Portamento and Bender are usually handled by VoiceManager directly via SynthParams,
    // but if PerformanceState manages "Legato Logic" or "Portamento Time smoothing" apart from voice,
    // it would go here. 
    // Currently JunoVoiceManager reads directly from SynthParams?
    // Let's check VoiceManager. 
    // VoiceManager::updateParams takes SynthParams. 
    // So PerformanceState might not strictly need this for Portamento *values*, 
    // but we added the call in PluginProcessor. 
    // We can leave it empty or use it for future performance flags.
}
