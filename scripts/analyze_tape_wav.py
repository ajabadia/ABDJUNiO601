#!/usr/bin/env python3
"""
Analyze Juno-60/106 cassette tape WAV files to understand FSK signal characteristics.
Helps debug and tune the FSK decoder parameters.
"""
import wave
import numpy as np
import struct
import os
from collections import Counter

def analyze_wav(filepath):
    print(f"\n{'='*60}")
    print(f"Analyzing: {filepath}")
    print(f"{'='*60}")
    
    with wave.open(filepath, 'rb') as wav:
        n_channels = wav.getnchannels()
        sampwidth = wav.getsampwidth()
        framerate = wav.getframerate()
        n_frames = wav.getnframes()
        duration = n_frames / framerate
        
        print(f"  Channels: {n_channels}")
        print(f"  Sample rate: {framerate} Hz")
        print(f"  Bit depth: {sampwidth * 8}-bit")
        print(f"  Frames: {n_frames}")
        print(f"  Duration: {duration:.2f} seconds")
        
        raw = wav.readframes(n_frames)
        
    # Convert to float samples
    if sampwidth == 1:
        fmt = f'{n_frames * n_channels}B'
        data = struct.unpack(fmt, raw)
        samples = np.array(data, dtype=np.float32) / 128.0
    elif sampwidth == 2:
        fmt = f'<{n_frames * n_channels}h'
        data = struct.unpack(fmt, raw)
        samples = np.array(data, dtype=np.float32) / 32768.0
    else:
        print("  Unsupported bit depth")
        return
    
    # Mix to mono if stereo
    if n_channels > 1:
        samples = samples.reshape(-1, n_channels).mean(axis=1)
    
    # Normalize
    max_abs = np.max(np.abs(samples))
    if max_abs > 0:
        samples = samples / max_abs
    
    # FFT analysis
    print(f"\n  --- Frequency Analysis ---")
    
    # Analyze in chunks to see frequency distribution over time
    chunk_size = int(framerate * 0.1)  # 100ms chunks
    step = chunk_size // 2
    
    mark_energy = []
    space_energy = []
    mid_energy = []
    
    all_freqs = np.fft.rfftfreq(chunk_size, 1/framerate)
    
    for start in range(0, len(samples) - chunk_size, step):
        chunk = samples[start:start+chunk_size] * np.hanning(chunk_size)
        spec = np.abs(np.fft.rfft(chunk))
        
        # Energy at Juno-60 frequencies
        space_freq_60 = 1360.0
        mark_freq_60 = 2380.0
        
        # Energy at Juno-106 frequencies  
        space_freq_106 = 1300.0
        mark_freq_106 = 2100.0
        
        # Find nearest bins
        def energy_at(spec, freqs, target_freq, bandwidth=100):
            idx = np.where((freqs > target_freq - bandwidth) & (freqs < target_freq + bandwidth))[0]
            if len(idx) > 0:
                return np.mean(spec[idx])
            return 0
        
        space_e_60 = energy_at(spec, all_freqs, space_freq_60)
        mark_e_60 = energy_at(spec, all_freqs, mark_freq_60)
        space_e_106 = energy_at(spec, all_freqs, space_freq_106)
        mark_e_106 = energy_at(spec, all_freqs, mark_freq_106)
        
        mark_energy.append(mark_e_60)
        space_energy.append(space_e_60)
    
    if mark_energy:
        print(f"  Mark (2380 Hz) energy range: {min(mark_energy):.4f} - {max(mark_energy):.4f}, mean: {np.mean(mark_energy):.4f}")
        print(f"  Space (1360 Hz) energy range: {min(space_energy):.4f} - {max(space_energy):.4f}, mean: {np.mean(space_energy):.4f}")
        
        # Determine dominant frequencies
        mark_ratio = np.mean(mark_energy) / (np.mean(mark_energy) + np.mean(space_energy) + 1e-10)
        space_ratio = np.mean(space_energy) / (np.mean(mark_energy) + np.mean(space_energy) + 1e-10)
        print(f"  Mark energy ratio: {mark_ratio:.3f}")
        print(f"  Space energy ratio: {space_ratio:.3f}")
        
        if mark_ratio > 0.6:
            print(f"  -> Likely JUNO-60 format (dominant ~2380 Hz mark)")
        elif space_ratio > 0.6:
            print(f"  -> Likely JUNO-60 format (dominant ~1360 Hz space)")
        else:
            print(f"  -> Mixed or JUNO-106 format")
    
    # Zero crossing analysis per bit period
    print(f"\n  --- Zero Crossing Analysis ---")
    
    for baud_rate, label in [(340, "Juno-60"), (1200, "Juno-106")]:
        samples_per_bit = framerate / baud_rate
        spb_int = int(samples_per_bit + 0.5)
        num_bits = len(samples) // spb_int
        
        zc_per_bit = []
        for b in range(min(num_bits, 10000)):
            start = b * spb_int
            end = min(start + spb_int, len(samples))
            chunk = samples[start:end]
            
            # Count zero crossings (basic)
            zc = np.sum(np.diff(np.signbit(chunk).astype(int)) != 0)
            zc_per_bit.append(zc)
        
        if zc_per_bit:
            zc_arr = np.array(zc_per_bit)
            # Filter out silence bits
            active_bits = zc_arr[zc_arr > 1]
            if len(active_bits) > 0:
                avg_period = 2.0 * spb_int / np.mean(active_bits)
                est_freq = framerate / avg_period if avg_period > 0 else 0
                print(f"  {label} ({baud_rate} baud, {spb_int}s/bit):")
                print(f"    Avg ZC/bit: {np.mean(active_bits):.1f}, est freq: {est_freq:.0f} Hz")
                print(f"    ZC range: {np.min(active_bits)}-{np.max(active_bits)}")
                
                # Histogram of zero crossing counts
                hist = Counter(active_bits.astype(int))
                top_zc = hist.most_common(5)
                print(f"    Top ZC counts: {top_zc}")
                
                # Check if this matches mark or space
                zc_mark = spb_int * 2 * mark_freq_60 / framerate
                zc_space = spb_int * 2 * space_freq_60 / framerate
                print(f"    Expected ZC for mark (2380 Hz): {zc_mark:.1f}")
                print(f"    Expected ZC for space (1360 Hz): {zc_space:.1f}")
                
                # Count how many bits are clearly mark vs space vs ambiguous
                mark_count = np.sum(active_bits > (zc_mark + zc_space) / 2)
                space_count = np.sum(active_bits < (zc_mark + zc_space) / 2)
                print(f"    Bits closer to Mark: {mark_count}, Space: {space_count}")
    
    # Detect leader/pilot tone (continuous mark frequency at start)
    print(f"\n  --- Leader/Pilot Tone ---")
    leader_chunk = samples[:int(framerate * 3)]  # First 3 seconds
    leader_spec = np.abs(np.fft.rfft(leader_chunk * np.hanning(len(leader_chunk))))
    leader_freqs = np.fft.rfftfreq(len(leader_chunk), 1/framerate)
    
    peak_idx = np.argmax(leader_spec[10:]) + 10
    peak_freq = leader_freqs[peak_idx]
    print(f"  Peak frequency in first 3s: {peak_freq:.0f} Hz")
    
    if abs(peak_freq - 2380) < 100:
        print(f"  -> Pilot tone matches Juno-60 mark (2380 Hz)")
    elif abs(peak_freq - 2100) < 100:
        print(f"  -> Pilot tone matches Juno-106 mark (2100 Hz)")
    elif abs(peak_freq - 1360) < 100:
        print(f"  -> Pilot tone matches Juno-60 space (1360 Hz)")
    elif abs(peak_freq - 1300) < 100:
        print(f"  -> Pilot tone matches Juno-106 space (1300 Hz)")
    else:
        print(f"  -> Unknown pilot tone frequency")
    
    # Measure actual baud rate from start bit transitions
    print(f"\n  --- Baud Rate Estimation ---")
    
    # Simple transition detection approach
    # Find regions of high energy (signal present)
    energy = np.convolve(np.abs(samples), np.ones(100)/100, mode='same')
    signal_mask = energy > 0.05
    
    if np.any(signal_mask):
        # Find transitions from high freq (mark) to low freq (space) and back
        # by looking at zero crossing rate changes
        zc_rate = np.zeros(len(samples))
        window = int(framerate * 0.01)  # 10ms window
        for i in range(window, len(samples) - window):
            zc = np.sum(np.diff(np.signbit(samples[i-window:i+window]).astype(int)) != 0)
            zc_rate[i] = zc
        
        # Smooth
        zc_rate = np.convolve(zc_rate, np.ones(50)/50, mode='same')
        
        # Bit transitions show as sudden changes in ZC rate
        zc_diff = np.abs(np.diff(zc_rate))
        threshold = np.percentile(zc_diff[signal_mask[1:]], 90)
        transitions = np.where(zc_diff > threshold)[0]
        
        if len(transitions) > 10:
            gaps = np.diff(transitions)
            valid_gaps = gaps[gaps > framerate * 0.001]  # > 1ms
            if len(valid_gaps) > 0:
                avg_gap = np.mean(valid_gaps)
                est_baud = framerate / avg_gap
                print(f"  Estimated baud rate: {est_baud:.0f} baud (from {len(valid_gaps)} transitions)")
                print(f"  Gap range: {np.min(valid_gaps):.1f} - {np.max(valid_gaps):.1f} samples")
                if abs(est_baud - 340) < 50:
                    print(f"  -> Matches Juno-60 (340 baud)")
                elif abs(est_baud - 1200) < 100:
                    print(f"  -> Matches Juno-106 (1200 baud)")

# Analyze all available tape files
docs_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs")

for root, dirs, files in os.walk(docs_dir):
    for f in sorted(files):
        if f.endswith('.wav'):
            filepath = os.path.join(root, f)
            try:
                analyze_wav(filepath)
            except Exception as e:
                print(f"Error analyzing {filepath}: {e}")

print(f"\n{'='*60}")
print("Analysis complete!")
print(f"{'='*60}")
