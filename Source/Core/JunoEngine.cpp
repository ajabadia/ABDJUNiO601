#include "JunoEngine.h"
#include "CalibrationSettings.h"
#include "JunoConstants.h"

using namespace JunoConstants;

JunoEngine::JunoEngine()
    : arpeggiator(std::make_unique<JunoArpeggiator>())
{
    auto coeffs = juce::dsp::IIR::Coefficients<float>(1.0f, -1.0f, 1.0f, -0.995f);
    dcBlocker.state = &coeffs;
}

void JunoEngine::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;

    voiceManager.prepare(sr, maxBlockSize);
    if (arpeggiator != nullptr) {
        arpeggiator->SetSampleRate((float)sr);
        arpeggiator->Reset();
    }

    chorus.prepare(sr, maxBlockSize);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32)maxBlockSize, 2 };
    dcBlocker.reset();
    dcBlocker.prepare(spec);

    chorusNoiseBuffer.setSize(2, maxBlockSize);
    lfoBuffer.assign(maxBlockSize + 128, 0.0f);

    smoothedSagGain.reset(sr, 0.02);
    smoothedSagGain.setCurrentAndTargetValue(1.0f);

    masterLFO.prepare(sr, maxBlockSize);
    wasAnyNoteHeld = false;
    lfoTimeAccumulator = 0.0;

    dryNoise.Init((float)sr);
    dryNoise.mPinkEnabled = true;
    dryNoise.SetHighShelf(500.0f, -16.0f, (float)sr);
    dryRipple.SetMainsHz(60.0f, (float)sr);
    dryRipple.SetAmplitudes(1.8e-5f, 8.9e-6f, 6.3e-6f);

    if (isSuperSix())
        tapeEcho.prepare(sr, 2, 512);
}

void JunoEngine::reset()
{
    voiceManager.resetAllVoices();
    chorus.reset();
    performanceState.noteOffFifo.reset();
    performanceState.noteOffBuffer.fill(0);
    dcBlocker.reset();
    smoothedSagGain.setCurrentAndTargetValue(1.0f);
    powerOnDelaySamples = 0;
    wasAnyNoteHeld = false;
    globalDriftAudible = 0.0f;
    thermalCounter = 0;
    thermalTarget = 0.0f;
    lfoTimeAccumulator = 0.0;
    chorusLfoPhaseI = 0.0f;
    chorusLfoPhaseII = 0.0f;
    arpEnabledCache = false;
}

void JunoEngine::panic()
{
    voiceManager.resetAllVoices();
    chorus.reset();
    performanceState.noteOffFifo.reset();
    performanceState.noteOffBuffer.fill(0);
}

void JunoEngine::setLfoCalibration(CalibrationSettings* cal)
{
    masterLFO.setCalibrationSettings(cal);
}

void JunoEngine::setTargetModel(int model)
{
    targetModel = model;
}

void JunoEngine::setHostInfo(double bpm, bool playing, double ppqPosition)
{
    hostBPM = bpm;
    hostPlaying = playing;
    hostBeatPos = ppqPosition;
}

void JunoEngine::setPortamentoEnabled(bool enabled)
{
    voiceManager.setPortamentoEnabled(enabled);
}

void JunoEngine::setPortamentoTime(float time)
{
    voiceManager.setPortamentoTime(time);
}

void JunoEngine::setPortamentoLegato(bool legato)
{
    voiceManager.setPortamentoLegato(legato);
}

void JunoEngine::setBenderAmount(float amount)
{
    voiceManager.setBenderAmount(amount);
}

void JunoEngine::triggerLfo()
{
}

void JunoEngine::noteOn(int channel, int midiNote, float velocity)
{
    if (arpEnabledCache && arpeggiator != nullptr) {
        arpeggiator->NoteOn(midiNote);
    } else {
        voiceManager.noteOn(channel, midiNote, velocity);
    }
}

void JunoEngine::noteOff(int channel, int midiNote, float velocity)
{
    juce::ignoreUnused(channel, velocity);
    if (arpEnabledCache && arpeggiator != nullptr) {
        arpeggiator->NoteOff(midiNote);
    } else {
        performanceState.handleNoteOff(midiNote, voiceManager);
    }
}

void JunoEngine::handleSustain(int value, bool inverted)
{
    if (inverted) value = 127 - value;
    performanceState.handleSustain(value);
}

void JunoEngine::flushSustain()
{
    performanceState.flushSustain(voiceManager);
}

float JunoEngine::getTotalEnvelopeLevel() const
{
    return voiceManager.getTotalEnvelopeLevel();
}

float JunoEngine::getChorusLfoPhase(int mode) const
{
    return (mode == 1) ? chorusLfoPhaseI : chorusLfoPhaseII;
}

bool JunoEngine::isAnyNoteHeld() const
{
    return voiceManager.isAnyNoteHeld();
}

void JunoEngine::setTapeEchoCalibration(const TapeEchoCal& cal)
{
    tapeEchoCal = cal;
}

