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
*   **JSON Banks (`.json` / `.jno`)**: Structured bank format with library name, category, author, tags, notes, and full patch parameter data. Imported via the Smart Import dialog with metadata validation.

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

## Tape Cassette Interface: FSK Decoding Pipeline

### Overview
The ABD JUNiO Super SIX includes a complete **FSK (Frequency Shift Keying)** tape decoding pipeline that reads authentic 1980s cassette backup data from the Roland Juno-60 and Juno-106 synthesizers. This pipeline converts raw WAV recordings of cassette dumps into playable presets within the synth engine.

### Juno-60 vs Juno-106 Cassette Formats

| Parameter | Juno-106 | Juno-60 |
|:---|---:|---:|
| **Baud Rate** | **1200 baud** (1200 bits/s) | **340 baud** (340 bits/s) |
| **Space frequency (bit 0)** | ~1300 Hz | 1360 Hz |
| **Mark frequency (bit 1)** | ~2100 Hz | 2380 Hz |
| **Leader (pilot) tone** | 1s of continuous marks (~2100 Hz) | Marks for clock sync (~2380 Hz) |
| **Samples per bit (at 44100 Hz)** | ~37 samples | ~130 samples |
| **Patch size** | 18 bytes data + 1 byte checksum | 18 bytes data + 1 byte checksum |
| **Switch byte mapping** | Chorus I/II bits | Sub Osc, PWM, dedicated HPF bits |

### Decoding Pipeline Architecture

The full decoding pipeline consists of 6 stages, implemented in `Source/Core/JunoTapeDecoder.h`:

```
WAV File → Load & Mix → Pre-process → Upsample → Leader Detection → FSK Decode → Validate → Presets
```

#### Stage 1: Audio Loading (`decodeWavFile`)
- Reads WAV via JUCE's `AudioFormatManager` (supports WAV, AIFF, FLAC)
- Handles 8/16/24/32-bit sample formats
- Supports stereo and mono files

#### Stage 2: Pre-processing
- **Mono mix**: Sums stereo to mono with 0.5 gain
- **DC removal**: 1-pole IIR high-pass filter (`alpha = 0.9943`, corner ~40 Hz)
- **Normalization**: Scales to unity peak amplitude

#### Stage 3: Upsampling to 44100 Hz
```cpp
// Source/Core/JunoTapeDecoder.h ~ line 407
if (reader->sampleRate < 43900.0 && reader->sampleRate > 0.0) {
    constexpr double kTargetSr = 44100.0;
    int upsampledLen = (int)((double)numSamples * kTargetSr / reader->sampleRate + 0.5);
    
    juce::LagrangeInterpolator interpolator;
    double ratio = reader->sampleRate / kTargetSr;
    interpolator.process(ratio, samples, upsampledData, upsampledLen);
}
```
- **Why**: Many tape recordings are stored at 22050 Hz (downsampled from 44100 Hz originals). At 44100 Hz, the Goertzel filter bandwidth is tighter (better frequency discrimination) and there are more samples per bit (~74 vs ~37 at 1200 baud).
- **Method**: Lagrange interpolation (order 5) via `juce::LagrangeInterpolator` — excellent quality for FSK signals without the ringing artifacts of FFT-based resampling.
- **Guard**: Only upsamples when `sr < 43900 Hz`. Files already at 44100 Hz or higher pass through unchanged.

#### Stage 4: Format Detection (`detectFormatFromLeaderTone`)
- Analyzes the first ~0.5 seconds of signal after onset
- Uses dual Goertzel filters at 2100 Hz (Juno-106 mark) and 2380 Hz (Juno-60 mark)
- Compares normalized energy: requires 2x dominance for a confident decision
- Falls back to probing both baud rates if detection is ambiguous

#### Stage 5: FSK Bit Decoding (`decodeFSK`)

**Goertzel Algorithm:**
A single-frequency DFT that acts as a narrow bandpass filter. Much more robust than zero-crossing counting for noisy recordings and high baud rates.

```cpp
struct GoertzelState {
    double s1, s2, coeff;  // Two-state resonant filter
};

double omega = 2.0 * pi * targetFreq / sampleRate;
coeff = 2.0 * cos(omega);
// Process: s0 = x + coeff * s1 - s2;  s2 = s1;  s1 = s0;
// Power = s1^2 + s2^2 - coeff * s1 * s2
```

**Sub-window majority voting:**
Each bit is divided into sub-windows (~65 samples minimum). Each sub-window votes on whether it contains more mark energy or space energy. The final bit decision is the majority vote. This provides noise immunity.

