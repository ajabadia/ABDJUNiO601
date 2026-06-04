#!/usr/bin/env python3
"""
FSK Cassette Tape Visualizer for ABDJUNiO601
=============================================
Analyzes WAV recordings of Juno-60 / Juno-106 cassette tape dumps.
Visualizes the FSK signal, Goertzel energy, decoded bits, and valid patches.

Usage:
    python scripts/visualize_tape.py <wav_file> [options]

Options:
    --baud 340|1200       Force baud rate (auto-detect by default)
    --start <seconds>     Start time for zoom window
    --end <seconds>       End time for zoom window
    --save <path>         Save plot to file instead of showing
    --goertzel-only       Only show Goertzel energy plot (faster for long files)
"""

import sys
import os
import argparse
import math

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from scipy import signal as scipy_signal

# ─── FSK Constants ───────────────────────────────────────────────────────────

JUNO_FORMATS = {
    340: {  # Juno-60
        "name":       "Juno-60",
        "baud":       340,
        "freq_space": 1360.0,
        "freq_mark":  2380.0,
        "leader_hz":  2380.0,
    },
    1200: {  # Juno-106
        "name":       "Juno-106",
        "baud":       1200,
        "freq_space": 1300.0,
        "freq_mark":  2100.0,
        "leader_hz":  2100.0,
    },
}

# ─── WAV Loader ──────────────────────────────────────────────────────────────

def load_wav(path):
    """Load a WAV file and return (samples, sample_rate, num_channels)."""
    import wave as wav_mod

    with wav_mod.open(path, "rb") as wf:
        sr = wf.getframerate()
        nch = wf.getnchannels()
        nframes = wf.getnframes()
        sampwidth = wf.getsampwidth()
        raw = wf.readframes(nframes)

    # Convert raw bytes to float samples
    if sampwidth == 1:      # 8-bit unsigned
        dtype = np.uint8
        scale = 128.0
    elif sampwidth == 2:    # 16-bit signed
        dtype = np.int16
        scale = 32768.0
    elif sampwidth == 3:    # 24-bit signed (packed)
        # Unpack 3-byte samples
        samples = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        vals = np.zeros(len(samples), dtype=np.int32)
        vals = (samples[:, 0].astype(np.int32) |
                (samples[:, 1].astype(np.int32) << 8) |
                (samples[:, 2].astype(np.int32) << 16))
        # Sign-extend 24-bit to 32-bit
        vals[vals >= 0x800000] -= 0x1000000
        samples = vals.astype(np.float32) / 8388608.0
        if nch > 1:
            samples = samples.reshape(-1, nch)
        return samples, sr, nch
    elif sampwidth == 4:    # 32-bit float or int
        dtype = np.int32
        scale = 2147483648.0

    samples = np.frombuffer(raw, dtype=dtype).astype(np.float32) / scale

    if nch > 1:
        samples = samples.reshape(-1, nch)

    return samples, sr, nch


def upsample_to_44100(samples, sr):
    """
    Upsample audio to 44100 Hz using linear interpolation.
    No-op if sr == 44100. Returns (upsampled_samples, new_sr).
    
    Linear interpolation is sufficient for FSK decoding because:
    - Goertzel algorithm is robust to minor interpolation artifacts
    - The FSK frequencies (1300-2380 Hz) are well below the 22050 Hz Nyquist
    - 2x upsampling (22050 -> 44100) with linear interpolation preserves
      zero-crossing timing better than nearest-neighbor
    """
    if sr >= 44100 or sr <= 0:
        # Only upsample lower rates — never downsample higher rates (would lose info)
        return samples, sr
    
    # Calculate output length: new_sr / sr * len(samples)
    target_sr = 44100
    num_out = int(round(len(samples) * target_sr / sr))
    
    # Linear interpolation via numpy
    x_old = np.arange(len(samples))
    x_new = np.linspace(0, len(samples) - 1, num_out)
    upsampled = np.interp(x_new, x_old, samples)
    
    return upsampled, target_sr


def preprocess(samples, sr, nch):
    """
    Full pre-processing pipeline:
    1. Mono mix (if stereo)
    2. Upsample to target_sr (if sr != target_sr)
    3. 1-pole HPF for DC removal
    4. Normalize to unity peak
    
    Returns (processed_samples, new_sr).
    """
    if nch > 1:
        samples = samples.mean(axis=1)

    # Upsample to target sample rate for consistent decoding
    samples, sr = upsample_to_44100(samples, sr)

    # 1-pole HPF (alpha = 0.9943 for DC removal)
    alpha = 0.9943
    y = np.zeros_like(samples)
    y_prev = 0.0
    x_prev = 0.0
    for i in range(len(samples)):
        x = samples[i]
        y_i = alpha * (y_prev + x - x_prev)
        y[i] = y_i
        y_prev = y_i
        x_prev = x
    samples = y

    # Normalize
    max_peak = np.max(np.abs(samples))
    if max_peak > 0.0001:
        samples = samples / max_peak

    return samples, sr


# ─── Goertzel Algorithm ──────────────────────────────────────────────────────

def goertzel_init(target_freq, sr):
    """Initialize Goertzel state for a target frequency."""
    omega = 2.0 * math.pi * target_freq / sr
    return {"s1": 0.0, "s2": 0.0, "coeff": 2.0 * math.cos(omega)}


def goertzel_process(st, x):
    """Process one sample through the Goertzel filter."""
    s0 = x + st["coeff"] * st["s1"] - st["s2"]
    st["s2"] = st["s1"]
    st["s1"] = s0


def goertzel_power(st):
    """Get the power (magnitude squared) from the Goertzel state."""
    return st["s1"] * st["s1"] + st["s2"] * st["s2"] - st["coeff"] * st["s1"] * st["s2"]


def goertzel_detect_bit(samples, start, length, gs_init, gm_init):
    """Detect a bit using dual Goertzel. Returns 0 (space) or 1 (mark)."""
    if length < 4:
        return 0
    gs = gs_init.copy()
    gm = gm_init.copy()
    for i in range(start, start + length):
        x = float(samples[i])
        goertzel_process(gs, x)
        goertzel_process(gm, x)
    ps = goertzel_power(gs)
    pm = goertzel_power(gm)
    return 1 if pm > ps else 0


