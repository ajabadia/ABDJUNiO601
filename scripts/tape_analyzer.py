#!/usr/bin/env python3
"""
TapeAnalyzer — Signal Quality Analysis for FSK Cassette Tape Dumps
==================================================================
Extracts 6 quantitative metrics from preprocessed tape audio to determine
decoding strategy (SmartTapeReader Phase 1).

Metrics:
1. Leader SNR     — Energy in FSK band (800-3000 Hz) vs noise out-of-band
2. Frequency jitter — Std dev of leader tone frequency (Goertzel sliding window)
3. Effective duration — Time from leader end to last valid data
4. DC Bias residual  — Mean of signal after HPF
5. Dropout zones     — % of samples below 5% of peak amplitude in data zone
6. Occupied bandwidth — Max frequency with significant energy in spectrogram

Output: dict with all metrics + composite quality_score (0.0-1.0) + format detection

Usage:
    python scripts/tape_analyzer.py <wav_file> [--baud 340|1200] [--verbose]
"""

import sys, os, math, argparse, time
import numpy as np
from scipy import signal as scipy_signal

# Import the shared preprocessing pipeline
sys.path.insert(0, 'scripts')
import importlib.util
spec = importlib.util.spec_from_file_location('vis', 'scripts/visualize_tape.py')
vis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vis)


