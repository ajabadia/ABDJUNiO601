#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "SynthParams.h"
#include "JunoVoiceManager.h"
#include "PerformanceState.h"
#include "JunoModelConfig.h"
#include "../Synth/ChorusBBD.h"
#include "../Synth/JunoLFO.h"
#include "../Synth/JunoTapeEcho.h"
#include "../Synth/JunoArpeggiator.h"

class CalibrationSettings;

class JunoEngine {
public:
    struct TapeEchoCal {
        float delayInputLevel = 1.0f;
        float delayWetDry = 1.0f;
        float delayWowRate = 5.0f;
        float delayFlutterRate = 15.0f;
        float delayTapeScrapeRate = 100.0f;
        float delayWowAmp = 0.02f;
        float delayFlutterAmp = 0.005f;
        float delayTapeScrapeAmp = 0.001f;
        float delayWowFlutterScale = 1.0f;
        float delaySaturationInputGain = 1.0f;
        float delayHead2Ratio = 0.8f;
        float delayHead3Ratio = 0.6f;
        float delayBassFreq = 500.0f;
        float delayTrebleFreq = 3000.0f;
        float delayFeedbackLpfBase = 8000.0f;
        float delayFeedbackLpfRange = 4000.0f;
        float delaySpringGain = 0.5f;
        float delaySpringReflectionScale = 0.3f;
        float delaySchroederLpf = 0.6f;
        float delaySchroederGain = 0.5f;
        float delaySchroederSatDrive = 0.3f;
    };

    JunoEngine();
    ~JunoEngine() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void panic();

    void process(SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples, bool lfoTrig);

    void noteOn(int channel, int midiNote, float velocity);
    void noteOff(int channel, int midiNote, float velocity);
    void handleSustain(int value, bool inverted);
    void flushSustain();

    void setHostInfo(double bpm, bool playing, double ppqPosition);

    void setPortamentoEnabled(bool enabled);
    void setPortamentoTime(float time);
    void setPortamentoLegato(bool legato);
    void setBenderAmount(float amount);

    void triggerLfo();

    float getTotalEnvelopeLevel() const;
    float getChorusLfoPhase(int mode) const;
    bool isAnyNoteHeld() const;

    void setTapeEchoCalibration(const TapeEchoCal& cal);
    void setLfoCalibration(CalibrationSettings* cal);
    void setTargetModel(int model);
    int getTargetModel() const { return targetModel; }
    bool isSuperSix() const { return targetModel == 0; }

    ABD::JunoVoiceManager& getVoiceManager() { return voiceManager; }
    ABD::PerformanceState& getPerformanceState() { return performanceState; }

private:
    void updateThermalDrift(SynthParams& params);
    void renderAudio(SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples, bool lfoTrig);
    void applyChorus(const SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples);
    void processMasterEffects(SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples);
    void applyTapeEcho(SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples);
    void applyNoiseFloor(const SynthParams& params, juce::AudioBuffer<float>& buffer, int numSamples);
    void updateArpFromParams(const SynthParams& params);
    void processArpeggiator(int numSamples);

    ABD::JunoVoiceManager voiceManager;
    ABD::PerformanceState performanceState;
    ChorusBBD chorus;
    JunoLFO masterLFO;
    std::unique_ptr<JunoArpeggiator> arpeggiator;
    JunoTapeEcho tapeEcho;
    AnalogFloorNoise dryNoise;
    RailRipple dryRipple;

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dcBlocker;
    juce::AudioBuffer<float> chorusNoiseBuffer;
    std::vector<float> lfoBuffer;
    juce::LinearSmoothedValue<float> smoothedSagGain;

    juce::Random chorusNoiseGen;
    juce::Random masterNoiseGen;

    float chorusLfoPhaseI = 0.0f;
    float chorusLfoPhaseII = 0.0f;
    int powerOnDelaySamples = 0;
    bool wasAnyNoteHeld = false;
    float globalDriftAudible = 0.0f;
    int thermalCounter = 0;
    float thermalTarget = 0.0f;
    double lfoTimeAccumulator = 0.0;

    double sampleRate = 44100.0;
    int targetModel = JUNO_TARGET_MODEL;
    bool arpEnabledCache = false;

    double hostBPM = 120.0;
    bool hostPlaying = false;
    double hostBeatPos = 0.0;

    TapeEchoCal tapeEchoCal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JunoEngine)
};
