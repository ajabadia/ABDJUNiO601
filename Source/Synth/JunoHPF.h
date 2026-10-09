#pragma once

/*
  ==============================================================================

    JunoHPF.h (Shim de compatibilidad para ABDJUNiO601)

    La implementación matemática y física canónica del filtro pasa-altos Juno
    ha sido unificada en ABDSharedCode/DspCore/DspJunoHPF.h:
      - Simulación ngspice de conmutación CD4051B para Juno-60.
      - Posiciones conmutadas para Juno-106 con centinela Bass Boost.
      - Curva monótona PCHIP para modo continuo Juno-6.
      - 100% C++ puro estándar, libre de dependencias de JUCE.

  ==============================================================================
*/

#include "DspCore/DspJunoHPF.h"

using HPFMode         = abd::dsp::HPFMode;
using BassBoostFilter = abd::dsp::BassBoostFilter;
using JunoHPF         = abd::dsp::JunoHPFSwitched;

using abd::dsp::getJuno60HPFFreq;
using abd::dsp::getJuno106HPFFreq;
using abd::dsp::getJuno6HPFFreqPCHIP;
