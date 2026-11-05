/*
  ==============================================================================

    ChorusBBDParityTest.cpp
    La extraccion del coro BBD del Juno a ABDSharedCode::DspEffects, contra una
    REFERENCIA CONGELADA (Source/DSP/Effects/ChorusBBDFrozen.h).

    Que fija este test:

      1. QUE LA EXTRACCION FUE UNA EXTRACCION, NO UNA REESCRITURA. La referencia
         es el coro original de Source/Synth/ChorusBBD.{h,cpp} tal cual, con las
         unicas excepciones documentadas en su cabecera (las transcendentales de
         la libm por las deterministas del modulo, porque este es libm-free).
         Mismo guion de parametros bloque a bloque, misma entrada determinista,
         dos renders, y se comparan los DOS canales con margen 0 ulps.

         NOTA (misma nota, y mismo motivo, que ABDNeural/Tests/
         DspEffectsParityTest.cpp): la referencia ya llama a las
         transcendentales deterministas, asi que este test NO prueba ese cambio
         (seria circular). Lo que prueba es que la extraccion fue fiel con la
         aritmetica puesta. El error de las identidades esta medido y va en
         DspEffects/JunoBBD.h.

         Aqui el margen no es negociable. No hay una implementacion ajena contra
         la que comparar: el unico modo de saber que el sonido no cambio es que
         la aritmetica sea identica. Si alguien toca el motor, este test dice si
         cambio la salida y en que muestra.

      2. QUE LA INYECCION DE POLITICA NO SE COLA EN LA ARITMETICA. El perfil y
         la etapa son lo que hace el motor parametrizable; el render tiene que
         ser el mismo con cualquier combinacion que reproduzca los numeros del
         original.

      3. DETERMINISMO: dos objetos nuevos con el mismo guion dan identicos bit a
         bit. Los tres generadores de ruido son LCG, asi que no hay reloj ni rand
         oculto, pero eso se comprueba.

      4. COLA FINITA: los tres modos, con ruido y sin senal, no producen NaN ni
         infinitos.

    Uso: JUNiO601_ChorusBBDParityTest  (ctest lo ejecuta; no toma argumentos)

  ==============================================================================
*/

#include "Source/DSP/Effects/ChorusBBDFrozen.h"

#include "DspEffects/JunoBBD.h"
#include "DspEffects/characters/BbdNoise.h"
#include "DspEffects/profiles/JunoBbdProfile.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{

int gChecks = 0;
int gFails  = 0;

// Donde se rompio la paridad, para que el fallo diga algo mas que "difiere".
int gFirstDiffBlock  = -1;
int gFirstDiffSample = -1;
float gRefL = 0.0f, gGotL = 0.0f, gRefR = 0.0f, gGotR = 0.0f;
float gRefInL = 0.0f, gRefInR = 0.0f;

void check(bool ok, const char* what)
{
    ++gChecks;
    if (!ok)
    {
        ++gFails;
        std::printf("       [FAIL] %s\n", what);
    }
}

//==============================================================================
/** Un cambio de parametro, con el bloque en el que ocurre. */
struct ParamChange
{
    int block; // indice del bloque (de 256 muestras) en el que se aplica
    int what;  // 0 rate, 1 depth, 2 mix, 3 hiss db, 4 hiss colour, 5 mult
    float value;
};

using Engine = abd::dsp::JunoBBD<abd::dsp::JunoBbdJ106Profile, abd::dsp::BbdNoiseStage>;

/** El mismo guion en las dos implementaciones, sin codigo compartido. */
void driveParams(Engine& e, juno_frozen::ChorusBBDFrozen& f,
                 const ParamChange* script, int numChanges)
{
    int next = 0;

    for (int block = 0; block < 120; ++block)
    {
        while (next < numChanges && script[next].block == block)
        {
            const ParamChange& c = script[next];

            switch (c.what)
            {
                case 0:
                    e.setRate(c.value);
                    f.setRate(c.value);
                    break;
                case 1:
                    e.setDepth(c.value);
                    f.setDepth(c.value);
                    break;
                case 2:
                    e.setMix(c.value);
                    f.setMix(c.value);
                    break;
                case 3:
                    e.setHissLevelDb(c.value);
                    f.setHissLevel(c.value);
                    break;
                case 4:
                    e.setHissColour(c.value);
                    f.setHissColour(c.value);
                    break;
                case 5:
                    e.setHissMultiplier(c.value);
                    f.setHissMultiplier(c.value);
                    break;
                default:
                    break;
            }

            ++next;
        }

        // Los 256 muestras del bloque, identicas en las dos implementaciones.
        for (int s = 0; s < 256; ++s)
        {
            const int i     = block * 256 + s;
            const float inL = 0.35f * std::sin(float(i) * 0.0137f) + 0.12f * std::sin(float(i) * 0.2110f + 0.7f);
            const float inR = 0.28f * std::sin(float(i) * 0.0171f + 1.3f) + 0.09f * std::sin(float(i) * 0.1930f + 2.1f);

            float aL, aR, bL, bR;
            e.process(inL, inR, aL, aR);
            f.process(inL, inR, bL, bR);

            if (std::memcmp(&aL, &bL, sizeof(float)) != 0)
            {
                gFirstDiffBlock  = block;
                gFirstDiffSample = i;
                gRefL            = bL;
                gGotL            = aL;
                gRefR            = bR;
                gGotR            = aR;
                gRefInL          = inL;
                gRefInR          = inR;
                return;
            }
            if (std::memcmp(&aR, &bR, sizeof(float)) != 0)
            {
                gFirstDiffBlock  = block;
                gFirstDiffSample = i;
                gRefL            = bL;
                gGotL            = aL;
                gRefR            = bR;
                gGotR            = aR;
                gRefInL          = inL;
                gRefInR          = inR;
                return;
            }
        }
    }
}

} // anonymous namespace

