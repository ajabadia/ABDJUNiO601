#!/usr/bin/env python3
"""
Spectral Audio Comparer for ABDJUNiO601 — Qualitative A/B Analysis

Compares a reference recording (e.g. original Juno hardware from synthmania.com MP3s)
against the engine's output WAV. Provides visual spectral overlay, FFT comparison,
and quantitative metrics.

Usage:
    python scripts/compare_audio.py ref.wav engine.wav
    python scripts/compare_audio.py ref.mp3 engine.wav --ffmpeg
    python scripts/compare_audio.py ref.wav engine.wav --segment 1.5 4.0  --note C3

Options:
    --segment START END   Only compare a time segment (seconds)
    --ffmpeg              Use ffmpeg for MP3/other format loading
    --output DIR          Save plots to directory (default: show interactive)
    --no-show             Don't show interactive plot (only save)
    --note NOTE           Note name for plot annotation (e.g. C3, E4)
    --label-ref LABEL     Custom label for reference (default: "Reference")
    --label-engine LABEL  Custom label for engine (default: "Engine")
"""

import argparse
import os
import sys
from math import gcd

import numpy as np
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.widgets import SpanSelector
from scipy import signal


# ─── Audio Loading ───────────────────────────────────────────────────────────

def load_audio(filepath, use_ffmpeg=False, target_sr=44100):
    """Load audio file, convert to mono, resample to target_sr.
    
    Returns (samples: np.ndarray, sample_rate: int) or raises.
    """
    ext = os.path.splitext(filepath)[1].lower()
    
    # Try loading with scipy.io.wavfile first for WAV
    if ext == '.wav' and not use_ffmpeg:
        try:
            sr, data = _load_wav_scipy(filepath)
            return _to_mono_resample(data, sr, target_sr)
        except Exception as e:
            print(f"  scipy failed: {e}, trying ffmpeg...")
    
    # Fallback: use ffmpeg
    if use_ffmpeg or ext != '.wav':
        try:
            return _load_via_ffmpeg(filepath, target_sr)
        except Exception as e:
            if ext == '.mp3':
                print(f"  ffmpeg not available. Install ffmpeg or convert MP3 to WAV first.")
                print(f"  Error: {e}")
                sys.exit(1)
            raise
    
    raise ValueError(f"Could not load {filepath}")


def _load_wav_scipy(filepath):
    """Load WAV using scipy.io.wavfile."""
    from scipy.io import wavfile
    sr, data = wavfile.read(filepath)
    return sr, data.astype(np.float64)


def _load_via_ffmpeg(filepath, target_sr):
    """Load audio via ffmpeg subprocess and pipe through numpy."""
    import subprocess
    cmd = [
        'ffmpeg', '-i', filepath,
        '-f', 'f32le',          # 32-bit float PCM
        '-acodec', 'pcm_f32le',
        '-ar', str(target_sr),
        '-ac', '1',             # mono
        '-loglevel', 'error',
        'pipe:1'
    ]
    proc = subprocess.run(cmd, capture_output=True, check=True)
    data = np.frombuffer(proc.stdout, dtype=np.float32).astype(np.float64)
    return target_sr, data


