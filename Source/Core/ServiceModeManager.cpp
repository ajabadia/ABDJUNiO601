#include "ServiceModeManager.h"
#include "PluginProcessor.h"

ServiceModeManager::ServiceModeManager(SimpleJuno106AudioProcessor& p)
    : processor(p)
{
}

void ServiceModeManager::setVoiceSolo(int voiceIndex)
{
    soloVoice.store(voiceIndex);
}

void ServiceModeManager::triggerDCOReference(int midiNote, int rangeIndex)
{
    dcoRefNote = midiNote;
    dcoRefActive = true;
    // Implementation would trigger a specific note on the allocated voice(s)
}

void ServiceModeManager::startVCFSweep()
{
    vcfSweepActive = true;
    vcfSweepPhase = 0.0;
}

void ServiceModeManager::stopAllTests()
{
    vcfSweepActive = false;
    dcoRefActive = false;
    soloVoice.store(-1);
}

void ServiceModeManager::update(double sampleRate, int numSamples)
{
    if (vcfSweepActive)
    {
        // 5-second sweep from 0 to 1
        double phaseInc = (1.0 / (5.0 * sampleRate)) * numSamples;
        vcfSweepPhase += phaseInc;
        
        if (vcfSweepPhase >= 1.0)
        {
            vcfSweepPhase = 0.0;
            vcfSweepActive = false;
        }
        
        vcfSweepValue = (float)vcfSweepPhase;
    }
}
