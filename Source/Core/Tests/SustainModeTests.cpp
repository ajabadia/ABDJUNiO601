#include "../JunoEngine.h"
#include "../JunoVoiceManager.h"
#include "../PerformanceState.h"
#include "../SynthParams.h"
#include <JuceHeader.h>

class SustainModeTests : public juce::UnitTest
{
public:
    SustainModeTests()
        : juce::UnitTest("SustainMode", "Performance") {}

    // Banco de pruebas del motor completo: nota real -> audio real.
    // El modo del pedal se aplica dentro de JunoEngine::process(), asi que
    // cada banco calienta unos bloques antes de tocar el pedal.
    struct Rig
    {
        JunoEngine engine;
        SynthParams params;

        explicit Rig(int mode)
        {
            engine.prepare(44100.0, 512);
            params.sustainMode = mode;
            // Sin arpegiador: con el arp activo las notas van al arp y nunca
            // llegan al gestor de voces ni al estado del pedal.
            params.arpEnabled    = false;
            params.modelArp      = -1;
            params.noiseFloorMul = 0.0f; // silencio de verdad al soltar
            params.sawOn         = true;
            params.pulseOn       = false;
            params.subOscLevel   = 0.0f;
            params.noiseLevel    = 0.0f;
            params.attack        = 0.005f;
            params.decay         = 0.05f;
            params.sustain       = 0.8f;
            params.release       = 0.0f; // slider 0 = caida mas rapida (kDecRelTable)
            params.vcfFreq       = 0.85f;
            params.envAmount     = 0.25f;
            params.resonance     = 0.0f;
            params.vcfLFOAmount  = 0.0f;
            params.lfoToVCF      = 0.0f;
            params.chorusMode    = 0;
            // Sin eco/reverb: la cola del tape echo dura segundos y tapa la medida de silencio.
            params.lfoDelay          = 0.0f;
            params.lfoDelayMax       = 0.0f;
            params.chorusDelayI      = 0.0f;
            params.chorusDelayII     = 0.0f;
            params.delayEnabled      = false;
            params.delayRepeatRate   = 0.0f;
            params.delayIntensity    = 0.0f;
            params.delayBass         = 0.0f;
            params.delayTreble       = 0.0f;
            params.delayReverbVol    = 0.0f;
            params.delayEchoVol      = 0.0f;
            params.delayEchoCancel   = false;
            params.delaySyncEnabled  = false;
            params.delayWowFlutter   = 0.0f;
            params.delayReverbDecay  = 0.0f;
            params.delayEchoIsolator = 0.0f;
            params.numVoices         = 8;
            params.vcaLevel          = 0.8f;

            // 10 bloques = 116 ms: pasa el "thump" de encendido (80 ms) y
            // deja el motor estable antes de medir.
            render(10);
        }

        float render(int blocks)
        {
            juce::AudioBuffer<float> buffer(2, 512);
            float peak = 0.0f;

            for (int b = 0; b < blocks; ++b)
            {
                buffer.clear();
                engine.process(params, buffer, 512, false);

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < 512; ++i)
                        peak = juce::jmax(peak, std::abs(buffer.getSample(ch, i)));
            }
            return peak;
        }

