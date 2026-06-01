#pragma once

// Target Model Definition:
// 0 = Super SIX (Hybrid conmutable)
// 1 = 601 (Juno-106)
// 2 = 06 (Juno-60)
// 3 = SIX (Juno-6)
#ifndef JUNO_TARGET_MODEL
  #define JUNO_TARGET_MODEL 0  // Default to Super SIX (Unified Hybrid)
#endif

inline const char* getJunoModelName()
{
#if JUNO_TARGET_MODEL == 1
    return "ABD JUNiO 601";
#elif JUNO_TARGET_MODEL == 2
    return "ABD JUNiO 06";
#elif JUNO_TARGET_MODEL == 3
    return "ABD JUNiO SIX";
#else
    return "ABD JUNiO Super SIX";
#endif
}
