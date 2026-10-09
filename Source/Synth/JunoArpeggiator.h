/*
  ==============================================================================

    JunoArpeggiator.h
    Roland Juno arpeggiator compatibility shim for ABDJUNiO601.
    Delegates to canonical abd::synth::ArpeggiatorJuno (ABDSharedCode/SynthCore).
    Zero heap allocations in the audio thread (RT-safe).

  ==============================================================================
*/

#pragma once

#include "SynthCore/ArpeggiatorJuno.h"

// ==============================================================================
// Alias canonical implementation from SynthCore
// ==============================================================================
using JunoArpeggiator = abd::synth::ArpeggiatorJuno;

// ==============================================================================
// Legacy compatibility enum and division beats for ABDJUNiO601 callers
// ==============================================================================
enum ArpDivision
{
    kDiv1   = static_cast<int>(abd::synth::ArpDivision::kDiv1),
    kDiv2   = static_cast<int>(abd::synth::ArpDivision::kDiv2),
    kDiv4   = static_cast<int>(abd::synth::ArpDivision::kDiv4),
    kDiv4T  = static_cast<int>(abd::synth::ArpDivision::kDiv4T),
    kDiv8   = static_cast<int>(abd::synth::ArpDivision::kDiv8),
    kDiv8T  = static_cast<int>(abd::synth::ArpDivision::kDiv8T),
    kDiv16  = static_cast<int>(abd::synth::ArpDivision::kDiv16),
    kDiv16T = static_cast<int>(abd::synth::ArpDivision::kDiv16T),
    kDiv32  = static_cast<int>(abd::synth::ArpDivision::kDiv32),
    kNumArpDivisions = abd::synth::kNumArpDivisionsCount
};

static constexpr auto& kDivBeats = abd::synth::kArpDivBeats;