**Brute-force search:**
The decoder searches **29 speed factors** (0.86–1.14, step 0.01) × **5 phase offsets** (0.0–0.8, step 0.2) = **145 combinations** for each baud rate. This compensates for cassette wow/flutter and timing drift.

```
for (speedFactor in 0.86..1.14):
    for (phaseOffset in 0.0..0.8):
        bits = generateBitStream(samples, speedFactor, phaseOffset)
        for (offset in 0..3):
            bytes = extractBytes(bits, offset)
            validated = validateChecksums(bytes)
            keepBest(validated)
```

#### Stage 6: Checksum Validation (`validatePatches`)
- Each 18-byte patch is followed by a checksum byte: `checksum = (sum(byte[0..17]) & 0x7F)`
- Only patches with matching checksums are kept
- This is the primary false-positive filter

### Frame Format

Each byte is transmitted as an asynchronous serial frame over the FSK link:

```
  Start    Data (LSB first)         Stop
   Bit     b0 b1 b2 b3 b4 b5 b6 b7  Bit
  ┌────┐ ┌────────────────────────┐ ┌────┐
  │ 0  │ │ 1  2  4  8 16 32 64 128│ │ 1  │
  └────┘ └────────────────────────┘ └────┘
  ← 1 bit → ←      8 bits       → ← 1 bit →
  ←─────────── 10 bits total ────────────→
```

At 1200 baud: ~37 samples/bit, ~8.3ms per byte.
At 340 baud:  ~130 samples/bit, ~29.4ms per byte.

### Auto-Detect vs Forced Baud Rate

The decoder supports two modes:
- **`forcedBaudRate = 0`** (auto-detect): Runs leader tone analysis, then tries both baud rates (hint first, then alternative). Picks the one with more validated patches.
- **`forcedBaudRate = 340 | 1200`**: Skips leader detection and decodes only at the specified baud rate.

### Encoder (`JunoTapeEncoder`)

The tape encoder in `Source/Core/JunoTapeEncoder.h` generates authentic FSK audio from patch data:

```
Patches → Bit Stream → Render to Audio
```

1. **Leader tone**: 2 seconds of continuous marks (for tape sync)
2. **For each patch**: Frame (start bit + 8 data bits + stop bit) × 18 bytes + checksum
3. **Inter-block gap**: 0.1 seconds of marks between patches
4. **Trailer**: 0.5 seconds of marks
5. **Synthesis**: Pure sine wave at space (1360/1300 Hz) or mark (2380/2100 Hz) frequency

Supports both `Juno106` (1200 baud) and `Juno60` (340 baud) formats.

### Format Converter (`JunoFormatConverter`)

After decoding, the 18-byte raw patch data passes through `JunoFormatConverter` which maps hardware bytes to the internal parameter model:

```
Bytes → JunoFormatConverter → Preset (ValueTree)
```

**Juno-60 byte mapping differs from Juno-106** in switch bytes (16-17):
- **SW1**: Range, Saw, Pulse, Sub Osc, PWM (vs Chorus I/II in Juno-106)
- **SW2**: VCA Mode, VCF Polarity, HPF (shared structure, different bit positions)

The converter auto-selects the correct mapping based on the detected baud rate.

### Python Analysis Tools

Two Python scripts in `scripts/` provide offline analysis capabilities:

> **Note:** The Python scripts use a simplified FSK decoder (`decode_fsk(fast=True)`) with a single speed/phase pass. The C++ decoder performs a brute-force search over 145 speed×phase combinations, which finds more patches on noisy recordings but produces different hex values.

| Script | Purpose |
|:---|---|
| `visualize_tape.py` | FSK signal visualization (waveform, spectrogram, Goertzel energy, bit stream, patch regions). Supports zoom and per-patch field analysis. |
| `compare_patches.py` | Decodes all 8 reference tapes and matches decoded patches against 267 known factory presets. Shows hex dumps with field-level detail. |
| `validate_dcb.py` | Validates Juno-60 DCB format decoding against known factory patches. |

These scripts replicate the C++ decoding pipeline in Python (using `np.interp` for upsampling and simplified Goertzel FSK), making them useful for rapid prototyping and forensic tape analysis.

### Key Implementation Files

| File | Purpose |
|:---|---|
| `Source/Core/JunoTapeDecoder.h` | Complete decoding pipeline (inline header-only) |
| `Source/Core/JunoTapeEncoder.h` | Tape audio encoder (inline header-only) |
| `Source/Core/Importers/JunoTapeImporter.h/.cpp` | WAV file import adapter → PresetManager libraries |
| `Source/Core/Importers/JunoFormatConverter.h/.cpp` | 18-byte → Internal parameter mapping (Juno-60 & Juno-106) |
| `Source/Core/JunoTapeEncoder.h` | Patch → FSK audio synthesis (encodePatches) |
| `scripts/compare_patches.py` | Python factory preset matcher |
| `scripts/compare_upsamplers.py` | Python C++ vs Python pipeline hex comparator |
| `scripts/dump_tape_hex.py` | Python hex dump for all reference tapes |