void JunoEngine::updateThermalDrift(SynthParams& params)
{
    if (++thermalCounter > params.thermalInertia) {
        thermalCounter = 0;
        thermalTarget = (chorusNoiseGen.nextFloat() * 2.0f - 1.0f) * params.thermalIntensity;
    }
    globalDriftAudible += (thermalTarget - globalDriftAudible) * params.thermalMigration;
    params.thermalDrift = globalDriftAudible;
}

void JunoEngine::updateArpFromParams(const SynthParams& params)
{
    arpEnabledCache = params.arpEnabled && (params.modelArp != 2);
    arpeggiator->mEnabled = arpEnabledCache;
    arpeggiator->mMode = params.arpMode;
    arpeggiator->mRange = params.arpRange;
    arpeggiator->mRate = JunoArpeggiator::arpRate(params.arpRate);
    arpeggiator->mSyncToHost = params.arpSync;
    if (params.arpSync) {
        arpeggiator->mHostPlaying = hostPlaying;
        arpeggiator->mHostBPM = hostBPM;
        arpeggiator->mHostBeatPos = hostBeatPos;
        arpeggiator->mDivision = params.arpDivision;
    }
}

void JunoEngine::processArpeggiator(int numSamples)
{
    if (arpeggiator == nullptr)
        return;

    arpeggiator->Process(numSamples,
        [this](int note, int) {
            voiceManager.noteOn(0, note, 0.8f);
        },
        [this](int note, int) {
            voiceManager.noteOff(0, note, 0.0f);
        }
    );
}

void JunoEngine::process(SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples, bool lfoTrig)
{
    if (numSamples <= 0)
        return;

    updateThermalDrift(params);

    voiceManager.updateParams(params);

    voiceManager.setPortamentoEnabled(params.portamentoOn);
    voiceManager.setPortamentoTime(params.portamentoTime);
    voiceManager.setPortamentoLegato(params.portamentoLegato);
    voiceManager.setBenderAmount(params.benderValue);

    updateArpFromParams(params);
    processArpeggiator(numSamples);

    renderAudio(params, buffer, numSamples, lfoTrig);
    applyChorus(params, buffer, numSamples);
    processMasterEffects(params, buffer, numSamples);
    applyTapeEcho(params, buffer, numSamples);
    applyNoiseFloor(params, buffer, numSamples);
}

void JunoEngine::renderAudio(SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples, bool lfoTrig)
{
    using namespace JunoConstants;
    const double sr = sampleRate;

    bool anyHeld = voiceManager.isAnyNoteHeld();

    masterLFO.setDepth(1.0f);
    masterLFO.setDelay(params.lfoDelay * params.lfoDelayMax);
    masterLFO.updateGateState(anyHeld, lfoTrig);

    float tickRateMs = params.lfoTickRateMs;
    double tickRateHz = 1000.0 / (double)tickRateMs;
    double samplesPerTick = sr / tickRateHz;

    for (int i = 0; i < numSamples; ++i) {
        lfoTimeAccumulator += 1.0;
        if (lfoTimeAccumulator >= samplesPerTick) {
            lfoTimeAccumulator -= samplesPerTick;
            masterLFO.tick106();
        }

        float lfoVal = masterLFO.process(params.lfoRate);

        float res = params.lfoResolution;
        if (res > 1.0f) {
            lfoVal = std::round(lfoVal * res) / res;
        }

        lfoBuffer[i] = lfoVal;
    }

    voiceManager.renderNextBlock(buffer, 0, numSamples, lfoBuffer);
}

void JunoEngine::applyChorus(const SynthParams& params, juce::AudioBuffer<float>& buffer, int /*numSamples*/)
{
    int chorusModel = GET_MODEL_CHORUS(params);
    if (chorusModel != 2) return;

    int activeMode = (params.chorusCycleMode >= 0) ? params.chorusCycleMode : params.chorusMode;
    ChorusBBD::Mode mode = static_cast<ChorusBBD::Mode>(activeMode);

    chorus.setMode(mode);
    chorus.setHissLevel(params.chorusHissLvl);
    chorus.setHissMultiplier(params.chorusHiss);

    if (mode == ChorusBBD::Mode::Off)
        return;

    chorus.setCalibrationParams(params.chorusDelayI,
                                params.chorusDelayII,
                                params.chorusGainDry,
                                params.chorusGainWet,
                                params.chorusModDepth,
                                params.chorusSatBoost,
                                params.chorusFilterCutoff,
                                params.chorusBothRate);

    float rate = (mode == ChorusBBD::Mode::ChorusII) ? params.chorusLfoRateII : params.chorusLfoRate;
    if (mode == ChorusBBD::Mode::ChorusBoth) rate = params.chorusBothRate;

    chorus.setRate(rate);
    chorus.setDepth(1.0f);
    chorus.setMix(params.chorusMix);

    chorus.process(buffer);
}

