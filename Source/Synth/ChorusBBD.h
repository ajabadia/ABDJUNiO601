#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>

/**
 *  JUNiO 601 — Chorus BBD
 *
 *  Emula el chip MN3009 (utilizado en el coro del Juno-106):
 *  - Dos líneas de retardo por modo (L+R).
 *  - Chorus I: ~3.2ms delay base, LFO ~0.5Hz.
 *  - Chorus II: ~6.4ms delay base, LFO ~0.8Hz.
 *  - Chorus I+II: Modo combinado.
 *
 *  Características de fidelidad:
 *  - Interpolación cúbica Hermite.
 *  - Saturación NE570 estilo Juno (tanh).
 *  - Filtros de reconstrucción post-BBD (8kHz).
 *  - LFO en cuadratura (0, 180, 90, 270 grados).
 */
class ChorusBBD
{
public:
    enum class Mode { Off, ChorusI, ChorusII, ChorusBoth };

    ChorusBBD();
    ~ChorusBBD() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    
    void setMode(Mode m)        { mode = m; }
    void setRate(float hz)      { lfoRate = juce::jlimit(0.1f, 8.0f, hz); }
    void setDepth(float d)      { m_depth = juce::jlimit(0.0f, 1.0f, d); }
    void setMix(float w)        { wetMix = juce::jlimit(0.0f, 1.0f, w); }
    void setHissLevel(float db) { hissLvlDb = juce::jlimit(-96.0f, -40.0f, db); }
    void setHissColor(float color) { calHissColor = color; }
    
    // [Build 29] Calibration Overrides
    void setCalibrationParams(float dI, float dII, float depth, float sat, float cutoff) {
        calDelayI = dI;
        calDelayII = dII;
        calModDepth = depth;
        calSatBoost = sat;
        calFilterCutoff = cutoff;
    }

    void setHissMultiplier(float m) { hissMultiplier = juce::jlimit(0.0f, 2.0f, m); }

    /** Procesa buffer estéreo in-place */
    void process(juce::AudioBuffer<float>& buffer);

private:
    //-- Parameters ---------------------------------------------------------
    Mode  mode     { Mode::Off };
    float lfoRate  { 0.513f };
    float m_depth    { 0.65f  };
    float wetMix   { 0.50f  };
    float hissLvlDb { -52.0f };
    double sr      { 44100.0 };

    // [Build 29] Calibration Values
    float calDelayI { 3.2f };
    float calDelayII { 6.4f };
    float calModDepth { 1.5f };
    float calSatBoost { 1.2f };
    float calFilterCutoff { 8000.0f };

    //── Delay lines ────────────────────────────────────────────────────────
    static constexpr int MAX_DELAY_SAMPLES = 8192; // 100ms+ a 48k para seguridad
    static constexpr float DELAY_I_MS  = 3.2f;
    static constexpr float DELAY_II_MS = 6.4f;
    static constexpr float MOD_DEPTH_MS = 1.5f;

    struct DelayLine
    {
        std::vector<float> buf;
        int writePos { 0 };

        void init(int size) { buf.assign(size, 0.0f); writePos = 0; }
        void reset() { std::fill(buf.begin(), buf.end(), 0.0f); writePos = 0; }

        void write(float sample)
        {
            if (buf.empty()) return;
            buf[writePos] = sample;
            if (++writePos >= (int)buf.size()) writePos = 0;
        }

        float read(float delaySamples) const
        {
            if (buf.empty()) return 0.0f;
            int size = (int)buf.size();
            float rPos = (float)writePos - delaySamples;
            while (rPos < 0.0f) rPos += (float)size;
            while (rPos >= (float)size) rPos -= (float)size;

            int i1 = (int)rPos;
            int i0 = (i1 - 1 + size) % size;
            int i2 = (i1 + 1) % size;
            int i3 = (i1 + 2) % size;

            float frac = rPos - (float)i1;
            
            // Hermite cúbico
            float y0 = buf[i0], y1 = buf[i1], y2 = buf[i2], y3 = buf[i3];
            float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
            float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            float a2 = -0.5f * y0 + 0.5f * y2;
            float a3 = y1;

            return ((a0 * frac + a1) * frac + a2) * frac + a3;
        }
    };

    DelayLine lineI_L, lineI_R;
    DelayLine lineII_L, lineII_R;

    //── LFO ────────────────────────────────────────────────────────────────
    double lfoPhase { 0.0 };

    //── Post-filter (1-polo LP ~8kHz) ──────────────────────────────────────
    struct OnePoleLP
    {
        float b0 { 1.f }, a1 { 0.f }, z1 { 0.f };
        void prepare(double sampleRate, float cutoffHz)
        {
            float w = std::tan(juce::MathConstants<float>::pi * cutoffHz / (float)sampleRate);
            b0 = w / (1.f + w);
            a1 = (w - 1.f) / (w + 1.f);
        }
        void reset() { z1 = 0.f; }
        float process(float x)
        {
            float y = b0 * x + b0 * z1 - a1 * z1;
            z1 = y;
            return y;
        }
    };

    OnePoleLP filterI_L, filterI_R, filterII_L, filterII_R;

    //── Saturation & Noise ────────────────────────────────────────────────
    inline float saturate(float x) const { return std::tanh(x * calSatBoost); }

    juce::Random random;
    float hissMultiplier { 1.0f };
    float calHissColor { 0.4f };
    float noiseFilterL { 0.0f }, noiseFilterR { 0.0f };
};