### Reference Tape Recordings

The project includes 8 reference WAV recordings for testing and development:

| Tape | Source | SR | Duration | Format |
|:---|---|---|---:|---:|
| `docs/Juno-60 (1)/JUNO-60 Bank A.wav` | Juno-60 factory bank A | 22050 Hz | 48.0s | 1200 baud |
| `docs/Juno-60 (1)/JUNO-60 Bank B.wav` | Juno-60 factory bank B | 22050 Hz | 47.7s | 1200 baud |
| `docs/JUNO-106/JUNO106 Bank A.wav` | Juno-106 factory bank A | 22050 Hz | 4.0s | 1200 baud |
| `docs/JUNO-106/JUNO106 Bank B.wav` | Juno-106 factory bank B | 22050 Hz | 3.9s | 1200 baud |
| `docs/JUNO-106/Roland Juno-60 factory programs group 1.wav` | Juno-60 factory G1 | 22050 Hz | 48.0s | 1200 baud |
| `docs/JUNO-106/Roland Juno-60 factory programs group 2.wav` | Juno-60 factory G2 | 22050 Hz | 47.7s | 1200 baud |
| `JUNO106/original/tapes/roland_juno106_factory/j106ma.wav` | Juno-106 factory A | 22050 Hz | 4.0s | 1200 baud |
| `JUNO106/original/tapes/roland_juno106_factory/j106mb.wav` | Juno-106 factory B | 22050 Hz | 3.9s | 1200 baud |

All recordings are 22050 Hz (originally 44100 Hz, downsampled for storage). The Lagrange upsampler restores them to 44100 Hz before decoding.

---

## Referencias Rápidas — Smart Import & Testing

### Badges del Smart Import (Universal Dialog)

| Formato | Badge | Color | RGB (getComputedStyle) |
|---------|:-----:|:-----:|:----------------------:|
| **TAPE** | `TAPE` | Ámbar `#f90` | `rgb(255, 153, 0)` |
| **SYSEX** | `SYSEX` | Azul `#0af` | `rgb(0, 170, 255)` |
| **CSV** | `CSV` | Verde `#0f0` | `rgb(0, 255, 0)` |
| **JSON** | `JSON` | Rosa `#f0a` | `rgb(255, 0, 170)` |

### Cobertura de Tests CDP (Chrome DevTools Protocol)

| Formato | Checks | Secciones Verificadas | Script |
|:-------:|:------:|:---------------------|:-------|
| **TAPE** | 12 | Modal, badge ámbar, Tape section, SNR, Jitter, Dropouts, preset names, import button | `test_smart_import_all_formats.py` |
| **SYSEX** | 15 | Modal, badge azul, Sysex section, Device ID, Function, Checksum, Hex preview, preset names | `test_smart_import_all_formats.py` + `test_smart_import_direct.py` |
| **CSV** | 14 | Modal, badge verde, CSV section, column count, parameters, column names, preset names | `test_smart_import_all_formats.py` |
| **JSON** | 14 | Modal, badge rosa, CSV section (reusada), libraryName, category, column-list, preset names | `test_smart_import_all_formats.py` |
| **REAL** 🆕 | **16** | Modal JSON, badge, metadata (libraryName, category), preset names (4), import click, modal close, notification, fallback skip | `test_import_real.py` + `test_import_bank.json` |
| **Total** | **71** | | |

### Eventos JS Bridge (Smart Import)

| Evento | Dirección | Propósito | Archivos |
|--------|:---------:|:----------|:---------|
| `onSmartImportProgress` | C++ → JS | Progreso de decodificación (Tape) | `WebViewEditor.cpp` (10x dispatch), `script.js` (2x listener) |
| `onSmartImportResult` | C++ → JS | Resultado de importación (todos los formatos) | `WebViewEditor.cpp` (2x dispatch), `script.js` (1x listener) |

### Pipeline CI/CD (build_and_test.bat)

```batch
build_and_test.bat                → Build + Launch + 2 Tests + Cleanup
build_and_test.bat --test-only    → Skip build, just launch + test
build_and_test.bat --help         → Show usage
```

