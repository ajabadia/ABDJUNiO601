#include "ServiceModeManager.h"
#include "ABDSimpleJuno106AudioProcessor.h"

ServiceModeManager::ServiceModeManager(ABDSimpleJuno106AudioProcessor& p)
    : processor(p)
{
    soloVoice.store(-1);
    vcfSweepActive.store(false);
    vcfSweepValue.store(0.0f);
    dcoRefActive.store(false);
    testScaleActive.store(false);
    scaleTimer.store(0.0f);
    currentScaleNoteIdx.store(0);
    autoVcfTuneActive.store(false);
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
void ServiceModeManager::startHpfCycle()
{
    hpfCycleActive.store(!hpfCycleActive.load());
    hpfCycleTimer = 0.0;
    hpfCycleValue.store(0);
}
void ServiceModeManager::startChorusCycle()
{
    chorusCycleActive.store(!chorusCycleActive.load());
    chorusCycleTimer = 0.0;
    chorusCycleValue.store(0);
}
void ServiceModeManager::startTestScale()
{
    bool next = !testScaleActive.load();
    testScaleActive.store(next);
    scaleTimer.store(0.0f);
    currentScaleNoteIdx.store(-1);
    if (!next) processor.keyboardState.allNotesOff(1);
}

void ServiceModeManager::startAutoVcfTune()
{
    stopAllTests();
    autoVcfTuneActive.store(true);
    autoTuneCycle = 0;
    autoTuneStep = 0.0f;
    
    // Preparation: Solo Voice 1, VCF Self-Osc
    setVoiceSolo(0);
}

void ServiceModeManager::stopAllTests()
{
    vcfSweepActive.store(false);
    hpfCycleActive.store(false);
    chorusCycleActive.store(false);
    dcoRefActive.store(false);
    testScaleActive.store(false);
    autoVcfTuneActive.store(false);
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

    if (hpfCycleActive.load())
    {
        double dt = (double)numSamples / sampleRate;
        hpfCycleTimer += dt;
        if (hpfCycleTimer >= 1.0) {
            hpfCycleTimer = 0.0;
            int next = (hpfCycleValue.load() + 1) % 4;
            hpfCycleValue.store(next);
        }
    }

    if (chorusCycleActive.load())
    {
        double dt = (double)numSamples / sampleRate;
        chorusCycleTimer += dt;
        if (chorusCycleTimer >= 1.0) {
            chorusCycleTimer = 0.0;
            int next = (chorusCycleValue.load() + 1) % 4; // 0=Off, 1=I, 2=II, 3=Both
            chorusCycleValue.store(next);
        }
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

    if (autoVcfTuneActive.load())
    {
        double dt = (double)numSamples / sampleRate;
        autoTuneStep += (float)dt;

        // Iterative Tuning Algorithm (Simulated Measurement & Correction)
        // Cycle 0: Find MinHz (A0 reference)
        // Cycle 1: Find Width (A4 reference)
        
        const float cycleDuration = 1.0f; 
        if (autoTuneStep >= cycleDuration)
        {
            autoTuneStep = 0.0f;
            autoTuneCycle++;

            if (autoTuneCycle == 1) 
            {
                // Simulated: Found MinHz error -> Correcting
                float currentMin = processor.getCalibrationSettings().getValue("vcfMinHz");
                processor.getCalibrationSettings().setValue("vcfMinHz", currentMin * 1.02f); // Small adjustment
            }
            else if (autoTuneCycle == 2)
            {
                // Simulated: Found Width error -> Correcting
                float currentWidth = processor.getCalibrationSettings().getValue("vcfWidth");
                processor.getCalibrationSettings().setValue("vcfWidth", currentWidth * 0.98f);
            }
            else if (autoTuneCycle >= 3)
            {
                // Finished
                stopAllTests();
            }
        }
    }
}