# ─── FSK Decoder (Python replica of C++ logic) ────────────────────────────

def validate_patches(decoded_bytes):
    """Find valid Juno patches with correct checksums."""
    validated = []
    i = 0
    while i + 19 <= len(decoded_bytes):
        checksum = sum(decoded_bytes[i:i+18]) & 0x7F
        if checksum == decoded_bytes[i + 18]:
            validated.extend(decoded_bytes[i:i+18])
            i += 18
        i += 1
    return validated


def decode_fsk(samples, sr, bits_per_second, fast=True):
    """
    FSK decoder using Goertzel-based bit detection.
    
    Args:
        samples: preprocessed audio samples
        sr: sample rate
        bits_per_second: baud rate (340 or 1200)
        fast: if True, single pass (speed=1.0, phase=0). 
              if False, brute-force over speed/phase (slow in Python!)
    
    Returns dict with bits, bytes, patches, and per-bit Goertzel energies.
    Note: Simplified version of C++ decodeFSK (no sub-window majority voting).
    """
    fmt = JUNO_FORMATS[bits_per_second]
    freq_space = fmt["freq_space"]
    freq_mark  = fmt["freq_mark"]
    nominal_spb = sr / bits_per_second

    gs_template = goertzel_init(freq_space, sr)
    gm_template = goertzel_init(freq_mark, sr)

    if fast:
        speed_factors = [1.0]
        phase_offsets = [0.0]
    else:
        speed_factors = np.arange(0.86, 1.15, 0.01)
        phase_offsets = np.arange(0.0, 1.0, 0.2)

    best_bits = []
    best_bytes = []
    best_validated = []
    max_patches = 0
    best_speed = 0.0
    best_phase = 0.0
    best_energies = []

    for sf in speed_factors:
        spb = nominal_spb * sf
        for po in phase_offsets:
            bits = []
            energies = []
            sample_pos = po * spb
            n = len(samples)

            while int(sample_pos + spb + 1.0) < n:
                bit_start = int(sample_pos)
                bit_end = min(n - 1, int(sample_pos + spb + 1.0))
                bit_len = bit_end - bit_start
                if bit_len < 4:
                    sample_pos += spb
                    continue

                # Single Goertzel per bit (simplified vs C++ sub-window voting)
                bit = goertzel_detect_bit(samples, bit_start, bit_len, gs_template, gm_template)

                # Compute raw energies for visualization
                gs = gs_template.copy()
                gm = gm_template.copy()
                for i in range(bit_start, bit_end):
                    x = float(samples[i])
                    goertzel_process(gs, x)
                    goertzel_process(gm, x)
                energies.append((goertzel_power(gs), goertzel_power(gm)))

                bits.append(bit)
                sample_pos += spb

            # Extract bytes from bit stream (try 4 offsets)
            for offset in range(4):
                bytes_out = []
                i = offset
                while i + 10 < len(bits):
                    if bits[i] == 1 and bits[i+1] == 0:
                        byte = 0
                        valid = True
                        for b in range(8):
                            idx = i + 1 + b
                            if idx < len(bits):
                                if bits[idx] == 1:
                                    byte |= (1 << b)
                            else:
                                valid = False
                                break
                        stop_idx = i + 1 + 8
                        if valid and stop_idx < len(bits) and bits[stop_idx] == 1:
                            bytes_out.append(byte & 0x7F)
                            i = stop_idx
                    i += 1

                validated = validate_patches(bytes_out)
                num_patches = len(validated) // 18
                if num_patches > max_patches:
                    max_patches = num_patches
                    best_bits = list(bits)
                    best_bytes = list(bytes_out)
                    best_validated = list(validated)
                    best_speed = sf
                    best_phase = po
                    best_energies = list(energies)

            if max_patches >= 56:
                break
        if max_patches >= 56:
            break

    return {
        "bits": best_bits,
        "bytes": best_bytes,
        "validated": best_validated,
        "num_patches": max_patches,
        "speed_factor": best_speed,
        "phase_offset": best_phase,
        "energies": best_energies,
    }


# ─── Leader Tone Detection ──────────────────────────────────────────────────

def detect_format(samples, sr):
    """Detect tape format from leader tone frequency. Returns baud rate or 0."""
    leader_len = min(len(samples), int(sr * 3.0))
    if leader_len < int(sr * 0.5):
        return 0

    # Find onset
    global_peak = float(np.max(np.abs(samples[:leader_len])))
    if global_peak < 0.001:
        return 0

    onset_threshold = global_peak * 0.05
    leader_start = 0
    step = int(sr * 0.1)
    while leader_start < leader_len:
        chunk_end = min(leader_len, leader_start + step)
        local_peak = float(np.max(np.abs(samples[leader_start:chunk_end])))
        if local_peak > onset_threshold:
            break
        leader_start += step

    if leader_start >= leader_len:
        return 0

    # Count zero crossings in leader region
    leader_seg = samples[leader_start:leader_len]
    peak_in_leader = float(np.max(np.abs(leader_seg)))
    noise_gate = peak_in_leader * 0.1
    zc = 0
    last_above = leader_seg[0] > noise_gate
    for s in leader_seg[1:]:
        above = s > noise_gate
        below = s < -noise_gate
        if above and not last_above:
            zc += 1
            last_above = True
        elif below and last_above:
            zc += 1
            last_above = False

    if zc < 10:
        return 0

    duration = len(leader_seg) / sr
    est_freq = zc / (2.0 * duration)
    return 340 if est_freq > 2240.0 else 1200


# ─── Visualization ──────────────────────────────────────────────────────────

