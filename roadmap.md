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

## Sprint 9: Deep Fidelity Audit & Hardware Verification [COMPLETED]
**Goal**: Bit-accurate and component-level alignment with original Roland Juno-106 hardware.
- [x] **DCO Research**: [CORREGIDO] Implementado Intel 8253 2MHz emulation (8MHz/4). Eliminada cuantización de 31kHz. Dividores de 16-bit reales para total fidelidad en barridos de pitch.
- [x] **VCF Audit**: [CORREGIDO] Eliminada redundancia en HPF Pos 0. Auditado Passband loss del IR3109 (Verificado: resComp=0.2 por defecto).
- [x] **VCA & Envelopes**: [CORREGIDO] Rangos de tiempo alineados con hardware (3s/12s/12s). Emulación MCU Rate (3ms) y DAC steps integrada.
- [x] **HPF Verification**: [CORREGIDO] Posición 0 (Bass Boost) exclusiva vía shelving filter (+3dB @ 70Hz). Posiciones 1-3 alineadas con curvas de servicio.
- [x] **LFO & Modulation**: [CORREGIDO] Rangos 0.1Hz - 30Hz validados. Curva de onset RC auténtica implementada.
- [x] **Chorus BBD**: [CORREGIDO] Modo 'Both' a 7.7Hz mono-line (BBD clock acelerado). Añadida 'chorusBothRate' a zona de calibración.
- [x] **SysEx Protocol**: [CORREGIDO] Bit-level compatibility with original Roland 106 patch dumps (SW1/SW2 alignment).
- [x] **ADSR & VCA**: [CORREGIDO] Lógica GATE/ENV realineada con panel físico. Curva temporal cuadrática ajustable integrada en calibración.
- [x] **Tape Interface**: [CORREGIDO] FSK Frequencies (1300/2100Hz) and raw 18-byte block parsing verified for hardware tape backups.
- [ ] **Sample Comparison Audit**: Comprehensive A/B testing against original MP3 factory samples.

## Sprint 10: Fidelity Alignment & Comparison Fixes [COMPLETED]
**Goal**: Resolve "notable differences" found in sample comparison by hard-aligning engine response.
- [x] **Disable Velocity Default**: Force velocity sensitivity to 0% for all factory ROM patches to match 1984 hardware behavior.
- [x] **DCO Mixing Audit**: Re-verify relative levels of Saw, Pulse, and Sub-osc against service manual op-amp circuit gain.
- [x] **VCF Curve Scaling**: Implement precise hardware-accurate exponential frequency mapping (0-127 to Hz).
- [x] **Filter Passband Loss**: Fine-tune IR3109 resonance-induced volume drop to ensure authentic "thinning" of the sound.

## Sprint 11: SysEx Audit & Final Alignment [IN PROGRESS]
**Goal**: Ensure 100% bidirectional compatibility and project finalization.
- [x] **SysEx Bit-Accuracy**: Fixed inversion logic for Chorus and VCA Mode in `JunoSysEx.h`.
- [x] **Bidirectional Sync**: Ensure UI parameter changes match full patch dumps.
- [ ] **Final Verification**: Confirm all MP3 samples match current engine output (Build #64).

## Post-Launch / Future Iterations
- [ ] **Responsive Engine**: Re-evaluate dynamic layouts for Desktop, Tablet, and Mobile.
- [ ] **Accessibility (Web & Native)**: Keyboard navigation, ARIA labels, and high-contrast themes.
- [ ] **Expansion**: Additional chorus modes or vintage "aging" profiles.