        float renderRms(int blocks)
        {
            juce::AudioBuffer<float> buffer(2, 512);
            double acc = 0.0;
            int n      = 0;

            for (int b = 0; b < blocks; ++b)
            {
                buffer.clear();
                engine.process(params, buffer, 512, false);

                for (int i = 0; i < 512; ++i)
                {
                    const double s = 0.5 * (buffer.getSample(0, i) + buffer.getSample(1, i));
                    acc += s * s;
                    ++n;
                }
            }
            return (float)std::sqrt(acc / juce::jmax(1, n));
        }
        // Magnitud de una frecuencia concreta (Goertzel): comprueba contenido
        // espectral y no solo nivel.
        float toneMagnitude(double hz, int blocks)
        {
            juce::AudioBuffer<float> buffer(2, 512);
            double re = 0.0, im = 0.0;
            int n = 0;

            for (int b = 0; b < blocks; ++b)
            {
                buffer.clear();
                engine.process(params, buffer, 512, false);

                for (int i = 0; i < 512; ++i)
                {
                    const double x  = 0.5 * (buffer.getSample(0, i) + buffer.getSample(1, i));
                    const double ph = juce::MathConstants<double>::twoPi * hz * (double)n / 44100.0;
                    re += x * std::cos(ph);
                    im += x * std::sin(ph);
                    ++n;
                }
            }

            return (float)(std::sqrt(re * re + im * im) / juce::jmax(1, n));
        }
    };

    void runTest() override
    {
        // ---------------------------------------------------------------
        // Contabilidad del FIFO (rapido y determinista)
        // ---------------------------------------------------------------
        struct Fixture
        {
            ABD::JunoVoiceManager vm;
            ABD::PerformanceState ps;
            Fixture() { vm.prepare(44100.0, 512); }
        };

        beginTest("Normal: el pedal retiene los note-off hasta soltarlo");
        {
            Fixture f;
            f.ps.setMode(0);
            f.ps.handleNoteOn(60);
            f.ps.handleSustain(127);
            f.ps.handleNoteOff(60, f.vm);
            expectEquals(f.ps.noteOffFifo.getNumReady(), 1);
            f.ps.handleSustain(0);
            f.ps.flushSustain(f.vm);
            expectEquals(f.ps.noteOffFifo.getNumReady(), 0);
        }

        beginTest("SOS: solo se sostienen las notas que ya sonaban");
        {
            Fixture f;
            f.ps.setMode(1);
            f.ps.handleNoteOn(60);
            f.ps.handleSustain(127);
            f.ps.handleNoteOff(60, f.vm);
            expectEquals(f.ps.noteOffFifo.getNumReady(), 1);
            f.ps.handleNoteOn(64);
            f.ps.handleNoteOff(64, f.vm);
            expectEquals(f.ps.noteOffFifo.getNumReady(), 1);
            f.ps.handleSustain(0);
            f.ps.flushSustain(f.vm);
            expectEquals(f.ps.noteOffFifo.getNumReady(), 0);
        }

        beginTest("Toggle: la pisada conmuta y soltar no cambia el estado");
        {
            Fixture f;
            f.ps.setMode(2);
            f.ps.handleSustain(127);
            f.ps.handleNoteOn(60);
            f.ps.handleNoteOff(60, f.vm);
            expectEquals(f.ps.noteOffFifo.getNumReady(), 1);
            f.ps.handleSustain(0);
            expectEquals(f.ps.noteOffFifo.getNumReady(), 1);
            f.ps.handleSustain(127);
            f.ps.handleSustain(0);
            f.ps.flushSustain(f.vm);
            expectEquals(f.ps.noteOffFifo.getNumReady(), 0);
        }

        // ---------------------------------------------------------------
        // Audio real: el motor tiene que sonar y callarse donde toca
        // ---------------------------------------------------------------
        beginTest("Normal (audio): sin pedal la nota se apaga al soltarla");
        {
            Rig r(0);
            r.engine.noteOn(1, 60, 0.8f);
            const float on = r.render(20);

            r.engine.noteOff(1, 60, 0.0f);
            r.render(40); // se descarta la cola del release
            const float after = r.render(60);

            expect(on > 0.01f, "la nota debe sonar, peak=" + juce::String(on));
            expect(after < on * 0.15f,
                   "sin pedal el release debe apagarla (on=" + juce::String(on) + ", after=" + juce::String(after) + ")");
        }

        beginTest("Normal (audio): el pedal mantiene la nota tras el note-off");
        {
            Rig r(0);
            r.engine.handleSustain(127, false);
            r.engine.noteOn(1, 60, 0.8f);
            const float on = r.render(20);

            r.engine.noteOff(1, 60, 0.0f);
            const float held    = r.render(20);
            const float magHeld = r.toneMagnitude(261.63, 8); // C4
            const float magRef  = r.toneMagnitude(330.0, 8);  // sin componente

            expect(on > 0.01f, "la nota debe sonar, peak=" + juce::String(on));
            expect(held > on * 0.3f,
                   "con el pedal pisado debe seguir sonando (on=" + juce::String(on) + ", held=" + juce::String(held) + ")");

            r.engine.handleSustain(0, false);
            r.engine.flushSustain();
            r.render(40);
            const float released = r.render(60);
            const float magAfter = r.toneMagnitude(261.63, 8);

            expect(released < held * 0.15f,
                   "al soltar el pedal debe apagarse (held=" + juce::String(held) + ", released=" + juce::String(released) + ")");
            expect(magHeld > magRef * 5.0f,
                   "lo sostenido debe ser la nota, no ruido (nota=" + juce::String(magHeld) + ", referencia=" + juce::String(magRef) + ")");
            expect(magAfter < magHeld * 0.2f,
                   "la fundamental debe desaparecer al soltar (sostenida=" + juce::String(magHeld) + ", tras soltar=" + juce::String(magAfter) + ")");
        }

        beginTest("SOS (audio): la nota nueva no queda congelada");
        {
            Rig r(1);
            r.engine.noteOn(1, 60, 0.8f);
            r.render(20);

            r.engine.handleSustain(127, false); // congela la 60
            r.engine.noteOff(1, 60, 0.0f);
            const float held = r.render(20);
            expect(held > 0.01f,
                   "la nota pisada antes del pedal debe quedar sostenida, peak=" + juce::String(held));

            r.engine.noteOn(1, 67, 0.8f);
            const float both = r.renderRms(10);

            r.engine.noteOff(1, 67, 0.0f); // no estaba congelada
            r.renderRms(30);
            const float afterNew = r.renderRms(40);

            expect(afterNew < both * 0.92f,
                   "la nota nueva debe apagarse (ambas=" + juce::String(both) + ", tras soltar=" + juce::String(afterNew) + ")");
            expect(afterNew > 0.001f,
                   "la nota congelada debe seguir sonando (rms=" + juce::String(afterNew) + ")");

            r.engine.handleSustain(0, false);
            r.engine.flushSustain();
            r.renderRms(30);
            const float released = r.renderRms(40);

            expect(released < afterNew * 0.15f,
                   "al soltar el pedal se libera lo congelado (held=" + juce::String(afterNew) + ", released=" + juce::String(released) + ")");
        }

        beginTest("Toggle (audio): pisar dos veces libera, soltar no hace nada");
        {
            Rig r(2);
            r.engine.handleSustain(127, false); // conmuta a ON
            r.engine.noteOn(1, 60, 0.8f);
            r.render(20);

            r.engine.noteOff(1, 60, 0.0f);
            const float held = r.render(20);
            expect(held > 0.01f,
                   "con el pedal conmutado a ON la nota debe quedar sostenida, peak=" + juce::String(held));

            r.engine.handleSustain(0, false); // soltar no conmuta
            const float stillOn = r.render(10);
            expect(stillOn > held * 0.5f,
                   "soltar el pedal no debe liberar la nota (held=" + juce::String(held) + ", after release=" + juce::String(stillOn) + ")");

            r.engine.handleSustain(127, false); // segunda pisada -> OFF
            r.engine.flushSustain();
            r.render(40);
            const float released = r.render(60);

            expect(released < held * 0.15f,
                   "la segunda pisada debe liberar la nota (held=" + juce::String(held) + ", released=" + juce::String(released) + ")");
        }
    }
};

static SustainModeTests sustainModeTestsInstance;