def _to_mono_resample(data, orig_sr, target_sr):
    """Convert to mono float64 and resample to target sample rate."""
    # Convert to float if needed
    if data.dtype.kind == 'i':
        info = np.iinfo(data.dtype)
        data = data.astype(np.float64) / max(abs(info.min), info.max)
    elif data.dtype.kind == 'u':
        data = data.astype(np.float64) / np.iinfo(data.dtype).max
        data = data * 2.0 - 1.0
    
    # Stereo to mono
    if data.ndim == 2:
        data = data.mean(axis=1)
    
    # Resample if needed — use polyphase resampling for better transient preservation
    if orig_sr != target_sr:
        g = gcd(orig_sr, target_sr)
        data = signal.resample_poly(data, target_sr // g, orig_sr // g)
    
    return data, target_sr


# ─── Time Alignment ──────────────────────────────────────────────────────────

def align_signals(ref, engine, sr, max_offset_sec=2.0):
    """Align engine signal to reference using cross-correlation.
    
    Returns (ref_aligned, engine_aligned, offset_samples).
    """
    max_offset = int(max_offset_sec * sr)
    
    # Normalize for correlation
    ref_norm = ref / (np.linalg.norm(ref) + 1e-10)
    engine_norm = engine / (np.linalg.norm(engine) + 1e-10)
    
    # Cross-correlation
    corr = signal.correlate(engine_norm, ref_norm, mode='same')
    mid = len(corr) // 2
    peak_idx = np.argmax(np.abs(corr))
    offset = peak_idx - mid
    
    # Clamp to max_offset
    if abs(offset) > max_offset:
        print(f"  Warning: offset {offset} samples exceeds max {max_offset}, clamping")
        offset = np.clip(offset, -max_offset, max_offset)
    
    # Align
    if offset > 0:
        engine_aligned = engine[offset:]
        ref_aligned = ref
        # Trim to match lengths
        min_len = min(len(ref_aligned), len(engine_aligned))
        ref_aligned = ref_aligned[:min_len]
        engine_aligned = engine_aligned[:min_len]
    else:
        ref_aligned = ref[-offset:]
        engine_aligned = engine
        min_len = min(len(ref_aligned), len(engine_aligned))
        ref_aligned = ref_aligned[:min_len]
        engine_aligned = engine_aligned[:min_len]
    
    print(f"  Alignment offset: {offset} samples ({offset/sr:.3f}s)")
    return ref_aligned, engine_aligned, offset


# ─── Spectral Analysis ───────────────────────────────────────────────────────

def compute_spectrum(samples, sr, n_fft=4096, hop_length=None):
    """Compute magnitude spectrogram and average spectrum."""
    if hop_length is None:
        hop_length = n_fft // 4
    
    frequencies = np.fft.rfftfreq(n_fft, 1/sr)
    
    # STFT
    _, _, stft = signal.stft(samples, fs=sr, nperseg=n_fft, noverlap=n_fft - hop_length)
    magnitude = np.abs(stft)
    
    # Average spectrum over time
    avg_spectrum = magnitude.mean(axis=1)
    
    # Spectral centroid
    spectral_centroid = np.sum(frequencies * avg_spectrum) / (np.sum(avg_spectrum) + 1e-10)
    
    return frequencies, magnitude, avg_spectrum, spectral_centroid


def compute_metrics(ref, engine, sr):
    """Compute comparison metrics between two aligned signals."""
    min_len = min(len(ref), len(engine))
    ref = ref[:min_len]
    engine = engine[:min_len]
    
    # RMS difference
    rms_diff = np.sqrt(np.mean((ref - engine) ** 2))
    
    # RMS levels
    rms_ref = np.sqrt(np.mean(ref ** 2))
    rms_engine = np.sqrt(np.mean(engine ** 2))
    
    # Correlation coefficient (temporal similarity)
    if np.std(ref) > 1e-10 and np.std(engine) > 1e-10:
        corr_coef = np.corrcoef(ref, engine)[0, 1]
    else:
        corr_coef = 0.0
    
    # Spectral similarity
    freq, spec_ref, _, centroid_ref = compute_spectrum(ref, sr)
    _, spec_engine, _, centroid_engine = compute_spectrum(engine, sr)
    
    # Normalize spectrums
    spec_ref_norm = spec_ref / (np.sum(spec_ref) + 1e-10)
    spec_engine_norm = spec_engine / (np.sum(spec_engine) + 1e-10)
    
    # Spectral similarity (cosine similarity)
    spectral_sim = np.sum(spec_ref_norm * spec_engine_norm) / (
        np.sqrt(np.sum(spec_ref_norm ** 2)) * np.sqrt(np.sum(spec_engine_norm ** 2)) + 1e-10
    )
    
    # Spectral centroid ratio
    centroid_ratio = centroid_engine / (centroid_ref + 1e-10)
    
    # Spectral rolloff (frequency below which 85% of energy is contained)
    cumsum_ref = np.cumsum(spec_ref_norm)
    cumsum_engine = np.cumsum(spec_engine_norm)
    rolloff_idx_ref = min(np.searchsorted(cumsum_ref, 0.85), len(freq) - 1)
    rolloff_idx_engine = min(np.searchsorted(cumsum_engine, 0.85), len(freq) - 1)
    rolloff_ref = freq[rolloff_idx_ref]
    rolloff_engine = freq[rolloff_idx_engine]
    
    return {
        'rms_diff': rms_diff,
        'rms_ref_db': 20 * np.log10(rms_ref + 1e-10),
        'rms_engine_db': 20 * np.log10(rms_engine + 1e-10),
        'rms_ratio_db': 20 * np.log10(rms_engine / (rms_ref + 1e-10)),
        'correlation': corr_coef,
        'spectral_similarity': spectral_sim,
        'centroid_ref_hz': centroid_ref,
        'centroid_engine_hz': centroid_engine,
        'centroid_ratio': centroid_ratio,
        'rolloff_ref_hz': rolloff_ref,
        'rolloff_engine_hz': rolloff_engine,
        'duration_sec': min_len / sr,
    }


# ─── Visualization ───────────────────────────────────────────────────────────

def plot_comparison(ref, engine, sr, metrics, segment=None,
                    label_ref="Reference", label_engine="Engine", note=""):
    """Create a comprehensive comparison plot with 4 panels."""
    fig, axes = plt.subplots(4, 1, figsize=(14, 12), constrained_layout=True)
    fig.suptitle(f"Audio A/B Comparison — {label_ref} vs {label_engine}" +
                 (f"  ({note})" if note else ""),
                 fontsize=14, fontweight='bold')
    
    min_len = min(len(ref), len(engine))
    t = np.arange(min_len) / sr
    
    # Panel 1: Waveforms overlaid
    ax1 = axes[0]
    ax1.plot(t, ref[:min_len], alpha=0.7, label=label_ref, color='#2196F3', linewidth=0.8)
    ax1.plot(t, engine[:min_len], alpha=0.7, label=label_engine, color='#FF5722', linewidth=0.8)
    ax1.set_ylabel('Amplitude')
    ax1.set_title('Waveform Overlay')
    ax1.legend(fontsize=9)
    ax1.grid(True, alpha=0.3)
    if segment:
        ax1.axvspan(segment[0], segment[1], alpha=0.1, color='yellow')
        ax1.set_xlim(segment)
    
    # Panel 2: Average Spectrum (FFT) overlay
    ax2 = axes[1]
    freq, _, avg_spec_ref, centroid_ref = compute_spectrum(ref, sr)
    _, _, avg_spec_engine, centroid_engine = compute_spectrum(engine, sr)
    
    # Normalize for comparison
    avg_spec_ref_db = 20 * np.log10(avg_spec_ref / (np.max(avg_spec_ref) + 1e-10) + 1e-10)
    avg_spec_engine_db = 20 * np.log10(avg_spec_engine / (np.max(avg_spec_engine) + 1e-10) + 1e-10)
    
    ax2.semilogx(freq, avg_spec_ref_db, alpha=0.8, label=label_ref, color='#2196F3', linewidth=1.2)
    ax2.semilogx(freq, avg_spec_engine_db, alpha=0.8, label=label_engine, color='#FF5722', linewidth=1.2, linestyle='--')
    
    # Mark spectral centroids
    ax2.axvline(centroid_ref, color='#2196F3', alpha=0.5, linestyle=':', linewidth=1)
    ax2.axvline(centroid_engine, color='#FF5722', alpha=0.5, linestyle=':', linewidth=1)
    
    ax2.set_xlabel('Frequency (Hz)')
    ax2.set_ylabel('Magnitude (dB)')
    ax2.set_title(f'Average Spectrum (Centroid: {label_ref}={centroid_ref:.0f} Hz, '
                  f'{label_engine}={centroid_engine:.0f} Hz)')
    ax2.legend(fontsize=9)
    ax2.grid(True, alpha=0.3, which='both')
    ax2.set_xlim(20, 16000)
    ax2.set_ylim(-80, 5)
    
    # Panel 3: Spectrogram difference (ref - engine)
    ax3 = axes[2]
    _, _, stft_ref = signal.stft(ref, fs=sr, nperseg=2048, noverlap=1536)
    _, _, stft_engine = signal.stft(engine, fs=sr, nperseg=2048, noverlap=1536)
    
    min_frames = min(stft_ref.shape[1], stft_engine.shape[1])
    spec_ref_db = 20 * np.log10(np.abs(stft_ref[:, :min_frames]) + 1e-10)
    spec_engine_db = 20 * np.log10(np.abs(stft_engine[:, :min_frames]) + 1e-10)
    
    # Difference spectrogram
    diff_db = spec_ref_db - spec_engine_db
    # Clip to reasonable range
    diff_db = np.clip(diff_db, -20, 20)
    
    t_spec = np.arange(min_frames) * (2048 - 1536) / sr
    f_spec = np.fft.rfftfreq(2048, 1/sr)
    
    im = ax3.pcolormesh(t_spec, f_spec, diff_db, shading='gouraud',
                        cmap='RdBu_r', vmin=-20, vmax=20)
    cbar = fig.colorbar(im, ax=ax3, label='Difference (dB)', shrink=0.8)
    ax3.set_ylabel('Frequency (Hz)')
    ax3.set_xlabel('Time (s)')
    ax3.set_title(f'Spectrogram Difference ({label_ref} — {label_engine})')
    ax3.set_ylim(0, 8000)
    
    # Panel 4: Metrics table
    ax4 = axes[3]
    ax4.axis('off')
    
    rows = [
        ['Metric', label_ref, label_engine, 'Match'],
        ['RMS Level', f"{metrics['rms_ref_db']:.1f} dB", f"{metrics['rms_engine_db']:.1f} dB",
         f"{metrics['rms_ratio_db']:+.1f} dB"],
        ['Spectral Centroid', f"{metrics['centroid_ref_hz']:.0f} Hz", f"{metrics['centroid_engine_hz']:.0f} Hz",
         f"{metrics['centroid_ratio']:.3f}x"],
        ['Spectral Rolloff (85%)', f"{metrics['rolloff_ref_hz']:.0f} Hz", f"{metrics['rolloff_engine_hz']:.0f} Hz",
         f"{metrics['rolloff_engine_hz']/metrics['rolloff_ref_hz']:.3f}x"],
        ['Temporal Correlation', '—', '—', f"{metrics['correlation']:.4f}"],
        ['Spectral Similarity', '—', '—', f"{metrics['spectral_similarity']:.4f}"],
        ['RMS Diff', '—', '—', f"{metrics['rms_diff']:.5f}"],
        ['Duration', f"{metrics['duration_sec']:.2f}s", f"{metrics['duration_sec']:.2f}s", '—'],
    ]
    
    table = ax4.table(cellText=rows, loc='center', cellLoc='center',
                      colWidths=[0.22, 0.20, 0.20, 0.20])
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1, 1.6)
    
    # Style header row
    for j in range(4):
        table[0, j].set_facecolor('#1a237e')
        table[0, j].set_text_props(color='white', fontweight='bold')
    
    # Color the match column
    for i in range(1, len(rows)):
        cell = table[i, 3]
        val = rows[i][3]
        if 'dB' in val:
            db_val = float(val.replace(' dB', '').replace('+', ''))
            if abs(db_val) < 2:
                cell.set_facecolor('#c8e6c9')  # green
            elif abs(db_val) < 6:
                cell.set_facecolor('#fff9c4')  # yellow
            else:
                cell.set_facecolor('#ffcdd2')  # red
        elif 'x' in val:
            ratio = float(val.replace('x', ''))
            if 0.8 < ratio < 1.25:
                cell.set_facecolor('#c8e6c9')
            elif 0.5 < ratio < 2.0:
                cell.set_facecolor('#fff9c4')
            else:
                cell.set_facecolor('#ffcdd2')
        elif val != '—':
            try:
                num_val = float(val)
                if num_val > 0.8:
                    cell.set_facecolor('#c8e6c9')
                elif num_val > 0.5:
                    cell.set_facecolor('#fff9c4')
                else:
                    cell.set_facecolor('#ffcdd2')
            except:
                pass
    
    ax4.set_title('Comparison Metrics', fontweight='bold')
    
    return fig


