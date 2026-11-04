// Source/Core/LfoTables.h
#pragma once

#include <cstdint>

/**
 * LFO tables — single source of truth (Sprint 17 FW.8).
 *
 * Both tables are transcribed byte for byte from the master CPU ROM
 * (ABDJuno6ROMRE, "v2 b_2.ic37", uPD7811G) and re-verified 128/128 and 8/8
 * against ABDJuno6ROMRE/disasm/juno106_B_ic37_v2_ghidra.asm (FW.1).
 *
 * CalibrationSettings may replace either table at runtime from a CSV; when it
 * does not, both the calibration accessors and JunoLFO read from here.
 */
namespace LfoTables
{
// ROM 0C60_lfoSpeedTbl, master ROM (ic37) at 0x0C60.
// 128 entries x 16-bit little-endian, little-endian byte order in the ROM image.
extern const uint16_t kDefaultLfoSpeedTbl[128];

// ROM 0B30_LfoDelayRampTbl (idx = pot >> 4), master ROM (ic37) at 0x0B30.
// 8 entries x 16-bit little-endian.
extern const uint16_t kDefaultLfoRampTbl[8];

constexpr int kLfoSpeedTableSize = 128;
constexpr int kLfoRampTableSize  = 8;

// Clamping accessors (index is clamped to the table bounds, never wrapped).
uint16_t getLfoSpeedCoeff(int index) noexcept;
uint16_t getLfoRampIncrement(int index) noexcept;

} // namespace LfoTables

/*
 * Firmware RAM state map used by the LFO / delay path (FW.6).
 *
 *   ic37 ($FFxx)                       model
 *   --------------------------------   ---------------------------------------------
 *   $FF4B  LFO speed coefficient       JunoLFO::mIntCoeff (loaded from 0x0C60 at 0x019B)
 *   $FF4D  LFO delay / holdoff rate    JunoLFO::mHoldoffInc (ADSR attack increment)
 *   $FF3D  LFO delay ramp accumulator  JunoLFO::mRampAccum
 *   $FF43  LFO delay ramp increment    JunoLFO::mRampInc (loaded from 0x0B30)
 *   $FF3B  LFO triangle accumulator    JunoLFO::mIntAccum (0x0000..0x1FFF, wraps mod 0x4000)
 *   $FF39  LFO triangle polarity flag  JunoLFO::mIntPolarity / mIntRising
 *   $FF23  LFO holdoff accumulator     JunoLFO::mHoldoffAccum (mod 0x4000)
 *   $FF71  LFO output magnitude        JunoLFO::mAmpInt
 *
 *   ic2 ($FFxx)
 *   --------------------------------   ---------------------------------------------
 *   $FF90  gate / trigger shadow      JunoLFO::mWasGated
 *   $FF40  delay pot byte             JunoLFO::delay -> index (pot >> 4)
 *   $FF88  LFO depth byte             JunoLFO::depth
 *   $FF50  note-on armed flag         JunoLFO::mArmed
 *
 * Tick rate: the master loop runs one LFO step every 4.2335 ms (236.2 Hz), the
 * same period used by the ADSR; 234.2 Hz is the older, inconsistent figure and
 * is not used anywhere in the code (FW.4).
 */
