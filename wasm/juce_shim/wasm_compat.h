/**
 * wasm_compat.h — Global compatibility header for JUCE + Emscripten WASM build.
 *
 * Force-included via -include flag in CMakeLists.txt for all compilation units.
 * Provides forward declarations and stubs for types that JUCE modules expect
 * but which are not available in a headless Emscripten build.
 */
#pragma once

#ifdef __EMSCRIPTEN__

#include <emscripten/emscripten.h>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <map>
#include <string>
#include <atomic>
#include <memory>

namespace juce
{
    using uint32 = unsigned int;
    using int64 = long long;
    using uint8 = unsigned char;
    using uint16 = unsigned short;
}

using juce::uint32;
using juce::int64;
using juce::uint8;
using juce::uint16;

#endif