# ─── Interactive Segment Selector ────────────────────────────────────────────

class SegmentSelector:
    """Interactive span selector for picking time segments."""
    
    def __init__(self, ref, engine, sr, label_ref="Ref", label_engine="Engine"):
        self.ref = ref
        self.engine = engine
        self.sr = sr
        self.label_ref = label_ref
        self.label_engine = label_engine
        self.selected_segment = None
        
    def select(self):
        """Open interactive selector, return (start, end) in seconds."""
        fig, ax = plt.subplots(figsize=(14, 3))
        t = np.arange(min(len(self.ref), len(self.engine))) / self.sr
        
        ax.plot(t, self.ref[:len(t)], alpha=0.7, label=self.label_ref, color='#2196F3', linewidth=0.8)
        ax.plot(t, self.engine[:len(t)], alpha=0.7, label=self.label_engine, color='#FF5722', linewidth=0.8)
        ax.set_xlabel('Time (s)')
        ax.set_ylabel('Amplitude')
        ax.set_title('Drag to select a segment for comparison, then close the window')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        def on_select(xmin, xmax):
            self.selected_segment = (xmin, xmax)
            ax.axvspan(xmin, xmax, alpha=0.2, color='yellow')
            fig.canvas.draw()
            print(f"  Selected segment: {xmin:.2f}s — {xmax:.2f}s")
        
        span = SpanSelector(ax, on_select, 'horizontal', useblit=True,
                           props=dict(alpha=0.3, facecolor='yellow'))
        
        plt.show()
        return self.selected_segment


