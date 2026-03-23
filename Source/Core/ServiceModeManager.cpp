#include "ServiceModeManager.h"
#include "PluginProcessor.h"

ServiceModeManager::ServiceModeManager(SimpleJuno106AudioProcessor& p)
    : processor(p)
{
    soloVoice.store(-1);
    vcfSweepActive.store(false);
    vcfSweepValue.store(0.0f);
    dcoRefActive.store(false);
    testScaleActive.store(false);
    scaleTimer.store(0.0f);
    currentScaleNoteIdx.store(0);
}

void ServiceModeManager::setVoiceSolo(int voiceIndex)
{
    soloVoice.store(voiceIndex);
}

void ServiceModeManager::triggerDCOReference(int midiNote, int /*rangeIndex*/)
{
    dcoRefNote = midiNote;
    dcoRefActive.store(true);
}

void ServiceModeManager::startVCFSweep()
{
    vcfSweepActive.store(true);
    vcfSweepPhase = 0.0;
}

void ServiceModeManager::startTestScale()
{
    bool next = !testScaleActive.load();
    testScaleActive.store(next);
    scaleTimer.store(0.0f);
    currentScaleNoteIdx.store(-1);
    if (!next) processor.keyboardState.allNotesOff(1);
}

void ServiceModeManager::stopAllTests()
{
    vcfSweepActive.store(false);
    dcoRefActive.store(false);
    testScaleActive.store(false);
    soloVoice.store(-1);
    processor.keyboardState.allNotesOff(1);
}

void ServiceModeManager::update(double sampleRate, int numSamples)
{
    if (vcfSweepActive.load())
    {
        double phaseInc = (1.0 / (5.0 * sampleRate)) * numSamples;
        vcfSweepPhase += phaseInc;
        
        if (vcfSweepPhase >= 1.0)
        {
            vcfSweepPhase = 0.0;
            vcfSweepActive.store(false);
        }
        
        vcfSweepValue.store((float)vcfSweepPhase);
    }

    if (testScaleActive.load())
    {
        double dt = (double)numSamples / sampleRate;
        float currentTimer = scaleTimer.load() + (float)dt;
        scaleTimer.store(currentTimer);

        const int scale[] = { 36, 40, 43, 48, 52, 55, 60, 64 }; // C Major Arp
        const float noteDuration = 0.5f;

        int idx = currentScaleNoteIdx.load();
        if (idx == -1 || currentTimer >= noteDuration)
        {
            if (idx >= 0)
                processor.keyboardState.noteOff(1, scale[idx], 0.0f);
            
            idx = (idx + 1) % 8;
            currentScaleNoteIdx.store(idx);
            processor.keyboardState.noteOn(1, scale[idx], 0.8f);
            scaleTimer.store(0.0f);
        }
    }
}