void JunoEngine::processMasterEffects(SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples)
{
    float masterVol = params.masterVolume * params.masterOutputGain;
    float envSum = voiceManager.getTotalEnvelopeLevel();
    float finalSag = 1.0f - (envSum * params.vcaSagAmt);
    finalSag = juce::jlimit(0.7f, 1.0f, finalSag);
    smoothedSagGain.setTargetValue(finalSag);

    buffer.applyGain(smoothedSagGain.getNextValue() * masterVol);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        float* d = buffer.getWritePointer(ch);
        if (params.lowCpuMode) {
            for (int i = 0; i < numSamples; ++i) {
                float x = juce::jlimit(-1.5f, 1.5f, d[i] * 1.1f);
                d[i] = x - (x * x * x) * 0.333333f;
            }
        } else {
            for (int i = 0; i < numSamples; ++i) {
                d[i] = std::tanh(d[i] * 1.1f);
            }
        }
    }

    if (buffer.getNumChannels() > 1) {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        float bleed = params.stereoBleed;
        for (int i = 0; i < numSamples; ++i) {
            float vL = l[i], vR = r[i];
            l[i] = vL + vR * bleed;
            r[i] = vR + vL * bleed;
        }
    }

    if (powerOnDelaySamples < (int)(0.080f * sampleRate)) {
        if (powerOnDelaySamples == 0) {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addSample(ch, 0, 4.0f);
        }
        powerOnDelaySamples += numSamples;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    dcBlocker.process(context);
}

void JunoEngine::applyTapeEcho(SynthParams& params, juce::AudioBuffer<float>& buffer, int /*numSamples*/)
{
    if (!isSuperSix())
        return;

    if (params.delayEnabled)
    {
        tapeEcho.setEnabled(true);
        tapeEcho.setDelaySetting(params.delaySetting);
        tapeEcho.setRepeatRate(params.delayRepeatRate);
        tapeEcho.setIntensity(params.delayIntensity);
        tapeEcho.setBass(params.delayBass);
        tapeEcho.setTreble(params.delayTreble);
        tapeEcho.setReverbVol(params.delayReverbVol);
        tapeEcho.setEchoVol(params.delayEchoVol);
        tapeEcho.setEchoCancel(params.delayEchoCancel);
        tapeEcho.setSyncEnabled(params.delaySyncEnabled);
        tapeEcho.setSyncDivision(params.delaySyncDivision);

        if (params.delaySyncEnabled) {
            tapeEcho.setHostBPM(hostBPM);
        }

        tapeEcho.setInputLevel(tapeEchoCal.delayInputLevel);
        tapeEcho.setWetDry(tapeEchoCal.delayWetDry);
        tapeEcho.setReverbType(params.delayReverbType);
        tapeEcho.setWowFlutter(params.delayWowFlutter);
        tapeEcho.setReverbDecay(params.delayReverbDecay);
        tapeEcho.setEchoIsolator(params.delayEchoIsolator);

        tapeEcho.setWowRate(tapeEchoCal.delayWowRate);
        tapeEcho.setFlutterRate(tapeEchoCal.delayFlutterRate);
        tapeEcho.setTapeScrapeRate(tapeEchoCal.delayTapeScrapeRate);
        tapeEcho.setWowAmp(tapeEchoCal.delayWowAmp);
        tapeEcho.setFlutterAmp(tapeEchoCal.delayFlutterAmp);
        tapeEcho.setTapeScrapeAmp(tapeEchoCal.delayTapeScrapeAmp);
        tapeEcho.setWowFlutterScale(tapeEchoCal.delayWowFlutterScale);
        tapeEcho.setSaturationInputGain(tapeEchoCal.delaySaturationInputGain);
        tapeEcho.setHead2Ratio(tapeEchoCal.delayHead2Ratio);
        tapeEcho.setHead3Ratio(tapeEchoCal.delayHead3Ratio);
        tapeEcho.setBassFreq(tapeEchoCal.delayBassFreq);
        tapeEcho.setTrebleFreq(tapeEchoCal.delayTrebleFreq);
        tapeEcho.setFeedbackLpfBase(tapeEchoCal.delayFeedbackLpfBase);
        tapeEcho.setFeedbackLpfRange(tapeEchoCal.delayFeedbackLpfRange);
        tapeEcho.setSpringGain(tapeEchoCal.delaySpringGain);
        tapeEcho.setSpringReflectionScale(tapeEchoCal.delaySpringReflectionScale);
        tapeEcho.setSchroederLpf(tapeEchoCal.delaySchroederLpf);
        tapeEcho.setSchroederGain(tapeEchoCal.delaySchroederGain);
        tapeEcho.setSchroederSatDrive(tapeEchoCal.delaySchroederSatDrive);

        tapeEcho.process(buffer);
    }
    else
    {
        tapeEcho.setEnabled(false);
    }
}

void JunoEngine::applyNoiseFloor(const SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples)
{
    float dbScale = std::pow(10.0f, (params.masterNoise + 80.0f) / 20.0f);
    float dryNoiseVol = 0.0015f * params.noiseFloorMul * dbScale;
    float rippleVol = params.mainsRippleMul;

    for (int i = 0; i < numSamples; ++i) {
        float n = dryNoise.Process() * dryNoiseVol;
        n += dryRipple.Process() * rippleVol;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            buffer.getWritePointer(ch)[i] += n;
        }
    }
}