def visualize(wav_path, forced_baud=0, start_time=None, end_time=None,
              save_path=None, goertzel_only=False):
    """Main visualization function."""
    # Load and preprocess
    print(f"Loading: {wav_path}")
    samples_raw, sr, nch = load_wav(wav_path)
    duration = len(samples_raw) / sr
    print(f"  Sample rate: {sr} Hz, Channels: {nch}, Duration: {duration:.1f}s")

    orig_sr = sr
    samples, sr = preprocess(samples_raw, sr, nch)

    # Detect format
    if forced_baud:
        baud = forced_baud
    else:
        baud = detect_format(samples, sr)

    if baud not in JUNO_FORMATS:
        print("  ⚠ Could not determine tape format. Defaulting to Juno-106.")
        baud = 1200

    fmt = JUNO_FORMATS[baud]
    print(f"  Format: {fmt['name']} ({baud} baud, space={fmt['freq_space']:.0f} Hz, "
          f"mark={fmt['freq_mark']:.0f} Hz)")
    if sr != orig_sr:
        print(f"  Upsampled: {orig_sr} Hz → {sr} Hz (linear interpolation)")

    # Crop to requested window
    start_sample = int(start_time * sr) if start_time else 0
    end_sample = int(end_time * sr) if end_time else len(samples)
    if start_time or end_time:
        print(f"  Zoom: {start_time or 0:.1f}s – {end_time or duration:.1f}s")

    seg = samples[start_sample:end_sample]
    t = np.arange(len(seg)) / sr + (start_time or 0)

    # Decode FSK (skip in goertzel-only mode for speed)
    result = None
    if not goertzel_only:
        print("  Decoding FSK (fast pass)...", end=" ", flush=True)
        result = decode_fsk(samples, sr, baud, fast=True)
        num_p = result["num_patches"] if result else 0
        print(f"done. {len(result['bits'])} bits, {len(result['bytes'])} bytes, "
              f"{num_p} patches.")
    else:
        print("  Skipping bit decode (--goertzel-only mode)")

    # Prepare patch regions for annotation (only in full mode)
    patch_regions = []
    if result and result["validated"] and result["bytes"]:
        val_bytes = result["validated"]
        raw_bytes = result["bytes"]
        patch_idx = 0
        search_pos = 0
        while patch_idx * 18 < len(val_bytes) and search_pos < len(raw_bytes):
            patch_data = val_bytes[patch_idx * 18 : (patch_idx + 1) * 18]
            for pos in range(search_pos, len(raw_bytes) - 18):
                if raw_bytes[pos:pos+18] == patch_data:
                    bit_start = (pos * 10) // 1
                    bit_end = ((pos + 18) * 10) // 1
                    if bit_end < len(result["bits"]):
                        patch_regions.append((bit_start, bit_end, f"P{patch_idx+1}"))
                        search_pos = pos + 18
                        patch_idx += 1
                        break

    # ── Plotting ────────────────────────────────────────────────────────────
    if goertzel_only:
        fig, axes = plt.subplots(2, 1, figsize=(16, 6), sharex=True,
                                 gridspec_kw={"height_ratios": [1, 1]},
                                 constrained_layout=True)
        ax_spec = axes[0]
        ax_goe  = axes[1]
    else:
        fig = plt.figure(figsize=(18, 10), constrained_layout=True)
        gs = fig.add_gridspec(4, 1, height_ratios=[1.2, 1.2, 1.5, 1.5],
                              hspace=0.25)
        ax_wave = fig.add_subplot(gs[0])
        ax_spec = fig.add_subplot(gs[1])
        ax_goe  = fig.add_subplot(gs[2])
        ax_bits = fig.add_subplot(gs[3])

    # Normal 1st subplot: Waveform (or Goertzel subplot 0)
    if not goertzel_only:
        ax_wave.plot(t, seg, color="#444444", linewidth=0.4)
        ax_wave.set_ylabel("Amplitude")
        ax_wave.set_title(f"FSK Signal: {fmt['name']} – {os.path.basename(wav_path)}")
        ax_wave.grid(True, alpha=0.3)
        ax_wave.set_xlim(t[0], t[-1])

        # Leader tone annotation
        leader_len = min(len(samples), int(sr * 3.0))
        leader_end_t = leader_len / sr
        ax_wave.axvspan(0, leader_end_t, alpha=0.06, color="blue",
                        label=f"Leader tone ~{fmt['leader_hz']:.0f} Hz")
        ax_wave.legend(fontsize=8, loc="upper right")

    # Spectrogram (subplot 1 or 0)
    f, t_spec, Sxx = scipy_signal.spectrogram(
        seg, sr, nperseg=512, noverlap=384,
        window=("hamming",), scaling="density",
    )

    # Limit frequency range to FSK band
    freq_mask = (f >= 800) & (f <= 3000)
    Sxx_dB = 10 * np.log10(Sxx[freq_mask] + 1e-12)
    vmin = np.percentile(Sxx_dB, 10)
    vmax = np.percentile(Sxx_dB, 95)

    im = ax_spec.pcolormesh(t_spec + (start_time or 0), f[freq_mask],
                            Sxx_dB, shading="gouraud",
                            cmap="inferno", vmin=vmin, vmax=vmax)
    ax_spec.set_ylabel("Frequency (Hz)")
    ax_spec.set_ylim(800, 3000)

    # Mark space/mark frequencies
    ax_spec.axhline(fmt["freq_space"], color="cyan", linestyle="--", linewidth=0.8,
                    label=f"Space {fmt['freq_space']:.0f} Hz")
    ax_spec.axhline(fmt["freq_mark"], color="lime", linestyle="--", linewidth=0.8,
                    label=f"Mark {fmt['freq_mark']:.0f} Hz")
    ax_spec.axhline((fmt["freq_space"] + fmt["freq_mark"]) / 2,
                    color="yellow", linestyle=":", linewidth=0.5, alpha=0.6,
                    label="Decision threshold")
    ax_spec.legend(fontsize=7, loc="upper right")
    ax_spec.grid(True, alpha=0.2)
    ax_spec.set_xlim(t[0], t[-1])

    # Colorbar
    cbar = plt.colorbar(im, ax=ax_spec, fraction=0.05, pad=0.02)
    cbar.set_label("dB", fontsize=8)

    # Goertzel energy (subplot 2 or 1)
    energies = result["energies"] if result else []
    if energies:
        # Compute Goertzel energy over sliding windows for continuous visualization
        window_size = int(sr / baud)  # one bit period
        hop_size = window_size // 4
        num_windows = max(1, (len(seg) - window_size) // hop_size)

        gs_cont = goertzel_init(fmt["freq_space"], sr)
        gm_cont = goertzel_init(fmt["freq_mark"], sr)
        goe_t = []
        goe_space = []
        goe_mark = []

        for wi in range(num_windows):
            ws = wi * hop_size
            we = ws + window_size
            if we >= len(seg):
                break

            gs = gs_cont.copy()
            gm = gm_cont.copy()
            for i in range(ws, we):
                x = float(seg[i])
                goertzel_process(gs, x)
                goertzel_process(gm, x)

            goe_t.append(t[ws])
            goe_space.append(goertzel_power(gs) / (window_size ** 2))
            goe_mark.append(goertzel_power(gm) / (window_size ** 2))

        ax_goe.plot(goe_t, goe_space, color="cyan", linewidth=0.6,
                    label=f"Space ({fmt['freq_space']:.0f} Hz)")
        ax_goe.plot(goe_t, goe_mark, color="lime", linewidth=0.6,
                    label=f"Mark ({fmt['freq_mark']:.0f} Hz)")
        ax_goe.set_ylabel("Goertzel Energy (norm)")
        ax_goe.set_yscale("log")
        ax_goe.grid(True, alpha=0.3)
        ax_goe.legend(fontsize=8, loc="upper right")
        ax_goe.set_xlim(t[0], t[-1])

        # Per-bit energies overlay (dots where bit = 0 or 1)
        if result and result["bits"] and len(energies) == len(result["bits"]):
            spb = sr / baud
            for bi, (bit, (pe, pn)) in enumerate(zip(result["bits"], energies)):
                bt = (bi * spb) / sr + (start_time or 0)
                norm_e = (pn if bit else pe) / (window_size ** 2)
                color = "lime" if bit else "cyan"
                ax_goe.scatter(bt, norm_e, c=color, s=3, alpha=0.5, zorder=5)
    else:
        ax_goe.text(0.5, 0.5, "No bits decoded", ha="center", va="center",
                    transform=ax_goe.transAxes, fontsize=12, color="gray")
        ax_goe.set_ylabel("Goertzel Energy")

    # Bits and patches (subplot 3, only in full mode)
    if not goertzel_only and result:
        bits = result["bits"]
        if bits:
            spb = sr / baud
            bit_t = np.arange(len(bits)) * spb / sr + (start_time or 0)

            # Plot bits as a stair-step
            ax_bits.step(bit_t, bits, where="post", color="#555555", linewidth=0.8)

            # Highlight patch regions
            for ps, pe, plabel in patch_regions:
                pts = ps * spb / sr + (start_time or 0)
                pte = pe * spb / sr + (start_time or 0)
                ax_bits.axvspan(pts, pte, alpha=0.2, color="gold", zorder=0)
                ax_bits.text((pts + pte) / 2, 1.05, plabel,
                             ha="center", fontsize=7, color="#996600",
                             fontweight="bold")

            ax_bits.set_ylabel("Bit (0=Space, 1=Mark)")
            ax_bits.set_xlabel("Time (seconds)")
            ax_bits.set_ylim(-0.1, 1.4)
            ax_bits.set_yticks([0, 1])
            ax_bits.grid(True, alpha=0.3)
            ax_bits.set_xlim(t[0], t[-1])
        else:
            ax_bits.text(0.5, 0.5, "No bits decoded", ha="center", va="center",
                         transform=ax_bits.transAxes, fontsize=12, color="gray")
            ax_bits.set_xlabel("Time (seconds)")


# ─── PLL-based FSK Decoder (prototype) ─────────────────────────────────────
# Uses quadrature demodulation + early-late gate PLL for bit timing recovery.
# Unlike the brute-force Goertzel decoder which tests 145 speed/phase combos,
# the PLL adaptively tracks the bit clock, compensating for tape wow/flutter.
#
# Pipeline:
#   samples → quadrature mix (cos/sin at fc) → LPF → I/Q baseband
#   → instantaneous freq (phase derivative) → PLL timing recovery → bits


def _design_fsk_lpf(sr, freq_space, freq_mark, num_taps=65):
    """Design LPF for quadrature FSK demodulation.
    
    After mixing with fc = (freq_space + freq_mark) / 2:
    - The mark frequency becomes a positive deviation from fc
    - The space frequency becomes a negative deviation
    - The 2*fc component is high-frequency and needs to be removed
    
    The filter cutoff should pass the deviation bandwidth but reject 2*fc.
    """
    fc = (freq_space + freq_mark) / 2.0
    dev = (freq_mark - freq_space) / 2.0  # max frequency deviation from fc
    cutoff = dev * 3.0  # pass deviation with margin
    # Ensure cutoff is below the 2*fc zone to avoid aliasing
    max_cutoff = fc * 0.8
    cutoff = min(cutoff, max_cutoff)
    if cutoff < 1.0:
        cutoff = dev * 5.0
    cutoff = min(cutoff, sr * 0.4)
    return scipy_signal.firwin(num_taps, cutoff, fs=sr)


def _quadrature_demodulate(samples, sr, freq_space, freq_mark):
    """
    Quadrature demodulation of FSK signal.
    
    Returns instantaneous frequency deviation signal (normalized to -1..1).
    Positive = mark, Negative = space.
    """
    fc = (freq_space + freq_mark) / 2.0
    n = len(samples)
    
    # Mix with cos/sin at center frequency
    t = np.arange(n, dtype=np.float64) / sr
    mix_cos = np.cos(2.0 * np.pi * fc * t)
    mix_sin = np.sin(2.0 * np.pi * fc * t)
    
    I = samples * mix_cos
    Q = samples * mix_sin
    
    # LPF to remove 2*fc component
    lpf = _design_fsk_lpf(sr, freq_space, freq_mark)
    pad = len(lpf) // 2
    I = np.convolve(np.pad(I, pad, mode='edge'), lpf, mode='valid')
    Q = np.convolve(np.pad(Q, pad, mode='edge'), lpf, mode='valid')
    
    # Trim to original length
    I = I[:n]
    Q = Q[:n]
    
    # Instantaneous phase (unwrapped)
    phase = np.unwrap(np.arctan2(Q, I))
    
    # Instantaneous frequency = derivative of phase
    # Scale: d(phase)/dt * sr / (2*pi) gives Hz deviation from fc
    freq_dev = np.diff(phase) * sr / (2.0 * np.pi)
    
    # Normalize: divide by max deviation
    max_dev = (freq_mark - freq_space) / 2.0
    freq_dev = freq_dev / max_dev
    
    return freq_dev


def _extract_bits_from_stream(bits, baud):
    """
    Extract bytes from raw bit stream (start/stop frame format).
    
    Frame format (same as C++ and Goertzel decoder):
      - Idle state: mark (1)
      - Start bit: space (0)
      - 8 data bits: LSB first
      - Stop bit: mark (1)
    
    Returns (bytes_list, validated_patches_list).
    """
    bytes_out = []
    i = 0
    while i + 10 < len(bits):
        # Look for start bit: 1→0 transition
        if bits[i] == 1 and bits[i + 1] == 0:
            byte = 0
            valid = True
            # 8 data bits (LSB first), starting at i+1 (skip the mark we checked)
            # Actually: bits[i] is the idle/stop mark, bits[i+1] is start bit (0)
            # Data bits start at i+2
            for b in range(8):
                idx = i + 2 + b
                if idx < len(bits):
                    if bits[idx] == 1:
                        byte |= (1 << b)
                else:
                    valid = False
                    break
            # Stop bit at i + 10
            stop_idx = i + 2 + 8
            if valid and stop_idx < len(bits) and bits[stop_idx] == 1:
                bytes_out.append(byte & 0x7F)
                i = stop_idx - 1
        i += 1
    
    validated = validate_patches(bytes_out)
    return bytes_out, validated


def decode_fsk_pll(samples, sr, bits_per_second):
    """
    PLL-based FSK decoder using quadrature demodulation + early-late gate.
    
    Key design decisions:
    - Quadrature demodulation gives clean instantaneous frequency signal
      that is robust to amplitude variations and quantization noise
    - Early-late gate PLL tracks bit timing adaptively, handling
      tape wow/flutter without brute-force speed/phase sweep
    - PI loop filter provides fast lock with stable tracking
    
    Args:
        samples: preprocessed audio (float32, normalized)
        sr: sample rate
        bits_per_second: 340 (Juno-60) or 1200 (Juno-106)
    
    Returns dict with same keys as decode_fsk() for drop-in comparison:
        bits, bytes, validated, num_patches, and PLL diagnostics.
    """
    fmt = JUNO_FORMATS[bits_per_second]
    freq_space = fmt["freq_space"]
    freq_mark = fmt["freq_mark"]
    spb = sr / bits_per_second  # samples per bit (nominal)
    
    # --- Step 1: Quadrature demodulation to get instantaneous frequency ---
    # The freq_dev signal is positive for mark (1) and negative for space (0)
    freq_dev = _quadrature_demodulate(samples, sr, freq_space, freq_mark)
    
    # --- Step 2: Early-late gate PLL for bit timing recovery ---
    # PLL state
    phase = 0.0                # fractional sample position within current bit (0..spb)
    freq_est = spb             # estimated samples per bit (starts at nominal)
    integrator = 0.0           # loop filter integrator (for Ki)
    
    # PLL gains (tuned for fast lock with stable tracking)
    Kp = 0.15                  # proportional gain
    Ki = 0.02                  # integral gain
    
    # Early-late gate parameters
    early_late_offset = 0.25   # fraction of bit period for early/late sampling
    
    n = len(freq_dev)
    bits = []
    pll_diagnostics = {
        "phase": [],
        "freq_est": [],
        "timing_error": [],
        "bit_positions": [],
    }
    
    sample_pos = 0.0  # absolute sample position
    
    # Linear interpolation function (defined once outside loop for performance)
    def _interp(pos):
        idx = int(pos)
        frac = pos - idx
        if idx + 1 >= n:
            return freq_dev[-1] if n > 0 else 0.0
        return freq_dev[idx] * (1.0 - frac) + freq_dev[idx + 1] * frac
    
    while int(sample_pos + spb) < n:
        on_time = _interp(sample_pos)
        early = _interp(max(0.0, sample_pos - early_late_offset * spb))
        late = _interp(min(float(n - 1), sample_pos + early_late_offset * spb))
        
        # Timing error: early-late gate on the ABSOLUTE VALUE of the signal
        # For FSK, the energy at the bit center is highest (cleanest distinction)
        # Early/late samples have less reliable energy
        abs_early = abs(early)
        abs_late = abs(late)
        
        # Error signal: positive if late sample is stronger (clock too fast)
        # negative if early sample is stronger (clock too slow)
        timing_error = abs_early - abs_late
        
        # Loop filter (PI controller)
        integrator += Ki * timing_error
        phase_correction = Kp * timing_error + integrator
        
        # Clamp integrator to prevent windup
        integrator = max(-0.5, min(0.5, integrator))
        
        # Update phase (adjust sample advancement speed)
        phase += phase_correction
        
        # Bit decision: on-time sample > 0 → mark (1), else space (0)
        bit = 1 if on_time > 0 else 0
        bits.append(bit)
        
        # Diagnostics
        pll_diagnostics["phase"].append(phase)
        pll_diagnostics["freq_est"].append(freq_est)
        pll_diagnostics["timing_error"].append(timing_error)
        pll_diagnostics["bit_positions"].append(sample_pos)
        
        # Advance by nominal samples per bit, plus any phase correction
        # The phase correction effectively modulates the bit period
        # to track wow/flutter
        sample_pos += spb + phase_correction * spb * 0.1
        
        # Slow decay of integrator to prevent drift
        integrator *= 0.999
    
    # --- Step 3: Extract bytes and validate patches ---
    bytes_out, validated = _extract_bits_from_stream(bits, bits_per_second)
    num_patches = len(validated) // 18
    
    return {
        "bits": bits,
        "bytes": bytes_out,
        "validated": validated,
        "num_patches": num_patches,
        "speed_factor": 0.0,  # Not applicable for PLL
        "phase_offset": 0.0,  # Not applicable for PLL
        "energies": [],        # Not computed for PLL
        "pll_diagnostics": pll_diagnostics,
    }


# ─── PLL-based FSK Decoder v2: Gardner Timing Error Detector ──────────────
#
# Gardner TED formula:  e(n) = x(nT + T/2) * [x(nT) - x((n-1)T)]
#
# Key advantages over early-late gate:
#   1. Only updates on bit transitions → less noise in steady-state
#   2. Self-normalizing when divided by |x(nT) - x((n-1)T)|²
#   3. Produces accurate signed error proportional to timing offset
#   4. Works directly on baseband NRZ signal (frequency deviation for FSK)
#
# The Gardner TED samples at two points per bit:
#   - On-time sample: x(nT) at the bit center
#   - Midpoint sample: x(nT + T/2) halfway between bit centers
#
# The error signal is: e = midpoint * (current_on_time - previous_on_time)
# This is zero when timing is perfect (midpoint ≈ 0 at bit transitions).


def decode_fsk_pll_gardner(samples, sr, bits_per_second):
    """
    PLL-based FSK decoder using quadrature demodulation + Gardner TED.
    
    The Gardner Timing Error Detector provides superior timing recovery
    compared to the early-late gate, especially for FSK signals where
    the frequency deviation produces a clean baseband NRZ waveform.
    
    Advantages over decode_fsk_pll():
    - Only updates on bit transitions (less jitter in steady-state)
    - Self-normalizing timing error magnitude
    - Faster lock acquisition
    - Better tracking of wow/flutter
    
    Args:
        samples: preprocessed audio (float32, normalized)
        sr: sample rate
        bits_per_second: 340 (Juno-60) or 1200 (Juno-106)
    
    Returns dict with same keys as decode_fsk() for drop-in comparison.
    """
    fmt = JUNO_FORMATS[bits_per_second]
    freq_space = fmt["freq_space"]
    freq_mark = fmt["freq_mark"]
    spb = sr / bits_per_second  # samples per bit (nominal)
    half_spb = spb / 2.0
    
    # --- Step 1: Quadrature demodulation to get instantaneous frequency ---
    freq_dev = _quadrature_demodulate(samples, sr, freq_space, freq_mark)
    n = len(freq_dev)
    
    # Avoid extreme values
    freq_dev = np.clip(freq_dev, -5.0, 5.0)
    
    # --- Step 2: Gardner TED PLL for bit timing recovery ---
    # PLL state
    phase = 0.0                # fractional sample position (0..spb)
    integrator = 0.0           # loop filter integrator (for Ki)
    
    # PLL gains (different from early-late because Gardner is more sensitive)
    Kp = 0.08                  # proportional gain (lower than ELG due to higher gain)
    Ki = 0.008                 # integral gain
    
    n = len(freq_dev)
    bits = []
    raw_freq_dev = []          # store freq_dev at each bit center for analysis
    pll_diagnostics = {
        "phase": [],
        "timing_error": [],
        "bit_positions": [],
    }
    
    sample_pos = 0.0           # absolute sample position
    on_time_prev = 0.0         # previous on-time sample (for Gardner TED)
    locked = False
    lock_counter = 0
    LOCK_THRESHOLD = 8          # bits of consistent timing error < threshold
    
    # Linear interpolation function
    def _interp(pos):
        idx = int(pos)
        frac = pos - idx
        if idx + 1 >= n:
            return freq_dev[-1] if n > 0 else 0.0
        return freq_dev[idx] * (1.0 - frac) + freq_dev[idx + 1] * frac
    
    # Pre-compute effective length in bits for timeout
    max_bits = int(n / (spb * 0.7)) + 1000
    bit_count = 0
    
    while int(sample_pos + half_spb + 1.0) < n and bit_count < max_bits:
        # Gardner needs TWO samples: on-time and midpoint
        on_time = _interp(sample_pos)      # x(nT) - bit center
        midpoint = _interp(sample_pos + half_spb)  # x(nT + T/2)
        
        # Gardner TED: e = midpoint * (on_time - on_time_prev)
        # Only valid when there's a bit transition (on_time != on_time_prev)
        transition = on_time - on_time_prev
        timing_error = midpoint * transition
        
        # Normalize by |transition| to make amplitude-independent
        # Add small epsilon to avoid division by zero
        transition_mag = abs(transition) + 1e-10
        timing_error_normalized = timing_error / transition_mag
        
        # Loop filter (PI controller)
        integrator += Ki * timing_error_normalized
        phase_correction = Kp * timing_error_normalized + integrator
        
        # Clamp integrator to prevent windup
        integrator = np.clip(integrator, -0.3, 0.3)
        
        # Update phase
        phase += phase_correction
        
        # Bit decision: on-time sample > 0 → mark (1), else space (0)
        bit = 1 if on_time > 0 else 0
        bits.append(bit)
        raw_freq_dev.append(on_time)
        
        # Diagnostics
        pll_diagnostics["phase"].append(phase)
        pll_diagnostics["timing_error"].append(timing_error_normalized)
        pll_diagnostics["bit_positions"].append(sample_pos)
        
        # Track lock status
        if abs(timing_error_normalized) < 0.05:
            lock_counter += 1
        else:
            lock_counter = 0
        locked = lock_counter >= LOCK_THRESHOLD
        
        # Advance by nominal samples per bit, plus phase correction
        # Gardner phase correction modulates the sample advancement
        sample_pos += spb + phase_correction * 2.0
        
        # Store for next iteration
        on_time_prev = on_time
        bit_count += 1
        
        # Slow decay to prevent drift in long idle periods
        integrator *= 0.9999
    
    # --- Step 3: Extract bytes and validate patches ---
    bytes_out, validated = _extract_bits_from_stream(bits, bits_per_second)
    num_patches = len(validated) // 18
    
    return {
        "bits": bits,
        "bytes": bytes_out,
        "validated": validated,
        "num_patches": num_patches,
        "speed_factor": 0.0,
        "phase_offset": 0.0,
        "energies": [],
        "pll_diagnostics": pll_diagnostics,
    }


# ─── Visualization ──────────────────────────────────────────────────────────

def visualize(wav_path, forced_baud=0, start_time=None, end_time=None,
              save_path=None, goertzel_only=False):
    """Main visualization function."""
    # Load and preprocess
    print(f"Loading: {wav_path}")
    samples_raw, sr, nch = load_wav(wav_path)
    duration = len(samples_raw) / sr
    print(f"  Sample rate: {sr} Hz, Channels: {nch}, Duration: {duration:.1f}s")

    orig_sr = sr
    samples, sr = preprocess(samples_raw, sr, nch)

    # Detect format
    if forced_baud:
        baud = forced_baud
    else:
        baud = detect_format(samples, sr)

    if baud not in JUNO_FORMATS:
        print("  ⚠ Could not determine tape format. Defaulting to Juno-106.")
        baud = 1200

    fmt = JUNO_FORMATS[baud]
    print(f"  Format: {fmt['name']} ({baud} baud, space={fmt['freq_space']:.0f} Hz, "
          f"mark={fmt['freq_mark']:.0f} Hz)")
    if sr != orig_sr:
        print(f"  Upsampled: {orig_sr} Hz → {sr} Hz (linear interpolation)")

    # Crop to requested window
    start_sample = int(start_time * sr) if start_time else 0
    end_sample = int(end_time * sr) if end_time else len(samples)
    if start_time or end_time:
        print(f"  Zoom: {start_time or 0:.1f}s – {end_time or duration:.1f}s")

    seg = samples[start_sample:end_sample]
    t = np.arange(len(seg)) / sr + (start_time or 0)

    # Decode FSK (skip in goertzel-only mode for speed)
    result = None
    if not goertzel_only:
        print("  Decoding FSK (fast pass)...", end=" ", flush=True)
        result = decode_fsk(samples, sr, baud, fast=True)
        num_p = result["num_patches"] if result else 0
        print(f"done. {len(result['bits'])} bits, {len(result['bytes'])} bytes, "
              f"{num_p} patches.")
    else:
        print("  Skipping bit decode (--goertzel-only mode)")

    # Prepare patch regions for annotation (only in full mode)
    patch_regions = []
    if result and result["validated"] and result["bytes"]:
        val_bytes = result["validated"]
        raw_bytes = result["bytes"]
        patch_idx = 0
        search_pos = 0
        while patch_idx * 18 < len(val_bytes) and search_pos < len(raw_bytes):
            patch_data = val_bytes[patch_idx * 18 : (patch_idx + 1) * 18]
            for pos in range(search_pos, len(raw_bytes) - 18):
                if raw_bytes[pos:pos+18] == patch_data:
                    bit_start = (pos * 10) // 1
                    bit_end = ((pos + 18) * 10) // 1
                    if bit_end < len(result["bits"]):
                        patch_regions.append((bit_start, bit_end, f"P{patch_idx+1}"))
                        search_pos = pos + 18
                        patch_idx += 1
                        break

    # ── Plotting ────────────────────────────────────────────────────────────
    if goertzel_only:
        fig, axes = plt.subplots(2, 1, figsize=(16, 6), sharex=True,
                                 gridspec_kw={"height_ratios": [1, 1]},
                                 constrained_layout=True)
        ax_spec = axes[0]
        ax_goe  = axes[1]
    else:
        fig = plt.figure(figsize=(18, 10), constrained_layout=True)
        gs = fig.add_gridspec(4, 1, height_ratios=[1.2, 1.2, 1.5, 1.5],
                              hspace=0.25)
        ax_wave = fig.add_subplot(gs[0])
        ax_spec = fig.add_subplot(gs[1])
        ax_goe  = fig.add_subplot(gs[2])
        ax_bits = fig.add_subplot(gs[3])

    # Normal 1st subplot: Waveform (or Goertzel subplot 0)
    if not goertzel_only:
        ax_wave.plot(t, seg, color="#444444", linewidth=0.4)
        ax_wave.set_ylabel("Amplitude")
        ax_wave.set_title(f"FSK Signal: {fmt['name']} – {os.path.basename(wav_path)}")
        ax_wave.grid(True, alpha=0.3)
        ax_wave.set_xlim(t[0], t[-1])

        # Leader tone annotation
        leader_len = min(len(samples), int(sr * 3.0))
        leader_end_t = leader_len / sr
        ax_wave.axvspan(0, leader_end_t, alpha=0.06, color="blue",
                        label=f"Leader tone ~{fmt['leader_hz']:.0f} Hz")
        ax_wave.legend(fontsize=8, loc="upper right")

    # Spectrogram (subplot 1 or 0)
    f, t_spec, Sxx = scipy_signal.spectrogram(
        seg, sr, nperseg=512, noverlap=384,
        window=("hamming",), scaling="density",
    )

    # Limit frequency range to FSK band
    freq_mask = (f >= 800) & (f <= 3000)
    Sxx_dB = 10 * np.log10(Sxx[freq_mask] + 1e-12)
    vmin = np.percentile(Sxx_dB, 10)
    vmax = np.percentile(Sxx_dB, 95)

    im = ax_spec.pcolormesh(t_spec + (start_time or 0), f[freq_mask],
                            Sxx_dB, shading="gouraud",
                            cmap="inferno", vmin=vmin, vmax=vmax)
    ax_spec.set_ylabel("Frequency (Hz)")
    ax_spec.set_ylim(800, 3000)

    # Mark space/mark frequencies
    ax_spec.axhline(fmt["freq_space"], color="cyan", linestyle="--", linewidth=0.8,
                    label=f"Space {fmt['freq_space']:.0f} Hz")
    ax_spec.axhline(fmt["freq_mark"], color="lime", linestyle="--", linewidth=0.8,
                    label=f"Mark {fmt['freq_mark']:.0f} Hz")
    ax_spec.axhline((fmt["freq_space"] + fmt["freq_mark"]) / 2,
                    color="yellow", linestyle=":", linewidth=0.5, alpha=0.6,
                    label="Decision threshold")
    ax_spec.legend(fontsize=7, loc="upper right")
    ax_spec.grid(True, alpha=0.2)
    ax_spec.set_xlim(t[0], t[-1])

    # Colorbar
    cbar = plt.colorbar(im, ax=ax_spec, fraction=0.05, pad=0.02)
    cbar.set_label("dB", fontsize=8)

    # Goertzel energy (subplot 2 or 1)
    energies = result["energies"] if result else []
    if energies:
        # Compute Goertzel energy over sliding windows for continuous visualization
        window_size = int(sr / baud)  # one bit period
        hop_size = window_size // 4
        num_windows = max(1, (len(seg) - window_size) // hop_size)

        gs_cont = goertzel_init(fmt["freq_space"], sr)
        gm_cont = goertzel_init(fmt["freq_mark"], sr)
        goe_t = []
        goe_space = []
        goe_mark = []

        for wi in range(num_windows):
            ws = wi * hop_size
            we = ws + window_size
            if we >= len(seg):
                break

            gs = gs_cont.copy()
            gm = gm_cont.copy()
            for i in range(ws, we):
                x = float(seg[i])
                goertzel_process(gs, x)
                goertzel_process(gm, x)

            goe_t.append(t[ws])
            goe_space.append(goertzel_power(gs) / (window_size ** 2))
            goe_mark.append(goertzel_power(gm) / (window_size ** 2))

        ax_goe.plot(goe_t, goe_space, color="cyan", linewidth=0.6,
                    label=f"Space ({fmt['freq_space']:.0f} Hz)")
        ax_goe.plot(goe_t, goe_mark, color="lime", linewidth=0.6,
                    label=f"Mark ({fmt['freq_mark']:.0f} Hz)")
        ax_goe.set_ylabel("Goertzel Energy (norm)")
        ax_goe.set_yscale("log")
        ax_goe.grid(True, alpha=0.3)
        ax_goe.legend(fontsize=8, loc="upper right")
        ax_goe.set_xlim(t[0], t[-1])

        # Per-bit energies overlay (dots where bit = 0 or 1)
        if result and result["bits"] and len(energies) == len(result["bits"]):
            spb = sr / baud
            for bi, (bit, (pe, pn)) in enumerate(zip(result["bits"], energies)):
                bt = (bi * spb) / sr + (start_time or 0)
                norm_e = (pn if bit else pe) / (window_size ** 2)
                color = "lime" if bit else "cyan"
                ax_goe.scatter(bt, norm_e, c=color, s=3, alpha=0.5, zorder=5)
    else:
        ax_goe.text(0.5, 0.5, "No bits decoded", ha="center", va="center",
                    transform=ax_goe.transAxes, fontsize=12, color="gray")
        ax_goe.set_ylabel("Goertzel Energy")

    # Bits and patches (subplot 3, only in full mode)
    if not goertzel_only and result:
        bits = result["bits"]
        if bits:
            spb = sr / baud
            bit_t = np.arange(len(bits)) * spb / sr + (start_time or 0)

            # Plot bits as a stair-step
            ax_bits.step(bit_t, bits, where="post", color="#555555", linewidth=0.8)

            # Highlight patch regions
            for ps, pe, plabel in patch_regions:
                pts = ps * spb / sr + (start_time or 0)
                pte = pe * spb / sr + (start_time or 0)
                ax_bits.axvspan(pts, pte, alpha=0.2, color="gold", zorder=0)
                ax_bits.text((pts + pte) / 2, 1.05, plabel,
                             ha="center", fontsize=7, color="#996600",
                             fontweight="bold")

            ax_bits.set_ylabel("Bit (0=Space, 1=Mark)")
            ax_bits.set_xlabel("Time (seconds)")
            ax_bits.set_ylim(-0.1, 1.4)
            ax_bits.set_yticks([0, 1])
            ax_bits.grid(True, alpha=0.3)
            ax_bits.set_xlim(t[0], t[-1])
        else:
            ax_bits.text(0.5, 0.5, "No bits decoded", ha="center", va="center",
                         transform=ax_bits.transAxes, fontsize=12, color="gray")
            ax_bits.set_xlabel("Time (seconds)")

    # Summary text box
    patches_str = f"{result['num_patches']}" if result else "?"
    bytes_str  = f"{len(result['bytes'])}" if result else "?"
    speed_str  = f"{result['speed_factor']:.2f}" if result else "-"
    phase_str  = f"{result['phase_offset']:.1f}" if result else "-"
    summary = (f"Format: {fmt['name']}  |  Baud: {baud}  |  "
               f"Samples: {len(seg):,}  |  Duration: {len(seg)/sr:.1f}s\n"
               f"Patches: {patches_str}  |  "
               f"Best speed: {speed_str}  |  "
               f"Phase: {phase_str}  |  "
               f"Bytes decoded: {bytes_str}")

    fig.text(0.02, 0.005, summary, fontsize=8, family="monospace",
             bbox=dict(boxstyle="round,pad=0.5", facecolor="#ffffdd", alpha=0.8))

    # Print valid patch details
    if result and result["validated"]:
        print(f"\n  Validated patches ({result['num_patches']}):")
        for pi in range(result["num_patches"]):
            offset = pi * 18
            data = result["validated"][offset:offset+18]
            hex_str = " ".join(f"{b:02X}" for b in data)
            print(f"    Patch {pi+1:2d}: {hex_str}")

    # Save or show
    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"\n  Plot saved to: {save_path}")
    else:
        plt.show()


# ─── Main ──────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Visualize FSK cassette tape signals for Juno-60/106")
    parser.add_argument("wav_file", help="Path to WAV file")
    parser.add_argument("--baud", type=int, choices=[340, 1200], default=0,
                        help="Force baud rate (340=Juno-60, 1200=Juno-106)")
    parser.add_argument("--start", type=float, default=None,
                        help="Start time in seconds for zoom window")
    parser.add_argument("--end", type=float, default=None,
                        help="End time in seconds for zoom window")
    parser.add_argument("--save", type=str, default=None,
                        help="Save plot to file instead of displaying")
    parser.add_argument("--goertzel-only", action="store_true",
                        help="Only show Goertzel energy plot (faster)")
    args = parser.parse_args()

    if not os.path.isfile(args.wav_file):
        print(f"Error: File not found: {args.wav_file}")
        sys.exit(1)

    visualize(
        args.wav_file,
        forced_baud=args.baud,
        start_time=args.start,
        end_time=args.end,
        save_path=args.save,
        goertzel_only=args.goertzel_only,
    )


if __name__ == "__main__":
    main()