class TapeAnalyzer:
    """
    Analyzes preprocessed FSK tape audio and extracts quality metrics.
    
    Usage:
        analyzer = TapeAnalyzer(samples, sr)
        metrics = analyzer.analyze(baud=1200)
        # metrics = {
        #     "snr_db": 22.4,
        #     "jitter_pct": 5.1,
        #     "duration_s": 48.0,
        #     "dc_bias": 0.003,
        #     "dropout_pct": 3.2,
        #     "bandwidth_hz": 2450,
        #     "detected_baud": 1200,
        #     "quality_score": 0.62,
        #     "quality_label": "FAIR",
        # }
    """
    
    def __init__(self, samples, sr):
        """
        Args:
            samples: Preprocessed float32 audio (mono, DC-removed, normalized)
            sr: Sample rate in Hz
        """
        self.samples = np.asarray(samples, dtype=np.float64)
        self.sr = float(sr)
        self.n = len(self.samples)
        self.duration_s = self.n / self.sr
        
        # No cache needed — each metric calls Goertzel on different frequencies/segments
    
    # ─── Goertzel helpers ───────────────────────────────────────────────
    
    def _goertzel_init(self, freq):
        omega = 2.0 * math.pi * freq / self.sr
        return {"s1": 0.0, "s2": 0.0, "coeff": 2.0 * math.cos(omega)}
    
    def _goertzel_power(self, st):
        return st["s1"] * st["s1"] + st["s2"] * st["s2"] - st["coeff"] * st["s1"] * st["s2"]
    
    def _goertzel_on_segment(self, start, length, freq):
        """Compute Goertzel power for a single frequency on a segment."""
        st = self._goertzel_init(freq)
        for i in range(start, min(start + length, self.n)):
            x = self.samples[i]
            s0 = x + st["coeff"] * st["s1"] - st["s2"]
            st["s2"] = st["s1"]
            st["s1"] = s0
        return self._goertzel_power(st)
    
    # ─── Format Detection ──────────────────────────────────────────────
    
    def detect_format(self):
        """
        Detect tape format from leader tone frequency.
        Uses Goertzel on space and mark frequencies to decide.
        
        Returns: 340 (Juno-60), 1200 (Juno-106), or 0 (unknown)
        """
        leader_len = min(self.n, int(self.sr * 3.0))
        if leader_len < int(self.sr * 0.5):
            return 0
        
        # Find onset (skip leading silence)
        global_peak = float(np.max(np.abs(self.samples[:leader_len])))
        if global_peak < 0.001:
            return 0
        
        onset_threshold = global_peak * 0.05
        leader_start = 0
        step = int(self.sr * 0.1)
        while leader_start < leader_len:
            chunk_end = min(leader_len, leader_start + step)
            local_peak = float(np.max(np.abs(self.samples[leader_start:chunk_end])))
            if local_peak > onset_threshold:
                break
            leader_start += step
        
        if leader_start >= leader_len:
            return 0
        
        # Restrict analysis to ~0.5s after onset (pure leader, no data)
        leader_seg_end = min(leader_len, leader_start + int(self.sr * 0.5))
        seg_len = leader_seg_end - leader_start
        if seg_len < int(self.sr * 0.2):
            return 0
        
        # Goertzel at both mark frequencies
        power_j106 = self._goertzel_on_segment(leader_start, seg_len, 2100.0)
        power_j60  = self._goertzel_on_segment(leader_start, seg_len, 2380.0)
        
        norm = float(seg_len * seg_len)
        energy_j106 = power_j106 / norm
        energy_j60  = power_j60  / norm
        
        min_energy = 1e-8
        if energy_j106 < min_energy and energy_j60 < min_energy:
            return 0
        
        if energy_j60 > energy_j106 * 2.0:
            return 340   # Juno-60 (2380 Hz dominant)
        elif energy_j106 > energy_j60 * 2.0:
            return 1200  # Juno-106 (2100 Hz dominant)
        else:
            return 0     # Too close to call
    
    # ─── Metric 1: Leader SNR ──────────────────────────────────────────
    
    def estimate_snr(self, baud=1200):
        """
        Estimate SNR in FSK band during the leader tone.
        
        Method: 
        - Signal energy = Goertzel energy at the leader mark frequency
        - Noise energy = Goertzel energy at a frequency between mark and space
                        (where no FSK energy should be present)
        - SNR = 10 * log10(signal_power / noise_power)
        
        Returns: SNR in dB, or 0.0 if leader too short/quiet
        """
        leader_len = min(self.n, int(self.sr * 3.0))
        if leader_len < int(self.sr * 0.5):
            return 0.0
        
        # Find onset
        global_peak = float(np.max(np.abs(self.samples[:leader_len])))
        if global_peak < 0.001:
            return 0.0
        
        onset_threshold = global_peak * 0.05
        leader_start = 0
        step = int(self.sr * 0.1)
        while leader_start < leader_len:
            chunk_end = min(leader_len, leader_start + step)
            local_peak = float(np.max(np.abs(self.samples[leader_start:chunk_end])))
            if local_peak > onset_threshold:
                break
            leader_start += step
        
        if leader_start >= leader_len:
            return 0.0
        
        # Use middle 60% of leader for stable measurement
        leader_seg_end = min(leader_len, leader_start + int(self.sr * 0.5))
        mid_start = leader_start + int((leader_seg_end - leader_start) * 0.2)
        mid_end = leader_start + int((leader_seg_end - leader_start) * 0.8)
        seg_len = mid_end - mid_start
        if seg_len < int(self.sr * 0.1):
            return 0.0
        
        # Determine the mark frequency for this baud rate
        if baud == 340:
            mark_freq = 2380.0
        elif baud == 1200:
            mark_freq = 2100.0
        else:
            # Auto-detect
            detected = self.detect_format()
            if detected == 340:
                mark_freq = 2380.0
            else:
                mark_freq = 2100.0
        
        # Space frequency (where we expect some signal but less)
        space_freq = 1360.0 if baud == 340 else 1300.0
        
        # Noise frequency: midway between mark and space (minimal FSK energy)
        noise_freq = (mark_freq + space_freq) / 2.0
        
        # Signal power: Goertzel at mark frequency
        signal_power = self._goertzel_on_segment(mid_start, seg_len, mark_freq)
        
        # Noise power: Goertzel at noise frequency (between mark and space)
        noise_power = self._goertzel_on_segment(mid_start, seg_len, noise_freq)
        
        # Also measure noise in an out-of-band region (above FSK)
        oob_power = self._goertzel_on_segment(mid_start, seg_len, 4000.0)
        
        # Use the maximum (worst case) of in-band noise and out-of-band noise as the noise floor.
        # Conservative estimate: higher noise floor = lower SNR = safer strategy selection.
        noise_floor = max(noise_power, oob_power)
        
        if noise_floor < 1e-12 or signal_power < noise_floor:
            return 0.0
        
        snr_db = 10.0 * math.log10(signal_power / noise_floor)
        
        # Clamp to reasonable range
        return max(0.0, min(60.0, snr_db))
    
    # ─── Metric 2: Frequency Jitter ────────────────────────────────────
    
    def measure_jitter(self, baud=1200):
        """
        Measure frequency stability of the leader tone.
        
        Method:
        - Divide leader tone into ~50ms sliding windows
        - For each window, estimate frequency via zero-crossing counting
        - Compute std dev of frequency estimates → jitter as % of nominal
        
        Returns: jitter percentage (0.0-15.0), or 0.0 if leader too short
        """
        leader_len = min(self.n, int(self.sr * 3.0))
        if leader_len < int(self.sr * 0.5):
            return 0.0
        
        # Find onset
        global_peak = float(np.max(np.abs(self.samples[:leader_len])))
        if global_peak < 0.001:
            return 0.0
        
        onset_threshold = global_peak * 0.05
        leader_start = 0
        step = int(self.sr * 0.1)
        while leader_start < leader_len:
            chunk_end = min(leader_len, leader_start + step)
            local_peak = float(np.max(np.abs(self.samples[leader_start:chunk_end])))
            if local_peak > onset_threshold:
                break
            leader_start += step
        
        if leader_start >= leader_len:
            return 0.0
        
        # Nominal frequency for this baud rate
        if baud == 340:
            nominal_freq = 2380.0
        elif baud == 1200:
            nominal_freq = 2100.0
        else:
            detected = self.detect_format()
            nominal_freq = 2380.0 if detected == 340 else 2100.0
        
        # Sliding window: 50ms (~50ms × sr samples)
        window_size = int(self.sr * 0.05)
        hop_size = window_size // 2
        
        # Skip first 100ms of leader (startup transient)
        analysis_start = leader_start + int(self.sr * 0.1)
        analysis_len = min(leader_len, leader_start + int(self.sr * 0.5))
        
        if analysis_len - analysis_start < window_size:
            return 0.0
        
        frequencies = []
        
        for win_start in range(analysis_start, analysis_len - window_size, hop_size):
            segment = self.samples[win_start:win_start + window_size]
            
            # Zero-crossing counting
            peak_in_win = float(np.max(np.abs(segment)))
            if peak_in_win < 0.01:
                continue
            
            noise_gate = peak_in_win * 0.1
            zc = 0
            last_above = segment[0] > noise_gate
            for s in segment[1:]:
                above = s > noise_gate
                below = s < -noise_gate
                if above and not last_above:
                    zc += 1
                    last_above = True
                elif below and last_above:
                    zc += 1
                    last_above = False
            
            if zc > 0:
                freq = zc * self.sr / (2.0 * window_size)
                frequencies.append(freq)
        
        if len(frequencies) < 3:
            return 0.0
        
        # Jitter = (std / mean) * 100
        mean_freq = float(np.mean(frequencies))
        if mean_freq < 1.0:
            return 0.0
        
        std_freq = float(np.std(frequencies))
        jitter_pct = (std_freq / mean_freq) * 100.0
        
        return min(15.0, jitter_pct)
    
    # ─── Metric 3: Effective Duration ──────────────────────────────────
    
    def measure_effective_duration(self):
        """
        Measure effective signal duration from leader end to last valid data.
        
        Method:
        - Find leader end (where signal envelope drops below 10% peak)
        - Find data end (last position where envelope > 10% peak after leader)
        - Duration = data_end - leader_end
        
        Returns: effective duration in seconds
        """
        # Envelope: RMS in 50ms windows
        window_size = int(self.sr * 0.05)
        hop_size = window_size // 4
        num_windows = max(1, (self.n - window_size) // hop_size)
        
        if num_windows < 10:
            return self.duration_s
        
        envelope = []
        win_times = []
        
        for wi in range(num_windows):
            ws = wi * hop_size
            we = min(self.n, ws + window_size)
            seg = self.samples[ws:we]
            rms = float(np.sqrt(np.mean(seg ** 2)))
            envelope.append(rms)
            win_times.append(ws / self.sr)
        
        envelope = np.array(envelope)
        
        # Find threshold (10% of max envelope)
        max_env = float(np.max(envelope))
        if max_env < 0.001:
            return 0.0
        
        threshold = max_env * 0.10
        
        # Find first window above threshold (leader start)
        above = envelope > threshold
        if not np.any(above):
            return 0.0
        
        first_above = int(np.argmax(above))
        
        # Find last window above threshold (data end)
        last_above = int(np.where(above)[0][-1])
        
        if last_above <= first_above:
            return 0.0
        
        # Effective duration
        effective = win_times[last_above] - win_times[first_above]
        return max(0.0, effective)
    
    # ─── Metric 4: DC Bias ─────────────────────────────────────────────
    
    def measure_dc_bias(self):
        """
        Measure residual DC bias after HPF.
        
        Returns: absolute value of mean (ideally near 0)
        """
        mean_val = float(np.mean(self.samples))
        return abs(mean_val)
    
    # ─── Metric 5: Dropout Zones ───────────────────────────────────────
    
    def measure_dropouts(self, baud=1200):
        """
        Measure percentage of signal lost to dropouts (amplitude < 5% of peak).
        
        Method:
        - Find data zone (after leader tone)
        - Count samples with |amplitude| < 5% of global peak
        - Return percentage
        
        Returns: dropout percentage (0.0-30.0)
        """
        # Find leader end
        leader_len = min(self.n, int(self.sr * 3.0))
        
        # Find onset
        global_peak = float(np.max(np.abs(self.samples)))
        if global_peak < 0.001:
            return 0.0
        
        onset_threshold = global_peak * 0.05
        leader_start = 0
        step = int(self.sr * 0.1)
        while leader_start < leader_len:
            chunk_end = min(leader_len, leader_start + step)
            local_peak = float(np.max(np.abs(self.samples[leader_start:chunk_end])))
            if local_peak > onset_threshold:
                break
            leader_start += step
        
        # Data zone: from leader end to end of signal
        # Leader is ~0.5s, data zone starts ~0.5s after onset
        data_start = min(self.n, leader_start + int(self.sr * 0.6))
        data_zone = self.samples[data_start:]
        
        if len(data_zone) < int(self.sr * 0.5):
            return 0.0
        
        peak_in_data = float(np.max(np.abs(data_zone)))
        if peak_in_data < 0.001:
            return 100.0
        
        dropout_threshold = peak_in_data * 0.05
        dropout_samples = int(np.sum(np.abs(data_zone) < dropout_threshold))
        dropout_pct = dropout_samples * 100.0 / len(data_zone)
        
        # No clamp here — normalization in compute_quality_score handles capping
        return dropout_pct
    
    # ─── Metric 6: Occupied Bandwidth ──────────────────────────────────
    
    def measure_bandwidth(self):
        """
        Measure occupied bandwidth from spectrogram.
        
        Method:
        - Compute spectrogram on the full signal
        - Find the 95th percentile frequency (above which only 5% energy exists)
        
        Returns: frequency in Hz, or 0.0 if signal too short
        """
        if self.n < int(self.sr * 0.5):
            return 0.0
        
        # Use a moderate FFT size for frequency resolution
        nperseg = min(1024, self.n // 4)
        if nperseg < 64:
            return 0.0
        
        f, t_spec, Sxx = scipy_signal.spectrogram(
            self.samples, self.sr,
            nperseg=nperseg, noverlap=nperseg // 2,
            window=("hamming",), scaling="density",
        )
        
        # Focus on FSK band (800-5000 Hz)
        freq_mask = (f >= 800) & (f <= 5000)
        Sxx_band = Sxx[freq_mask]
        f_band = f[freq_mask]
        
        if Sxx_band.size == 0:
            return 0.0
        
        # Total energy in band
        total_energy = float(np.sum(Sxx_band))
        
        if total_energy < 1e-12:
            return 0.0
        
        # Cumulative energy distribution
        cumulative = np.cumsum(np.sum(Sxx_band, axis=1))
        cumulative_norm = cumulative / cumulative[-1]
        
        # Find frequency at 95th percentile
        idx_95 = int(np.argmax(cumulative_norm >= 0.95))
        bandwidth_hz = float(f_band[idx_95])
        
        return bandwidth_hz
    
    # ─── Composite Quality Score ───────────────────────────────────────
    
    def compute_quality_score(self, metrics):
        """
        Compute composite quality score from all metrics.
        
        Weights:
        - SNR: 0.35 (most important — determines fundamental decodability)
        - Jitter: 0.30 (determines if brute-force sweep is needed)
        - Dropouts: 0.20 (indicates data loss)
        - Duration: 0.15 (short tapes have less data to work with)
        
        Args:
            metrics: dict with snr_db, jitter_pct, duration_s, dropout_pct
        
        Returns:
            score: 0.0-1.0
            label: "GOOD", "FAIR", "POOR", "DEGRADED"
        """
        # Normalize each metric to 0.0-1.0
        snr_norm = max(0.0, min(1.0, metrics.get("snr_db", 0.0) / 40.0))
        jitter_norm = max(0.0, min(1.0, metrics.get("jitter_pct", 15.0) / 15.0))
        dropout_norm = max(0.0, min(1.0, metrics.get("dropout_pct", 30.0) / 30.0))
        duration_norm = max(0.0, min(1.0, metrics.get("duration_s", 0.0) / 30.0))
        
        # Weights
        w_snr = 0.35
        w_jitter = 0.30
        w_dropout = 0.20
        w_duration = 0.15
        
        score = (w_snr * snr_norm 
                 + w_jitter * (1.0 - jitter_norm) 
                 + w_dropout * (1.0 - dropout_norm) 
                 + w_duration * duration_norm)
        
        # Determine label
        if score >= 0.75:
            label = "GOOD"
            emoji = "\U0001F7E2"  # 🟢
        elif score >= 0.50:
            label = "FAIR"
            emoji = "\U0001F7E1"  # 🟡
        elif score >= 0.25:
            label = "POOR"
            emoji = "\U0001F7E0"  # 🟠
        else:
            label = "DEGRADED"
            emoji = "\U0001F534"  # 🔴
        
        return {
            "score": round(score, 3),
            "label": label,
            "emoji": emoji,
            "snr_norm": round(snr_norm, 3),
            "jitter_norm": round(jitter_norm, 3),
            "dropout_norm": round(dropout_norm, 3),
            "duration_norm": round(duration_norm, 3),
        }
    
    # ─── Main Analysis ────────────────────────────────────────────────
    
    def analyze(self, baud=0, verbose=False):
        """
        Run full analysis pipeline.
        
        Args:
            baud: 0=auto-detect, 340=Juno-60, 1200=Juno-106
            verbose: Print progress to stderr
        
        Returns:
            dict with all metrics, quality score, and format detection
        """
        start_time = time.time()
        
        if verbose:
            print(f"[TapeAnalyzer] Analyzing {self.n} samples @ {self.sr:.0f} Hz...", file=sys.stderr)
        
        # Step 1: Detect format
        if baud == 0:
            detected_baud = self.detect_format()
            if detected_baud == 0:
                detected_baud = 1200  # Default to Juno-106
            baud = detected_baud
        
        if verbose:
            fmt_name = "Juno-60" if baud == 340 else "Juno-106"
            print(f"[TapeAnalyzer] Detected format: {fmt_name} ({baud} baud)", file=sys.stderr)
        
        # Step 2: Extract metrics
        metrics = {}
        
        t0 = time.time()
        metrics["snr_db"] = self.estimate_snr(baud)
        if verbose:
            print(f"[TapeAnalyzer]   SNR: {metrics['snr_db']:.1f} dB ({time.time()-t0:.1f}s)", file=sys.stderr)
        
        t0 = time.time()
        metrics["jitter_pct"] = self.measure_jitter(baud)
        if verbose:
            print(f"[TapeAnalyzer]   Jitter: {metrics['jitter_pct']:.1f}% ({time.time()-t0:.1f}s)", file=sys.stderr)
        
        t0 = time.time()
        metrics["duration_s"] = self.measure_effective_duration()
        if verbose:
            print(f"[TapeAnalyzer]   Duration: {metrics['duration_s']:.1f}s ({time.time()-t0:.1f}s)", file=sys.stderr)
        
        t0 = time.time()
        metrics["dc_bias"] = self.measure_dc_bias()
        if verbose:
            print(f"[TapeAnalyzer]   DC Bias: {metrics['dc_bias']:.5f} ({time.time()-t0:.1f}s)", file=sys.stderr)
        
        t0 = time.time()
        metrics["dropout_pct"] = self.measure_dropouts(baud)
        if verbose:
            print(f"[TapeAnalyzer]   Dropouts: {metrics['dropout_pct']:.1f}% ({time.time()-t0:.1f}s)", file=sys.stderr)
        
        t0 = time.time()
        metrics["bandwidth_hz"] = self.measure_bandwidth()
        if verbose:
            print(f"[TapeAnalyzer]   Bandwidth: {metrics['bandwidth_hz']:.0f} Hz ({time.time()-t0:.1f}s)", file=sys.stderr)
        
        # Step 3: Quality score
        quality = self.compute_quality_score(metrics)
        metrics["quality_score"] = quality["score"]
        metrics["quality_label"] = quality["label"]
        metrics["quality_emoji"] = quality["emoji"]
        metrics["detected_baud"] = baud
        
        # Per-metric quality labels
        metrics["snr_quality"] = "GOOD" if metrics["snr_db"] > 30 else ("FAIR" if metrics["snr_db"] > 15 else ("POOR" if metrics["snr_db"] > 8 else "DEGRADED"))
        metrics["jitter_quality"] = "GOOD" if metrics["jitter_pct"] < 3 else ("FAIR" if metrics["jitter_pct"] < 8 else ("POOR" if metrics["jitter_pct"] < 12 else "DEGRADED"))
        metrics["dropout_quality"] = "GOOD" if metrics["dropout_pct"] < 5 else ("FAIR" if metrics["dropout_pct"] < 15 else ("POOR" if metrics["dropout_pct"] < 25 else "DEGRADED"))
        
        elapsed = time.time() - start_time
        metrics["analysis_time_s"] = round(elapsed, 2)
        
        if verbose:
            print(f"[TapeAnalyzer] Quality: {quality['emoji']} {quality['label']} (score={quality['score']:.3f})", file=sys.stderr)
            print(f"[TapeAnalyzer] Total: {elapsed:.1f}s", file=sys.stderr)
        
        return metrics
    
    # ─── Strategy Recommendation ────────────────────────────────────────
    
    def recommend_strategy(self, metrics):
        """
        Recommend decoding strategy based on quality metrics.
        
        Returns:
            dict with strategy name, decoders to run, auto_select flag
        """
        score = metrics.get("quality_score", 0.0)
        duration = metrics.get("duration_s", 0.0)
        snr = metrics.get("snr_db", 0.0)
        
        # Short tape override
        is_short = duration < 10.0
        
        if score >= 0.75 and not is_short:
            return {
                "strategy": "GOOD",
                "decoders": ["goertzel_fast"],
                "auto_select": True,
                "description": "Maxima velocidad — Goertzel single pass",
            }
        elif score >= 0.50 or is_short:
            return {
                "strategy": "FAIR",
                "decoders": ["goertzel_bf"],
                "auto_select": True,
                "description": "Goertzel brute-force (145 combos)",
            }
        elif score >= 0.25:
            return {
                "strategy": "POOR",
                "decoders": ["goertzel_bf", "goertzel_fast"],
                "auto_select": False,
                "description": "Multiples decodificadores - presentar resultados",
            }
        else:
            return {
                "strategy": "DEGRADED",
                "decoders": ["goertzel_bf", "goertzel_fast"],
                "auto_select": False,
                "description": "Todos los decodificadores - presentar mejores resultados",
            }
    
    # ─── Reporting ─────────────────────────────────────────────────────
    
    def format_report(self, metrics):
        """
        Format metrics as a human-readable report string.
        """
        quality = metrics["quality_emoji"]
        label = metrics["quality_label"]
        score = metrics["quality_score"]
        fmt_name = "Juno-60" if metrics["detected_baud"] == 340 else "Juno-106"
        baud = metrics["detected_baud"]
        
        lines = [
            f"  Formato: {fmt_name} ({baud} baud)",
            f"  Calidad: {quality} {label} (score: {score:.3f})",
            f"  SNR:     {metrics['snr_db']:.1f} dB ({metrics['snr_quality']})",
            f"  Jitter:  {metrics['jitter_pct']:.1f}% ({metrics['jitter_quality']})",
            f"  Duracion: {metrics['duration_s']:.1f}s",
            f"  DC Bias: {metrics['dc_bias']:.5f}",
            f"  Dropout: {metrics['dropout_pct']:.1f}% ({metrics['dropout_quality']})",
            f"  BW:      {metrics['bandwidth_hz']:.0f} Hz",
            f"  Tiempo:  {metrics['analysis_time_s']:.1f}s",
        ]
        return '\n'.join(lines)


# ─── Main ──────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Analyze signal quality of FSK cassette tape dumps")
    parser.add_argument("wav_file", help="Path to WAV file")
    parser.add_argument("--baud", type=int, default=0, choices=[0, 340, 1200],
                        help="Force baud rate (0=auto-detect)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show detailed progress")
    parser.add_argument("--json", action="store_true",
                        help="Output as JSON")
    args = parser.parse_args()
    
    if not os.path.isfile(args.wav_file):
        print(f"Error: File not found: {args.wav_file}")
        sys.exit(1)
    
    # Load and preprocess
    print(f"Cargando: {args.wav_file}")
    samples_raw, sr, nch = vis.load_wav(args.wav_file)
    print(f"  SR: {sr} Hz, Canales: {nch}, Duracion: {len(samples_raw)/sr:.1f}s")
    
    samples, sr = vis.preprocess(samples_raw, sr, nch)
    
    # Analyze
    analyzer = TapeAnalyzer(samples, sr)
    metrics = analyzer.analyze(baud=args.baud, verbose=True)
    
    if args.json:
        import json
        print(json.dumps(metrics, indent=2))
    else:
        print()
        print("─── Resultados ───")
        print(analyzer.format_report(metrics))
        
        # Strategy recommendation
        strategy = analyzer.recommend_strategy(metrics)
        print()
        print(f"─── Estrategia Recomendada ───")
        print(f"  {strategy['description']}")
        print(f"  Auto-select: {'Si' if strategy['auto_select'] else 'No — mostrar al usuario'}")
        print(f"  Decodificadores: {', '.join(strategy['decoders'])}")


if __name__ == '__main__':
    main()
