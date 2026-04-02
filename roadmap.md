# OMEGA Synth - New Era Roadmap

This roadmap outlines the evolution of the OMEGA synthesizer series, focusing on architectural modularity and high-fidelity sound engines. All points from the `newDev/0000.txt` index are incorporated here.

## Sprint 1: Architectural Foundation (ABD-SynthEngine)
**Goal**: Decouple core DSP from plugin-specific logic for future reuse.
**Reference**: [0010.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0010.txt)
- [x] Create `ABD-SynthEngine` core directory structure.
- [x] Implement `VoiceBase` and `VoiceAllocator` (Poly/Unison logic).
- [x] Implement `ADSRGeneric` and `LFOGeneric` (Parametric models).
- [x] Implement `PresetManagerBase` (JSON-based cross-synth bank manager).
- [x] Refactor JUNiO 601 to inherit from `ABD` core classes.
- [x] **Export**: Ensure DSP core is usable as an independent library.

## Sprint 2: High-Fidelity DSP (Fidelity 1.1)
**Goal**: Authentic analog behavior for core components.
**Reference**: [0004.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0004.txt), [0009.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0004.txt), [0011.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0011.txt)
- [x] **JunoVCF**: 4-pole OTA ladder model with auto-oscillation and octave tracking. (Ref: 0004.txt)
- [x] **JunoPortamento**: Exponential glide curve and hardware-accurate legato. (Ref: 0004.txt)
- [x] **ChorusBBD**: Dual-line BBD modeling with quadrature LFO. (Ref: 0009.txt)
- [x] **HPF Refinement**: Multi-stage filter with specific frequency steps and shelving boost. (Ref: 0011.txt)
- [x] **Sub-Oscillator**: Implement 8253-style flip-flop and verify phase alignment with DCO. (Ref: 0000.txt)
- [x] **Noise Source**: Characterize White/Pink noise and ensure correct pre-VCF mixing ratios. (Ref: 0000.txt)
- [x] **Advanced LFO**: Expanded waveforms (Stepped logic centered) and hardware-authentic Delay/Fade. (Ref: 0010.txt / 0000.txt)

## Sprint 3: Advanced Voice Management & Analog Mojo
**Goal**: Movement and texture through intentional instability.
**Reference**: [0004.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0004.txt), [0011.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0011.txt)
- [x] **Analog Drift**: Shared-DAC thermal drift simulation across voices (Finetuned frequency/magnitude). (Ref: 0004.txt / 0011.txt)
- [x] **Unison Detune**: Refined spread logic with inverse drift scaling. (Ref: 0004.txt)
- [x] **Velocity & Aftertouch**: Map MIDI expressivity to VCF/VCA per Juno-106 specs. (Ref: 0000.txt)
- [x] **Crosstalk**: Implement inter-voice interference (kVoiceCrosstalkAmount). (Ref: 0011.txt)
- [x] **ADSR Fidelity**: 10-bit DAC emulation and anti-click slew for smooth release.

## Sprint 4: Preset & State Intelligence
**Goal**: Robust session management and metadata.
**Reference**: [0005.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0005.txt), [0006.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0006.txt)
- [x] **Bank Manager**: Support for cross-synth bank management via `PresetManagerBase`. (Ref: 0005.txt)
- [x] **Metadata**: Authors, categories, and searchable tags for every patch. (Ref: 0005.txt)
- [x] **Musical Randomize**: Fixed ranges and musical consistency in randomization. (Ref: 0005.txt)
- [x] **State Versioning**: ValueTree-based migration and XML persistence. (Ref: 0006.txt)
- [x] **Full Serialization**: DAW state persistence and cross-platform JSON/XML export. (Ref: 0006.txt)
- [x] **Tape Simulation**: Reinstated robust FSK encoding/decoding for .wav tape backups.

- [x] **Service Mode & Calibration (Production Ready)**: Full diagnostic panel for voice/VCF/DCO testing. (Ref: 0002.txt)
    - [x] Matrix voice grid (8x2) and 2-column parameter layout.
    - [x] Integrated "Test Scale" sequencer for real-time auditioning.
    - [x] Implement VCF/DCO auto-tuning routines.
    - [x] **Persistent Logging Control**: ON/OFF selector in System Settings > General (Build 94).
- [ ] **Accessibility (Web & Native)**: Keyboard navigation, ARIA labels, and high-contrast themes. (Ref: 0007.1.txt)

## Sprint 6: Hardware Targets (Raspberry Pi)
**Goal**: Optimized performance for standalone embeds.
**Reference**: [0007.2.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0007.2.txt)
- [ ] **ARM Optimizations**: NEON/SIMD instructions for core DSP.
- [ ] **Standalone Infrastructure**: JACK/ALSA configuration and GPIO Display support.
- [ ] **Display Mirroring**: Physical 7-segment display control via GPIO.
- [ ] **Standalone UI**: Fixed-layout optimizations for small touchscreens.

## Sprint 7: Verification & Quality Assurance
**Goal**: Rock-solid stability across all platforms.
**Reference**: [0008.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0008.txt), [0009.txt](file:///D:/desarrollos/ABDJUNiO601/docs/newDev/0009.txt)
- [ ] **Integration Suites**: Full-chain validation (MIDI -> DSP -> UI -> State). (Ref: 0008.txt)
- [ ] **CPU Benchmarking**: Section-by-section budget analysis. (Ref: 0008.txt)
- [ ] **Host Validation**: Stress testing in Live, Reaper, and Logic Pro. (Ref: 0009.txt)

## Sprint 8: Professional Standards & Final Refactoring [COMPLETED]
**Goal**: Elevate codebase to production-grade quality and maintainability.
- [x] **Math Namespace Standardization**: Consistently use `std::` for all math functions (`std::tan`, `std::max`, `std::pow`) across `JunoVCF.cpp`, `JunoADSR.cpp`, and `JunoDCO.cpp`.
- [x] **JUCE Namespace Consistency**: Standardize on `juce::` prefixing rather than `using namespace juce` to improve code clarity.
- [x] **Header Include Hygiene**: Verify and clean up transitive includes (Audit `JunoVCF.h` and `JunoADSR.h`).
- [x] **Doxygen Documentation**: Add structured comments to all public headers for automatic documentation generation.
- [x] **Memory Audit**: Verified shared pointers and modern C++ practices.
- [x] **Logic Cleanup**: Refactored `Voice` and `JunoVCF` for professional readability.

## Post-Launch / Future Iterations
- [ ] **Responsive Engine**: Re-evaluate dynamic layouts for Desktop, Tablet, and Mobile.
- [ ] **Accessibility (Web & Native)**: Keyboard navigation, ARIA labels, and high-contrast themes.
- [ ] **Expansion**: Additional chorus modes or vintage "aging" profiles.
