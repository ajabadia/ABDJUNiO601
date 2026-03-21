# OMEGA Synth - Project Governance & Coding Rules

To maintain high-fidelity sound, clean architecture, and cross-platform compatibility, the following rules MUST be followed without exception.

## 1. Architectural Integrity (The ABD Layer)
- **Modularity**: All core DSP (Filters, Oscillators, Envelopes) must be placed in the `ABD-SynthEngine` namespace.
- **Independence**: Core DSP classes must NOT depend on `juce::AudioProcessor` or `juce::AudioProcessorValueTreeState`. They should take raw parameters (float/int) via `prepare()` and `process()`.
- **Inheritance**: All synth voices must inherit from `ABD::VoiceBase`. All voice managers must use `ABD::VoiceAllocator`.

## 2. DSP Fidelity & Performance
- **No Hardcoded Constants**: Do NOT hardcode calibration values (e.g., filter cutoff curves, BBD delay times) in the sound loop. Use the `CalibrationManager` or `SynthConstants` with appropriate tooltips.
- **Sample Accuracy**: Prefer per-sample modulation for critical paths (VCF cutoff, VCA gain) to avoid "zipper" noise.
- **Denormal Protection**: Always use `juce::dsp::util::snapToZero` or equivalent after complex feedback loops to prevent CPU spikes from denormal numbers.
- **Integer Math**: Use integer or fixed-point arithmetic for DCO phase accumulation to ensure bit-accurate stability.

## 3. State & Memory Management
- **Smart Pointers**: NEVER use raw pointers for object ownership. Use `std::unique_ptr` or `juce::ScopedPointer`.
- **ValueTree First**: All plugin state (Parameters, MIDI Learn, Preferences) must be serialized into the `juce::ValueTree`.
- **Versioning**: Every state modification that breaks backward compatibility MUST increment the `stateVersion` tag for migration.

## 4. WebUI & Communication
- **JSON Protocol**: All communication between C++ and JavaScript must use a unified JSON schema defined in `WebUIProtocol.h`.
- **Parameter Mapping**: Use the `JUCE_ID` for all parameter synchronization. Do not rely on magic indices.
- **Throttle Events**: Visual updates (LCD, Scopes) should be throttled to 30-60fps to save CPU for the DSP thread.

## 5. Testing & Documentation
- **TDD for DSP**: Every new sound-shaping component (e.g., `JunoVCF`) MUST have a corresponding unit test in the `Tests/` directory verifying its transfer function.
- **Code Comments**: Document the "Rationale" instead of just the "What". Cite hardware manuals (e.g., Roland Service Notes) where applicable.
- **Clean Git**: Each commit should be a logical unit of work. Do not commit build artifacts or temp files.

## 6. Development Workflow
- **Rules Documentation**: Any update to the core synth architecture must be reflected in this file.
- **Roadmap Sync**: Major feature completions should be checked off in `roadmap.md`.