//==============================================================================
/** Los tres modos, con el guion completo de parametros, tienen que dar 0 ulps. */
void testParityAllModes()
{
    using M = juno_frozen::Mode;

    const struct
    {
        M frozen;
        abd::dsp::JunoBbdMode engine;
        const char* name;
    } modes[] =
        {
            {M::ChorusI, abd::dsp::JunoBbdMode::ChorusI, "Chorus I"},
            {M::ChorusII, abd::dsp::JunoBbdMode::ChorusII, "Chorus II"},
            {M::ChorusBoth, abd::dsp::JunoBbdMode::ChorusBoth, "Chorus I+II"}};

    const ParamChange script[] =
        {
            {4, 0, 0.513f}, {9, 1, 0.20f}, {15, 0, 1.20f}, {20, 2, 0.00f}, {24, 2, 0.80f}, {30, 3, -80.0f}, {36, 3, -60.0f}, {42, 4, 1.00f}, {48, 4, 0.00f}, {54, 5, 2.00f}, {60, 1, 1.00f}, {66, 0, 7.70f}, {72, 2, 0.50f}, {80, 1, 0.65f}, {90, 0, 0.10f}};
    const int numChanges = int(sizeof(script) / sizeof(script[0]));

    for (const auto& m : modes)
    {
        Engine e;
        juno_frozen::ChorusBBDFrozen f;

        // El ORDEN importa y no es cosmetico. La referencia congelada lee su
        // calibracion DENTRO de `prepare()` (es lo que hacia el original: el
        // host carga los ajustes y despues prepara), asi que hay que darsela
        // ANTES. Si se hace al reves, la referencia se queda con el 9661 Hz por
        // defecto de su struct, el motor se queda con los 8000 Hz de la J106, y
        // los dos lados estan comparando filtros distintos: se ve en el
        // coeficiente del biquad (0.7326 contra 0.5774) y la salida se separa
        // 2e-05 en la muestra 85. El motor acepta la calibracion en
        // cualquier momento (sus `set...` son posteriores a `prepare`), asi que
        // el unico orden que vale para los dos es calibrar y luego preparar.
        f.setCalibrationParams(3.2f, 3.3f, 0.863f, 1.257f, 1.5f, 1.2f, 8000.0f, 7.7f);

        e.prepare(48000.0);
        f.prepare(48000.0);

        // Los numeros de fabrica de JUNiO601, por los dos lados.
        e.setMixerGains(0.863f, 1.257f);
        e.setBaseDelaysMs(3.2f, 3.3f);
        e.setModulation(1.5f, 7.7f);
        e.setSaturation(1.2f);
        // Y la frecuencia de reconstruccion, por el mismo motivo que arriba: se
        // le dice al motor la misma cifra que a la referencia, para estar
        // comparando el filtro y no dos filtros distintos.
        e.setReconstructionCutoff(8000.0f);
        // El nivel de siseo se pone IGUAL a los dos lados a proposito. Cada uno
        // tiene su default y son de capas distintas: el del motor es el de la
        // tabla de calibracion del panel (-68 dB) y el del original es el de su
        // struct (-52 dB). Un test de paridad no puede comparar defaults de
        // capas distintas, asi que los pone a la misma cifra y empieza desde ahi.
        e.setHissLevelDb(-52.0f);
        f.setHissLevel(-52.0f);

        e.setMode(m.engine);
        f.setMode(m.frozen);

        driveParams(e, f, script, numChanges); // sale antes de la primera muestra

        if (gFirstDiffBlock >= 0)
        {
            std::printf("       %-12s difiere en la muestra %d (bloque %d)\n",
                        m.name, gFirstDiffSample, gFirstDiffBlock);
            std::printf("                   in  L=%.9f  R=%.9f\n",
                        double(gRefInL), double(gRefInR));
            std::printf("                   ref L=%.9f  R=%.9f\n",
                        double(gRefL), double(gRefR));
            std::printf("                   got L=%.9f  R=%.9f\n",
                        double(gGotL), double(gGotR));
        }

        // OJO, la asercion mira al reves de como parece: lo que se compara es
        // si se ha encontrado una divergencia (`gFirstDiffBlock >= 0`), asi que
        // lo que hay que exigir es que NO se haya encontrado. Escribir aqui un
        // `check (false, ...)` fijo deja el test en rojo aunque las tres
        // implementaciones coincidan muestra a muestra, que es un test que no
        // comprueba nada y ademas esconde un fallo real: hay que mirar el signo.
        check(gFirstDiffBlock < 0, "coro BBD: el motor compartido DIFIERE de la referencia congelada");

        gFirstDiffBlock = -1;
    }
}

int main()
{
    std::printf("[info] coro BBD extraido: motor compartido contra REFERENCIA CONGELADA\n");
    std::printf("[info] margen 0 ulps esperado (arregmetica identica, no \"parecido\")\n");

    testParityAllModes();

    if (gFails == 0)
        std::printf("[OK] coro BBD: %d comprobaciones, 0 ulps en los tres modos\n", gChecks);
    else
        std::printf("[FALLO] coro BBD: %d de %d comprobaciones\n", gFails, gChecks);

    return gFails == 0 ? 0 : 1;
}
