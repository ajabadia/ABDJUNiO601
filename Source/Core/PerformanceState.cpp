#include "PerformanceState.h"
#include "JunoVoiceManager.h"

namespace ABD
{
PerformanceState::PerformanceState()
    : noteOffFifo(256)
{
    sustainPedalActive = false;
    for (auto& n : heldNotes) n = false;
    for (auto& n : latchedNotes) n = false;
}

void PerformanceState::pushSustained(int note)
{
    int start1, size1, start2, size2;
    noteOffFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
        noteOffBuffer[start1] = note;
    else if (size2 > 0)
        noteOffBuffer[start2] = note;
    noteOffFifo.finishedWrite(size1 + size2);
}

void PerformanceState::latchHeld()
{
    for (int n = 0; n < 128; ++n)
        latchedNotes[n].store(heldNotes[n].load());
}

void PerformanceState::releaseLatched()
{
    for (int n = 0; n < 128; ++n)
        if (latchedNotes[n].exchange(false))
            pushSustained(n);
}

void PerformanceState::handleSustain(int value)
{
    const bool pressed = value >= 64;
    const int m        = mode.load();

    if (m == 2) // Toggle: cada pisada conmuta, soltar el pedal no hace nada
    {
        if (!pressed) return;
        const bool next = !sustainPedalActive.load();
        sustainPedalActive.store(next);
        if (next) latchHeld();
        return;
    }

    if (pressed == sustainPedalActive.load()) return;
    sustainPedalActive.store(pressed);

    if (m == 1) // SOS: solo las notas que ya sonaban al pisar el pedal
    {
        if (pressed)
            latchHeld();
        else
            releaseLatched();
    }
}

void PerformanceState::handleNoteOn(int note)
{
    if (note < 0 || note > 127) return;
    heldNotes[note].store(true);
}

void PerformanceState::handleNoteOff(int note, JunoVoiceManager& vm)
{
    const bool valid = (note >= 0 && note < 128);
    if (valid) heldNotes[note].store(false);

    const bool sosExcludes = (mode.load() == 1) && valid && !latchedNotes[note].load();
    if (sustainPedalActive.load() && !sosExcludes)
    {
        // Push to FIFO (Audio Thread Safe)
        pushSustained(note);
    }
    else
    {
        vm.noteOff(1, note, 0.0f);
    }
}

void PerformanceState::flushSustain(JunoVoiceManager& vm)
{
    if (!sustainPedalActive.load())
    {
        int numReady = noteOffFifo.getNumReady();
        if (numReady > 0)
        {
            int start1, size1, start2, size2;
            noteOffFifo.prepareToRead(numReady, start1, size1, start2, size2);

            for (int i = 0; i < size1; ++i) vm.noteOff(1, noteOffBuffer[start1 + i], 0.0f);
            for (int i = 0; i < size2; ++i) vm.noteOff(1, noteOffBuffer[start2 + i], 0.0f);

            noteOffFifo.finishedRead(size1 + size2);
        }
    }
}

void PerformanceState::updateParams(const SynthParams& p)
{
    // El modo se aplica desde JunoEngine::updateParams (aqui no hay definicion completa).
    juce::ignoreUnused(p);
}
} // namespace ABD
