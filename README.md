# ABDSimpleJuno106

A hardware-authentic emulation of the legendary Roland Juno-106 synthesizer, built using the JUCE framework. This project focuses on absolute sonic fidelity, bidirectional SysEx communication, and replicating the original hardware's diagnostic behaviors.

## Technical Architecture

The emulator is designed around a voice-stealing polyphonic engine (6 voices), mimicking the original 80017A VCF/VCA voice chips and DCO architecture.

### Audio Signal Path
The signal path preserves the unique Juno-106 topology:
1. **DCO (Digitally Controlled Oscillator)**: Authentic Pulse (with PWM), Sawtooth, and Sub-oscillator waveforms.
2. **Noise Generator**: White noise source for percussive or textured sounds.
3. **HPF (High Pass Filter)**: 4-step selector (0-3) exactly mimicking the hardware logic.
4. **VCF (Voltage Controlled Filter)**: 24dB/oct resonant low-pass filter with envelope modulation and keyboard tracking. 
5. **VCA (Voltage Controlled Amplifier)**: Switchable between Envelope or Gate mode, following the original hardware's bias characteristics.
6. **Chorus**: Dual-mode analog-modeled bucket-brigade delay (BBD) chorus.
7. **DC Blocker**: Final stage cleanup to ensure audio stability.

## Hardware Authenticity Features

### SysEx Implementation (Roland Protocol)
Designed for bidirectional communication with original hardware or modern controllers:
- **Parameter Change (0x32)**: Real-time update of all faders and switches.
- **Patch Dump (0x30)**: Full 18-byte patch encoding/decoding, compatible with hardware memory.
- **Manual Mode (0x31)**: Triggers the "Manual" button behavior.
- **Corrected bit-mapping**: Follows the official service manual for SW1 and SW2 (addressing HPF, VCA, and Chorus bit discrepancies).

### Authentic Test Mode
Replicates the diagnostic "Test Mode" used by technicians:
- **Entry**: Hold the **Transpose** button while clicking the **Power** button.
- **Assignment Modes**:
    - **POLY 1**: Unison mode.
    - **POLY 2**: Non-rotary assignment (fixed voices).
    - **POLY 1+2**: Rotary assignment (cyclic).
- **Digital Display**: A dedicated `JunoLCD` provides real-time "CH x" feedback of the active voice channel.
- **Calibration Macros**: BANK buttons 1-8 are mapped to specific service manual calibration programs (Resonance self-oscillation, Noise level, Sub-osc check, etc.).

### Standard MIDI Support
- **CC 1 (Modulation)**: Mapped to the LFO depth lever.
- **CC 64 (Sustain)**: Implements intelligent note-off queuing for authentic pedal behavior.
- **MIDI Learn**: Right-click any UI element to bind it to a hardware CC.

## Factory Presets
Includes the complete original factory banks (A and B, 64 patches) decoded directly from the hardware 18-byte format. Use the `PresetBrowser` in the bottom panel to explore the iconic sounds of the 80s.

## Build Requirements
- **Framework**: JUCE 7.x
- **Platform**: Windows / macOS / Linux (Standalone and Plug-in)
- **Compiler**: MSVC / Clang / GCC (clean build with zero warnings)

---
*Developed by ABD - Advanced Agentic Coding Project*
