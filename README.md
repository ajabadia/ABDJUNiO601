# JUNiO 601

A hardware-authentic emulation of the legendary Roland Juno-106 synthesizer, built using the JUCE framework. This project focuses on absolute sonic fidelity, bidirectional SysEx communication, and replicating the original hardware's diagnostic and performance behaviors.

## Technical Architecture

The emulator is designed around a fixed 6-voice polyphonic engine, strictly mimicking the original 80017A VCF/VCA voice chips and DCO architecture.

### Audio Signal Path
The signal path preserves the unique Juno-106 topology:
1. **DCO (Digitally Controlled Oscillator)**: Authentic Pulse (with PWM), Sawtooth, and Sub-oscillator waveforms with phase synchronization on Note-On.
2. **Noise Generator**: White noise source for percussive or textured sounds.
3. **HPF (High Pass Filter)**: 4-step selector (0-3) exactly mimicking the hardware logic (Bass Boost, Flat, 225Hz, 450Hz).
4. **VCF (Voltage Controlled Filter)**: 24dB/oct resonant low-pass filter with envelope modulation and keyboard tracking. 
5. **VCA (Voltage Controlled Amplifier)**: Switchable between Envelope or Gate mode, following the original hardware's bias characteristics.
6. **LFO**: Monophonic Global LFO phase synchronization across all voices with individual per-voice delay (fade-in) ramps.
7. **Chorus**: Dual-mode analog-modeled bucket-brigade delay (BBD) chorus with authentic background "hiss" emulation.
8. **DC Blocker**: Final stage cleanup to ensure audio stability.

## Hardware Authenticity Features

### SysEx Implementation (Roland Protocol)
Complete bidirectional communication for hardware integration:
- **Parameter Change (0x32)**: Real-time outgoing and incoming SysEx updates for all faders and switches.
- **Patch Dump (0x30)**: Full 18-byte patch encoding/decoding, compatible with original hardware dumps and tape backups.
- **Manual Mode (0x31)**: Dedicated "MANUAL" button to synchronize the sound with the current physical UI state.
- **Corrected bit-mapping**: Follows the official service manual for SW1 and SW2 (addressing HPF, VCA, and Chorus bit discrepancies).

### Performance & Memory
- **Group A/B Selection**: Authentic memory management with 128 patches divided into two groups of 64.
- **Random Generator**: A specialized "Musical Random" button that generates usable, aesthetically pleasing patches by following Juno-specific synthesis rules.
- **Panic Button**: "ALL OFF" functionality to instantly reset all voice states.

### Authentic Test Mode
Replicates the diagnostic "Test Mode" used by technicians:
- **Assignment Modes**:
    - **POLY 1**: Round-robin rotary assignment.
    - **POLY 2**: Static assignment (lowest-free).
    - **UNISON**: 6-voice stacked mode.
- **Digital Display**: A dedicated `JunoLCD` provides real-time "Group-Bank-Patch" and parameter feedback.
- **Calibration Macros**: BANK buttons 1-8 are mapped to specific service manual calibration programs.

## Standard MIDI Support
- **CC 1 (Modulation)**: Mapped to the LFO depth lever (Mod Wheel).
- **CC 64 (Sustain)**: Implements intelligent note-off queuing for authentic pedal behavior.
- **MIDI Learn**: Right-click any UI element to bind it to a hardware CC.

## Build Requirements
- **Framework**: JUCE 8 (Compatible with JUCE 7.x)
- **Platform**: Windows / macOS / Linux (Standalone and Plug-in)
- **Compiler**: MSVC / Clang / GCC (clean build with **zero warnings**)
- **Build System**: Automatic build counter and versioning via `build_standalone.bat`.

## Factory Preset Recovery
The original Juno‑106 ROM contains 128 factory patches stored in binary `.106` files. These files use a custom format with a `!j106\` header followed by a sequence of patch entries (name string + 18‑byte parameter block). A helper script `generate_factory_presets.py` can parse the file `factory patches.106` and generate a complete `FactoryPresets.h` with all 128 entries.

To regenerate the presets, run:
```bash
python generate_factory_presets.py
```
The script extracts only entries whose name starts with `A` (factory patches) and writes them to `Source/Core/FactoryPresets.h`. Other `.106` files (e.g., user libraries) may contain a different number of patches and are not processed by this script.

---
*Developed by ABD - Advanced Agentic Coding Project*
