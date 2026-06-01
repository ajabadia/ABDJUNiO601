# ABD JUNiO Super SIX

A high-fidelity unified hybrid emulation of the classic Roland Juno series (Juno-6, Juno-60, and Juno-106), built using the JUCE 8 framework. **Super SIX** represents the new era of the emulator, combining the distinct analog behaviors, components, and characteristics of all three vintage models into a single modular and conmutable synthesizer engine.

---

## The Unified Engine & Hybrid Architecture

Instead of forcing a single model emulation, **Super SIX** allows users to mix, match, and route components dynamically via the **ROUTING** panel or load authentic hardware eras instantly using calibration profiles:

### 1. Conmutable DSP Sections (Model Selection)
*   **DCO (Oscillator)**: Choose between discrete analog oscillators with thermal drift or quartz-stable DCOs. Switchable saw/pulse polarity and custom mixing ratios.
*   **HPF (High Pass Filter)**: 
    *   *Juno-6 mode*: Continuous frequency sweep slider (interpolated via hardware-calibrated PCHIP tables from 38.6 Hz to 1.39 kHz).
    *   *Juno-60 mode*: 4-position discrete switch without bass boost (position 0 is flat/bypass).
    *   *Juno-106 mode*: 4-position discrete switch with the active analog low-end shelving boost (+10.5 dB at 59 Hz) in position 0.
*   **VCF (Voltage Controlled Filter)**: Conmutable 4-pole ZDF TPT ladder topologies modeling the **IR3109** (bright, self-oscillating, screaming resonance) vs. the **80017A** (warm, smooth saturation). Integrated with 2x/4x polyphase IIR oversampling and adaptive thermal noise to prime filter auto-oscillation.
*   **ADSR (Envelopes)**: Toggle between the **J6/60 analog RC curve** (ultra-fast, percussive attack discharging towards -0.1V) and the **J106 digital MCU emulated envelope** (234.2 Hz tick resolution, 14-bit DAC steps, and hardware-correct 8-bit CPU multiplication truncation in `CalcDecay` leading to a 6% faster decay).
*   **Chorus (BBD)**: Simulates Bucket-Brigade Delay lines (MN3009). Select J60 or J106 BBD settings with realistic clock rate variations, charge transfer efficiency loss (CTE), input saturation, and turnaround clicks resonant at 30 Hz (`ClickRing`). Soportes for Mono-line both-mode (I+II) running at 7.85 Hz.
*   **Arpeggiator & Portamento**: Run the classic hardware arpeggiator (Juno-6/60) and polyphonic portamento / unison modes (Juno-106) simultaneously—expanding performance capabilities beyond original hardware limits.

---

## Key Features

### 1. Dynamic Skin & Theme Manager
The visual chassis adapts dynamically to the selected calibration profile, enforcing accurate hardware aesthetics:
*   **Juno-6 Profile**: Locks UI skin to **Juno-6 Analog** (mahogany wood side cheeks) and disables manual selection.
*   **Juno-60 Profile**: Locks UI skin to **Juno-60 Classic** (cherry wood side cheeks) and disables manual selection.
*   **Juno-106 Profile**: Restricts skin selection exclusively to **Juno-106 Classic** and **Juno-106S Dark** (metal side cheeks).
*   **Super SIX (Hybrid Mode)**: Unlocks full access to all 9 custom skin themes (Classic Blue, Space Echo, TR-808, Deepmind, ARP 2600, etc.), defaulting to **Classic Blue** (walnut wood cheeks). Enables hybrid component modeling colors in this theme.

### 2. Expanded 7x3 Digital LCD Display
The user interface features a custom 3-digit vintage LED display (`JunoLCD`) supporting a 26-bank layout:
*   **Bancos A-Z**: Letters A to Z containing 64 patches each (1,664 slots total).
*   **Display indicators**: Shows Bank letter + Patch indices (e.g., `A11` - `Z88`), parameter values, SysEx diagnostics, and test mode status.

### 3. Universal Preset & Bank Importer
*   **TAL-U-No-LX (`.pjunoxl`)**: Direct XML parsing to load single patches or bulk banks.
*   **Roland SysEx (`.syx` / `.mid`)**: Bidirectional 18-byte MIDI dump parsing.
*   **FSK Tape Backups (`.wav` / `.aif`)**: Decodes and encodes authentic 1980s cassette interface data (1300 Hz / 2100 Hz FSK).
*   **CSV Banks (`.csv`)**: Text-based bulk preset management.

---

## Technical Specifications
*   **Framework**: JUCE 8 (Compatible with JUCE 7.x)
*   **Audio Core**: 6-voice polyphonic engine built on the `ABD-SynthEngine` platform.
*   **SysEx Protocol**: Authentic Roland SysEx command implementation (IPR/APR support with correct SW1/SW2 bitmappings).
*   **Build Target**: Toggle compiler configurations via `JUNO_TARGET_MODEL` in `Source/Core/JunoModelConfig.h`:
    *   `0`: ABD JUNiO Super SIX (Unified Hybrid)
    *   `1`: ABD JUNiO 601 (Juno-106)
    *   `2`: ABD JUNiO 06 (Juno-60)
    *   `3`: ABD JUNiO SIX (Juno-6)

---
*Developed by **ABD-IA** - Advanced Agentic Coding Project*
**Version: 2.0.0 (Unified Hybrid Era)**