# ─── Main ────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Spectral Audio Comparer for ABDJUNiO601 — Qualitative A/B Analysis",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s ref.wav engine.wav
  %(prog)s ref.mp3 engine.wav --ffmpeg
  %(prog)s ref.wav engine.wav --segment 1.5 4.0
  %(prog)s ref.wav engine.wav --interactive
  %(prog)s ref.wav engine.wav --output ./comparisons/ --no-show
        """
    )
    parser.add_argument('reference', help='Reference WAV/MP3 file (e.g. original Juno recording)')
    parser.add_argument('engine', help='Engine output WAV file')
    parser.add_argument('--segment', nargs=2, type=float, metavar=('START', 'END'),
                        help='Time segment to compare (seconds)')
    parser.add_argument('--interactive', '-i', action='store_true',
                        help='Interactive mode: select segment by clicking/dragging on waveform')
    parser.add_argument('--ffmpeg', action='store_true',
                        help='Use ffmpeg for audio loading (supports MP3, FLAC, etc.)')
    parser.add_argument('--output', '-o', default=None,
                        help='Directory to save plot images (default: show interactive window)')
    parser.add_argument('--no-show', action='store_true',
                        help='Do not show interactive plot (only save)')
    parser.add_argument('--note', default='',
                        help='Note name for annotation (e.g. C3, E4)')
    parser.add_argument('--label-ref', default='Reference',
                        help='Custom label for reference (default: Reference)')
    parser.add_argument('--label-engine', default='Engine',
                        help='Custom label for engine (default: Engine)')
    
    args = parser.parse_args()
    
    # Validate files
    for fpath, fname in [(args.reference, 'Reference'), (args.engine, 'Engine')]:
        if not os.path.exists(fpath):
            print(f"Error: {fname} file not found: {fpath}")
            sys.exit(1)
    
    print("=" * 60)
    print("  ABDJUNiO601 — Spectral Audio Comparer")
    print("=" * 60)
    print(f"\nLoading reference: {args.reference}")
    ref, sr_ref = load_audio(args.reference, use_ffmpeg=args.ffmpeg)
    print(f"  → {len(ref)} samples @ {sr_ref} Hz ({len(ref)/sr_ref:.2f}s)")
    
    print(f"\nLoading engine:   {args.engine}")
    engine, sr_engine = load_audio(args.engine, use_ffmpeg=args.ffmpeg)
    print(f"  → {len(engine)} samples @ {sr_engine} Hz ({len(engine)/sr_engine:.2f}s)")
    
    # Resample if needed — use polyphase for better transient preservation
    target_sr = min(sr_ref, sr_engine)
    if sr_ref != target_sr:
        g = gcd(int(sr_ref), int(target_sr))
        print(f"\nResampling reference to {target_sr} Hz...")
        ref = signal.resample_poly(ref, target_sr // g, sr_ref // g)
    if sr_engine != target_sr:
        g = gcd(int(sr_engine), int(target_sr))
        print(f"\nResampling engine to {target_sr} Hz...")
        engine = signal.resample_poly(engine, target_sr // g, sr_engine // g)
    sr = target_sr
    
    # Interactive segment selection
    segment = args.segment
    if args.interactive:
        print("\nOpening interactive segment selector...")
        selector = SegmentSelector(ref, engine, sr,
                                  label_ref=args.label_ref, label_engine=args.label_engine)
        segment = selector.select()
        if segment is None:
            print("  No segment selected, using full audio")
    
    # Apply segment
    if segment:
        start_s, end_s = segment
        start_s = max(0, start_s)
        end_s = min(end_s, min(len(ref), len(engine)) / sr)
        ref_trim = ref[int(start_s * sr):int(end_s * sr)]
        engine_trim = engine[int(start_s * sr):int(end_s * sr)]
        print(f"\nUsing segment: {start_s:.2f}s — {end_s:.2f}s ({len(ref_trim)/sr:.2f}s duration)")
    else:
        ref_trim = ref
        engine_trim = engine
        print(f"\nUsing full audio: {len(ref_trim)/sr:.2f}s")
    
    # Align signals
    print("\nAligning signals...")
    ref_aligned, engine_aligned, offset = align_signals(ref_trim, engine_trim, sr)
    print(f"  Aligned length: {len(ref_aligned)/sr:.3f}s")
    
    # Compute metrics
    print("\nComputing metrics...")
    metrics = compute_metrics(ref_aligned, engine_aligned, sr)
    
    print(f"\n{'─' * 50}")
    print(f"  Results Summary")
    print(f"{'─' * 50}")
    print(f"  Duration:          {metrics['duration_sec']:.2f}s")
    print(f"  RMS:               Ref {metrics['rms_ref_db']:.1f} dB  |  Engine {metrics['rms_engine_db']:.1f} dB  |  Δ {metrics['rms_ratio_db']:+.1f} dB")
    print(f"  Spectral Centroid: Ref {metrics['centroid_ref_hz']:.0f} Hz | Engine {metrics['centroid_engine_hz']:.0f} Hz | Ratio {metrics['centroid_ratio']:.3f}x")
    print(f"  Spectral Rolloff:  Ref {metrics['rolloff_ref_hz']:.0f} Hz | Engine {metrics['rolloff_engine_hz']:.0f} Hz")
    print(f"  Correlation:       {metrics['correlation']:.4f}")
    print(f"  Spectral Similarity: {metrics['spectral_similarity']:.4f}")
    print(f"{'─' * 50}")
    
    # Interpret
    print(f"\n  Interpretation:")
    rms_ok = abs(metrics['rms_ratio_db']) < 2
    centroid_ok = 0.8 < metrics['centroid_ratio'] < 1.25
    sim_ok = metrics['spectral_similarity'] > 0.7
    
    if rms_ok and centroid_ok and sim_ok:
        print(f"  ✅ GOOD MATCH — Timbral character is similar")
    elif rms_ok and centroid_ok:
        print(f"  ⚠️ PARTIAL MATCH — Levels and brightness are close, but spectral detail differs")
    else:
        print(f"  ❌ DIFFERENT — Significant timbral differences detected")
    
    print(f"     - RMS level: {'✅' if rms_ok else '❌'} (Δ {metrics['rms_ratio_db']:+.1f} dB, target < ±2 dB)")
    print(f"     - Brightness: {'✅' if centroid_ok else '❌'} (ratio {metrics['centroid_ratio']:.3f}x, target 0.8-1.25)")
    print(f"     - Spectral detail: {'✅' if sim_ok else '❌'} ({metrics['spectral_similarity']:.4f}, target > 0.7)")
    
    # Plot
    print(f"\nGenerating comparison plot...")
    fig = plot_comparison(ref_aligned, engine_aligned, sr, metrics, segment,
                          label_ref=args.label_ref, label_engine=args.label_engine,
                          note=args.note)
    
    # Save or show
    if args.output:
        os.makedirs(args.output, exist_ok=True)
        ref_name = os.path.splitext(os.path.basename(args.reference))[0]
        engine_name = os.path.splitext(os.path.basename(args.engine))[0]
        outpath = os.path.join(args.output, f"compare_{ref_name}_vs_{engine_name}.png")
        fig.savefig(outpath, dpi=150, bbox_inches='tight')
        print(f"  Saved to: {outpath}")
    
    if not args.no_show:
        plt.show()
    else:
        plt.close(fig)
    
    print("\nDone.")


if __name__ == '__main__':
    main()