**Tests ejecutados en pipeline:**
1. `test_smart_import_all_formats.py` — 55 checks (Tape 12 + SysEx 15 + CSV 14 + JSON 14)
2. `test_import_real.py` — 16 checks (real import via JS bridge, fallback mode)

**Total:** **71 checks** en pipeline CI/CD

**Requisito:** Variable de entorno `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--remote-debugging-port=9222 --remote-allow-origins=*` para habilitar CDP en el WebView2.

---

## PresetBrowser Layout Constants

The native PresetBrowser (`Source/UI/PresetBrowser.h`) defines all layout dimensions as named `static constexpr` constants. All constants are protected by `static_assert` in `Source/Core/Tests/PresetBrowserTests.cpp` to enforce compile-time verification.

### Column Width Constants

| Constant | Value | Description |
|:---|---:|:---|
| `kColFavW` | 24 | Favorite star column width (fixed) |
| `kColCatWMin` | 80 | Category column minimum width (scales with `contentW / kColProportionDivisor`) |
| `kColLibWMin` | 70 | Library column minimum width (scales with `contentW / kColProportionDivisor`) |
| `kColGapAdj` | 8 | Gap adjustment: `nameW = contentW - favW - catW - libW - kColGapAdj` |
| `kColLeftMarg` | 4 | Left margin before the first column |
| `kColGap` | 2 | Gap (in pixels) between adjacent columns |
| `kColProportionDivisor` | 5 | Divisor for proportional width: `catW = libW = jmax(min, contentW / 5)` |

### Layout Constants

| Constant | Value | Description |
|:---|---:|:---|
| `kOuterMargin` | 5 | Outer margin around the entire PresetBrowser component |
| `kRowH` | 30 | Height of filter combo boxes, search field, and action button rows |
| `kInnerPad` | 2 | Inner padding applied as `.reduced()` to child widgets within rows |
| `kSectionGap` | 5 | Vertical gap between layout sections |
| `kHeaderBarH` | 22 | Height of the column header row (`PresetBrowserHeaderBar`) |
| `kHeaderListGap` | 2 | Vertical gap between header bar and the preset list |
| `kListRowH` | 24 | Height of each preset row in the `ListBox` |

### Font Scale Constants (relative to row height)

| Constant | Value | Used For |
|:---|---:|:---|
| `kNameFontScale` | 0.65 | Preset name text |
| `kStarFontScale` | 0.65 | Favorite star symbol (`★`) |
| `kDetailFontScale` | 0.55 | Category text |
| `kSmallFontScale` | 0.50 | Library name text |

### Visual / Alpha Constants

| Constant | Value | Used For | Source |
|:---|---:|:---|:---|
| `kSelectedBgAlpha` | 0.25 | Selection highlight background (`royalblue`) | `PresetBrowser.h` |
| `kNameTextAlpha` | 0.85 | Preset name text opacity (`white`) | `PresetBrowser.h` |
| `kDetailTextAlpha` | 0.50 | Category text opacity (`lightgrey`) | `PresetBrowser.h` |
| `kLibTextAlpha` | 0.50 | Library name text opacity (`orange`) | `PresetBrowser.h` |
| `kBgAlpha` | 0.20 | Default component background (`black`) | `PresetBrowser.h` |
| `kFieldWidthRatio` | 0.40 | Proportional width for search/library fields | `PresetBrowser.h` |
| `kHeaderFontSize` | 10.0 | Column header text font size | `PresetBrowserHeaderBar.h` |
| `kArrowSize` | 6.0 | Sort arrow triangle size | `PresetBrowserHeaderBar.h` |
| `kArrowOffsetX` | 4.0 | Sort arrow horizontal offset from rect edge | `PresetBrowserHeaderBar.h` |
| `kActiveBgAlpha` | 0.30 | Active column background when hovered | `PresetBrowserHeaderBar.h` |
| `kActiveBgDefAlpha` | 0.15 | Active column background by default | `PresetBrowserHeaderBar.h` |
| `kHoverBgAlpha` | 0.10 | Inactive column background when hovered | `PresetBrowserHeaderBar.h` |
| `kDefaultBgAlpha` | 0.20 | Column background by default | `PresetBrowserHeaderBar.h` |
| `kTextAlphaDim` | 0.60 | Non-hovered header text opacity | `PresetBrowserHeaderBar.h` |
| `kAnimThreshold` | 0.001 | Animation convergence threshold | `PresetBrowserHeaderBar.h` |
| `kAnimSpeed` | 0.30 | Arrow animation interpolation (30% per frame) | `PresetBrowserHeaderBar.h` |

---
*Developed by **ABD-IA** — Advanced Agentic Coding Project*
**Version: 2.0.0 (Unified Hybrid Era)**
