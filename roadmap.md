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

## Sprint 11: SysEx Audit & Final Alignment [COMPLETED]
**Goal**: Ensure 100% bidirectional compatibility and project finalization.
- [x] **SysEx Bit-Accuracy**: [CORREGIDO] Fixed inversion logic for Chorus and VCA Mode. Aligned SW1/SW2 with verified hardware specs (Bits 0-2 for Range, Bit 5 Active-Low Chorus).
- [x] **Bidirectional Sync**: Ensure UI parameter changes match full patch dumps.
- [x] **VCA Mode Logic**: [CORREGIDO] Resolved inconsistency between envelope triggering and parameter updates (0=ENV, 1=GATE consistently).
- [x] **Final Verification**: [CERTIFIED] All 128 Factory Presets pass the Fidelity Self-Test roundtrip.

## Sprint 12: Bank Expansion & Universal Import [COMPLETED]
**Goal**: Expand storage capacity and external compatibility.
- [x] **7x3 Display Transformation**: Expand 7x2 display to 7x3 including Bank (A-Z) + Patch Number (11-88, 64 patches/bank). [Ref: Factory A/B, User C-Z]
- [x] **Universal Preset Loader**: Implement parser for `.pjunoxl` (TAL-U-NO-LX) files to import external patches into JUNiO 601. [Ref: JUNO106\tau uno lx\]
- [x] **Batch Import Validation**: Verify TAL-U-NO-LX factory presets (106-factory-a) match internal 106 engine response.

## Sprint 13: JUNO-6/60 Integration & UI Polish [COMPLETED]
**Goal**: Integrate Juno-6 and Juno-60 engine modules, dynamic themes, and UI enhancements.
- [x] **Juno-6/60 Custom Themes**: Implemented four distinct themes (Classic Blue, Juno-60 Classic, Juno-6 Analog, Juno-106 Classic) utilizing the high-fidelity side panel PNG image assets (wood/metal).
- [x] **Calibration Auto-Sync**: Slipped instant theme change triggers when Default Calibration Profile changes.
- [x] **Taped Component Labels**: Placed dynamic model-routing tapes higher to avoid key overlaps, colored by model (beige J106, ocre J6, off-white J60).
- [x] **Vintage Arpeggiator Knobs**: Option C discrete rotary knobs for Mode and Range with active illuminated labels around them.
- [x] **Portamento Centering**: Re-aligned TIME knob and ON/OFF switch with balanced layout.

## Sprint 14: Theme Constraints, Component Isolation & Side Panel Layout Fixes [COMPLETED]
**Goal**: Enforce skin locking logic by profile, solve module model theme interference, restore wood/metal sides positioning, and resolve CMake build packaging issues.
- [x] **Profile-Based Skin Constraints**: Locked skin selection options dynamically depending on the active profile:
  - **Juno-6**: Only "JUNO-6 ANALOG" skin is selectable and selector is disabled.
  - **Juno-60**: Only "JUNO-60 CLASSIC" skin is selectable and selector is disabled.
  - **Juno-106**: Restricted to "JUNO-106 CLASSIC" and "JUNO-106S DARK" skins.
  - **Super Six**: Unrestricted (all 9 skins available), defaults to "CLASSIC BLUE".
- [x] **Component Isolation**: Scoped `.module[data-model]` style overrides to apply only under the `classic` theme. This prevents custom themes (like `dark-106s` and `space-echo`) from having their dark/styled module background colors overridden by individual module models.
- [x] **Side Cheek Layout Restoration**: Restored the missing absolute positioning, sizing, and rendering rules for `#synth-app::before` and `#synth-app::after` in [base.css](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/UI/WebUI/css/base.css) and the root synced CSS, displaying real wood/metal panels correctly.
- [x] **Painter Tape Target Filtering**: Wrapped dynamic calibration categories inside `.service-section` containers, enabling the script to correctly filter and display only the relevant section (and focusing the model selector) when clicking on a painter tape. Handle special case of `env` matching the `ADSR` category.
- [x] **CMake Packaging Resolution**: Cleaned up non-existent asset references (`wood_106_left.png` / `wood_106_right.png`) in [CMakeLists.txt](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/CMakeLists.txt) to fix the `juceaide` crash (MSB8066).

## Post-Launch / Future Iterations

