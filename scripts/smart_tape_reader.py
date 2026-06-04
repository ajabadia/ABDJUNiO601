#!/usr/bin/env python3
"""
SmartTapeReader — Intelligent Cassette Tape Decoding Orchestrator
=================================================================
Full pipeline: analyze → select strategy → decode → rank → decide.

Phases:
1. Signal analysis (via TapeAnalyzer) — 6 metrics + quality score
2. Strategy selection — choose decoders based on quality
3. Multiple decoding — run selected decoders in parallel
4. Result ranking — compare and rank decoder outputs
5. Decision — auto-select best or present top 3

Usage:
    python scripts/smart_tape_reader.py <wav_file> [options]
    
Options:
    --baud 340|1200       Force baud rate
    --verbose             Show detailed progress log (like UI would)
    --json                Output as JSON
    --compare-all         Run all decoders regardless of quality score

Output:
    Shows a step-by-step log of the process, then the final result.
"""

import sys, os, math, time, argparse

sys.path.insert(0, 'scripts')
import importlib.util

# Load TapeAnalyzer
spec = importlib.util.spec_from_file_location('ta', 'scripts/tape_analyzer.py')
ta = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ta)

# Load visualize_tape for decoders
spec2 = importlib.util.spec_from_file_location('vis', 'scripts/visualize_tape.py')
vis = importlib.util.module_from_spec(spec2)
spec2.loader.exec_module(vis)

# Load DCB corrector from test_dcb_corrector (replicates C++ correctDcbFormat)
spec3 = importlib.util.spec_from_file_location('dcbcorr', 'scripts/test_dcb_corrector.py')
dcbcorr = importlib.util.module_from_spec(spec3)
spec3.loader.exec_module(dcbcorr)


class SmartTapeReader:
    """
    Full SmartTapeReader orchestrator.
    
    Usage:
        reader = SmartTapeReader(wav_path)
        result = reader.run(verbose=True)
        # result = {
        #     "file": "JUNO-60 Bank A.wav",
        #     "format": "Juno-106 (1200 baud)",
        #     "quality": { ... metrics ... },
        #     "strategy": { ... chosen strategy ... },
        #     "decoder_results": [ ... per-decoder results ... ],
        #     "ranking": [ ... ranked results ... ],
        #     "decision": {
        #         "auto_selected": True/False,
        #         "winner": { ... best result ... },
        #         "alternatives": [ ... other top results ... ],
        #     }
        # }
    """
    
    def __init__(self, wav_path, baud=0):
        self.wav_path = wav_path
        self.file_label = os.path.basename(wav_path)
        self.forced_baud = baud
        
        # Load and preprocess
        self.samples_raw = None
        self.sr_raw = 0
        self.nch = 0
        self.samples = None
        self.sr = 0
        self.analyzer = None
        
        self._load_audio()
    
    def _log(self, msg, verbose=True):
        """Log a step (as the UI would show in real-time)."""
        if verbose:
            print(f"  {msg}", file=sys.stderr)
    
    def _load_audio(self):
        """Load and preprocess the WAV file."""
        self._log(f"[{self.file_label}] Cargando...")
        self.samples_raw, self.sr_raw, self.nch = vis.load_wav(self.wav_path)
        duration = len(self.samples_raw) / self.sr_raw
        self._log(f"  SR: {self.sr_raw} Hz, Canales: {self.nch}, Duracion: {duration:.1f}s")
        
        self._log(f"  Preprocesando: {'estéreo→mono' if self.nch > 1 else 'mono'}, upsampling, HPF, normalizar...")
        self.samples, self.sr = vis.preprocess(self.samples_raw, self.sr_raw, self.nch)
        
        self.analyzer = ta.TapeAnalyzer(self.samples, self.sr)
    
    # ─── Phase 1: Signal Analysis ──────────────────────────────────────
    
    def analyze_signal(self, verbose=True):
        """
        Phase 1: Analyze signal quality.
        
        Returns:
            dict with all metrics + quality score
        """
        self._log("")
        if verbose:
            self._log("=== Fase 1: Analizando calidad de senal... ===")
        
        t0 = time.time()
        metrics = self.analyzer.analyze(baud=self.forced_baud, verbose=verbose)
        
        # Per-metric quality lines (as UI would show)
        if verbose:
            self._log("")
            self._log("  Resultados por metrica:")
            
            snr_q = metrics.get("snr_quality", "?")
            snr = metrics.get("snr_db", 0)
            tag_snr = "[OK]" if snr_q == "GOOD" else ("[-]" if snr_q == "FAIR" else "[!!]")
            self._log(f"    {tag_snr} SNR:      {snr:.1f} dB ({snr_q})")
            
            j_q = metrics.get("jitter_quality", "?")
            j = metrics.get("jitter_pct", 0)
            tag_j = "[OK]" if j_q == "GOOD" else ("[-]" if j_q == "FAIR" else "[!!]")
            self._log(f"    {tag_j} Jitter:   {j:.1f}% ({j_q})")
            
            d_q = metrics.get("dropout_quality", "?")
            d = metrics.get("dropout_pct", 0)
            tag_d = "[OK]" if d_q == "GOOD" else ("[-]" if d_q == "FAIR" else "[!!]")
            self._log(f"    {tag_d} Dropouts: {d:.1f}% ({d_q})")
            
            dur = metrics.get("duration_s", 0)
            dur_q = "GOOD" if dur >= 10 else "SHORT"
            tag_dur = "[OK]" if dur_q == "GOOD" else "[-]"
            self._log(f"    {tag_dur} Duracion: {dur:.1f}s")
            
            bw = metrics.get("bandwidth_hz", 0)
            self._log(f"    BW:      {bw:.0f} Hz")
            
            dc = metrics.get("dc_bias", 0)
            self._log(f"    DC Bias: {dc:.5f}")
        
        elapsed = time.time() - t0
        metrics["analysis_time_s"] = round(elapsed, 2)
        
        return metrics
    
    # ─── Phase 2: Strategy Selection ──────────────────────────────────
    
    def select_strategy(self, metrics, compare_all=False, verbose=True):
        """
        Phase 2: Select decoding strategy based on quality.
        
        Args:
            metrics: dict from analyze_signal()
            compare_all: if True, run all decoders regardless of quality
        
        Returns:
            dict with strategy name, decoders list, auto_select flag
        """
        strategy = self.analyzer.recommend_strategy(metrics)
        
        if compare_all:
            # Override to run all decoders
            strategy["decoders"] = ["goertzel_bf", "goertzel_fast"]
            strategy["strategy"] = "COMPARE_ALL"
            strategy["auto_select"] = False
            strategy["description"] = "Comparando Goertzel BF vs Fast"
        
        if verbose:
            quality_label = metrics.get("quality_label", "?")
            score = metrics.get("quality_score", 0)
            self._log("")
            self._log("=== Fase 2: Estrategia seleccionada ===")
            self._log(f"  Calidad general: {quality_label} (score: {score:.3f})")
            self._log(f"  Estrategia: {strategy['description']}")
            self._log(f"  Decodificadores: {', '.join(strategy['decoders'])}")
            if strategy["auto_select"]:
                self._log("  Auto-select: SI - resultado unico")
            else:
                self._log("  Auto-select: NO - se mostraran multiples resultados")
        
        return strategy
    
    # ─── Phase 3: Decoding ────────────────────────────────────────────
    
    def _decode_goertzel_bf(self, baud, verbose=True):
        """Goertzel brute-force (145 combos). Uses Python decoder."""
        self._log(f"  [Goertzel BF] Decodificando {baud} baud (145 combos)...")
        t0 = time.time()
        result = vis.decode_fsk(self.samples, self.sr, baud, fast=False)
        elapsed = time.time() - t0
        return {
            "decoder": "goertzel_bf",
            "label": "Goertzel BF (145)",
            "patches": result.get("num_patches", 0),
            "bytes_raw": len(result.get("bytes", [])),
            "validated": bytes(result.get("validated", [])),
            "bits": len(result.get("bits", [])),
            "elapsed_s": round(elapsed, 2),
            "speed_factor": result.get("speed_factor", 0),
            "phase_offset": result.get("phase_offset", 0),
        }
    
    def _decode_goertzel_fast(self, baud, verbose=True):
        """Goertzel single pass (speed=1.0, phase=0)."""
        self._log(f"  [Goertzel Fast] Decodificando {baud} baud (single pass)...")
        t0 = time.time()
        result = vis.decode_fsk(self.samples, self.sr, baud, fast=True)
        elapsed = time.time() - t0
        return {
            "decoder": "goertzel_fast",
            "label": "Goertzel Fast (1)",
            "patches": result.get("num_patches", 0),
            "bytes_raw": len(result.get("bytes", [])),
            "validated": bytes(result.get("validated", [])),
            "bits": len(result.get("bits", [])),
            "elapsed_s": round(elapsed, 2),
            "speed_factor": result.get("speed_factor", 0),
            "phase_offset": result.get("phase_offset", 0),
        }
    
    def decode_all(self, strategy, verbose=True):
        """
        Phase 3: Run selected decoders.
        
        Args:
            strategy: dict from select_strategy()
        
        Returns:
            list of decoder result dicts
        """
        self._log("")
        self._log("=== Fase 3: Decodificando... ===")
        
        baud = self.analyzer.detect_format()
        if baud == 0:
            baud = 1200
        
        decoder_map = {
            "goertzel_bf": self._decode_goertzel_bf,
            "goertzel_fast": self._decode_goertzel_fast,
        }
        
        results = []
        for decoder_name in strategy["decoders"]:
            if decoder_name in decoder_map:
                result = decoder_map[decoder_name](baud, verbose)
                
                # Apply DCB corrector (reuses Python replica of C++ correctDcbFormat)
                if result["validated"] and len(result["validated"]) >= 18:
                    # Build list of 18-byte patches for the corrector
                    raw_validated = result["validated"]
                    patches_list = [bytes(raw_validated[i:i+18]) for i in range(0, len(raw_validated), 18)]
                    corrected_list = dcbcorr.correct_dcb_python(patches_list, baud)
                    result["validated_corrected"] = b"".join(corrected_list)
                    result["patches_corrected"] = len(corrected_list)
                else:
                    result["validated_corrected"] = b""
                    result["patches_corrected"] = 0
                
                results.append(result)
                
                if verbose:
                    p = result["patches_corrected"]
                    b = result["bytes_raw"]
                    t = result["elapsed_s"]
                    self._log(f"    -> {result['label']}: {p} patches, {b} bytes raw ({t:.1f}s)")
        
        return results
    
    # ─── Phase 4: Ranking ─────────────────────────────────────────────
    
    def _checksum_hit_rate(self, validated_bytes, raw_byte_count):
        """
        Estimate checksum hit rate from validated patches vs raw bytes.
        Each patch needs 19 bytes (18 data + 1 checksum) in raw stream.
        """
        if raw_byte_count < 19:
            return 0.0
        max_patches_from_raw = raw_byte_count // 19
        if max_patches_from_raw == 0:
            return 0.0
        n_valid = len(validated_bytes) // 18 if validated_bytes else 0
        return n_valid / max_patches_from_raw
    
    def _count_duplicates(self, validated_bytes):
        """Count duplicate patches (identical 18-byte sequences)."""
        if not validated_bytes or len(validated_bytes) < 18:
            return 0
        patches_seen = set()
        duplicates = 0
        for i in range(0, len(validated_bytes), 18):
            patch = validated_bytes[i:i+18]
            if patch in patches_seen:
                duplicates += 1
            else:
                patches_seen.add(patch)
        return duplicates
    
    def _byte_consistency(self, results):
        """
        Measure byte consistency between decoder results.
        Higher value = more agreement between decoders = more confidence.
        """
        if len(results) < 2:
            return 0.0
        
        # Compare all pairs of results
        total_matches = 0
        total_comparisons = 0
        
        for i in range(len(results)):
            for j in range(i + 1, len(results)):
                a = results[i].get("validated_corrected", b"")
                b = results[j].get("validated_corrected", b"")
                
                if not a or not b:
                    continue
                
                a_patches = [a[k:k+18] for k in range(0, len(a), 18)]
                b_patches = [b[k:k+18] for k in range(0, len(b), 18)]
                
                a_set = set(a_patches)
                b_set = set(b_patches)
                common = a_set & b_set
                
                total_matches += len(common)
                total_comparisons += 1
        
        if total_comparisons == 0:
            return 0.0
        
        return total_matches / total_comparisons
    
    def rank_results(self, decoder_results, verbose=True):
        """
        Phase 4: Rank decoder results by quality.
        
        Rank formula:
            rank = n_valid_patches * 1000
                 + checksum_hit_rate * 100
                 + byte_consistency_across_decoders * 10
                 - duplicate_penalty
        
        Returns:
            sorted list of (rank, result) tuples
        """
        self._log("")
        self._log("=== Fase 4: Ranking resultados... ===")
        
        # Compute byte consistency across all decoders first
        consistency = self._byte_consistency(decoder_results)
        
        ranked = []
        for r in decoder_results:
            n_valid = r.get("patches_corrected", 0)
            raw_bytes = r.get("bytes_raw", 0)
            validated = r.get("validated_corrected", b"")
            
            checksum_rate = self._checksum_hit_rate(validated, raw_bytes)
            duplicates = self._count_duplicates(validated)
            
            # Rank formula
            rank = (n_valid * 1000 
                    + checksum_rate * 100 
                    + consistency * 10 
                    - duplicates * 500)  # Heavily penalize duplicates
            
            ranked.append({
                "decoder": r["decoder"],
                "label": r["label"],
                "n_patches": n_valid,
                "bytes_raw": raw_bytes,
                "checksum_rate": round(checksum_rate, 3),
                "duplicates": duplicates,
                "consistency": round(consistency, 2),
                "rank": round(rank, 1),
                "elapsed_s": r.get("elapsed_s", 0),
            })
        
        # Sort by rank descending
        ranked.sort(key=lambda x: x["rank"], reverse=True)
        
        if verbose:
            self._log("")
            self._log(f"  {'Decoder':25s}  {'Patches':>8}  {'Checksum':>9}  {'Dup':>4}  {'Rank':>8}")
            self._log(f"  {'-'*25}  {'-'*8}  {'-'*9}  {'-'*4}  {'-'*8}")
            for r in ranked:
                self._log(f"  {r['label']:25s}  {r['n_patches']:>8d}  {r['checksum_rate']:>8.1%}  {r['duplicates']:>4d}  {r['rank']:>8.1f}")
        
        return ranked, consistency
    
    # ─── Phase 5: Decision ────────────────────────────────────────────
    
    def decide(self, ranked_results, strategy, consistency=0, verbose=True):
        """
        Phase 5: Make final decision.
        
        If auto-select OR #1 rank > #2 by 20% → auto-select.
        Otherwise → present top 3.
        """
        self._log("")
        self._log("=== Fase 5: Decision final ===")
        
        if not ranked_results:
            self._log("  No hay resultados de decodificacion.")
            return {"auto_selected": False, "winner": None, "alternatives": []}
        
        winner = ranked_results[0]
        alternatives = ranked_results[1:4] if len(ranked_results) > 1 else []
        
        # Decision logic
        auto_select = strategy["auto_select"]
        
        if not auto_select and len(ranked_results) >= 2:
            # Check if #1 is clearly better than #2
            rank_ratio = ranked_results[0]["rank"] / max(ranked_results[1]["rank"], 1)
            if rank_ratio >= 1.20:
                auto_select = True
        
        # Final result
        decision = {
            "auto_selected": auto_select,
            "winner": winner,
            "alternatives": alternatives,
        }
        
        if verbose:
            tag = "[AUTO]" if auto_select else "[OPCIONES]"
            verb = "Auto-seleccionado" if auto_select else "Presentando opciones"
            self._log(f"  {tag} {verb}: {winner['label']} ({winner['n_patches']} patches, rank={winner['rank']:.0f})")
            if not auto_select and alternatives:
                self._log("  Alternativas:")
                for alt in alternatives:
                    self._log(f"    - {alt['label']}: {alt['n_patches']} patches (rank={alt['rank']:.0f})")
        
        return decision
    
    # ─── Full Pipeline ────────────────────────────────────────────────
    
    def run(self, verbose=True, compare_all=False, show_patch_hex=False):
        """
        Run the full SmartTapeReader pipeline.
        
        Args:
            verbose: Show step-by-step log
            compare_all: Run all decoders (overrides strategy)
            show_patch_hex: Show hex dump of corrected patches
        
        Returns:
            dict with full results
        """
        self._log("")
        self._log(f"[SmartTapeReader] {self.file_label}")
        self._log("=" * 50)
        
        # Phase 1
        metrics = self.analyze_signal(verbose)
        
        # Phase 2
        strategy = self.select_strategy(metrics, compare_all, verbose)
        
        # Phase 3
        decoder_results = self.decode_all(strategy, verbose)
        
        # Phase 4
        ranked_results, consistency = self.rank_results(decoder_results, verbose)
        
        # Phase 5
        decision = self.decide(ranked_results, strategy, consistency, verbose)
        
        # Verify: validate DCB structure on winner patches
        winner_patches = []
        if decision["winner"]:
            for r in decoder_results:
                if r["decoder"] == decision["winner"]["decoder"]:
                    corrected = r.get("validated_corrected", b"")
                    if corrected:
                        for pi in range(len(corrected) // 18):
                            off = pi * 18
                            winner_patches.append(bytes(corrected[off:off+18]))
                    break
        
        # Build final result
        result = {
            "file": self.file_label,
            "format": {
                "baud": metrics.get("detected_baud", 0),
                "name": "Juno-60" if metrics.get("detected_baud") == 340 else "Juno-106",
            },
            "quality": {
                "score": metrics.get("quality_score", 0),
                "label": metrics.get("quality_label", ""),
                "snr_db": metrics.get("snr_db", 0),
                "jitter_pct": metrics.get("jitter_pct", 0),
                "duration_s": metrics.get("duration_s", 0),
                "dropout_pct": metrics.get("dropout_pct", 0),
                "bandwidth_hz": metrics.get("bandwidth_hz", 0),
                "dc_bias": metrics.get("dc_bias", 0),
                "snr_quality": metrics.get("snr_quality", ""),
                "jitter_quality": metrics.get("jitter_quality", ""),
                "dropout_quality": metrics.get("dropout_quality", ""),
            },
            "strategy": strategy,
            "decoder_results": [
                {
                    "decoder": r["decoder"],
                    "label": r["label"],
                    "patches": r.get("patches_corrected", 0),
                    "bytes_raw": r.get("bytes_raw", 0),
                    "elapsed_s": r.get("elapsed_s", 0),
                }
                for r in decoder_results
            ],
            "ranking": ranked_results,
            "decision": {
                "auto_selected": decision["auto_selected"],
                "winner_decoder": decision["winner"]["decoder"] if decision["winner"] else None,
                "winner_patches": decision["winner"]["n_patches"] if decision["winner"] else 0,
                "alternatives": [
                    {"decoder": a["decoder"], "patches": a["n_patches"]}
                    for a in decision["alternatives"]
                ],
            },
            "winner_patches_hex": [],
        }
        
        # Show hex of winner patches if requested
        if show_patch_hex and winner_patches:
            self._log("")
            self._log(f"Patches del ganador ({len(winner_patches)}):")
            for pi, pdata in enumerate(winner_patches):
                hex_str = " ".join(f"{b:02X}" for b in pdata)
                self._log(f"  P{pi+1:2d}: {hex_str}")
                result["winner_patches_hex"].append(hex_str)
        
        self._log("")
        self._log("=" * 50)
        self._log(f"[COMPLETADO] {metrics.get('analysis_time_s', 0):.1f}s")
        
        return result


# ─── Main ──────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="SmartTapeReader — Intelligent cassette tape decoding")
    parser.add_argument("wav_file", help="Path to WAV file")
    parser.add_argument("--baud", type=int, default=0, choices=[0, 340, 1200],
                        help="Force baud rate")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show detailed step-by-step log")
    parser.add_argument("--compare-all", action="store_true",
                        help="Run all decoders regardless of quality")
    parser.add_argument("--show-hex", action="store_true",
                        help="Show hex dumps of winner patches")
    parser.add_argument("--json", action="store_true",
                        help="Output as JSON")
    args = parser.parse_args()
    
    if not os.path.isfile(args.wav_file):
        print(f"Error: File not found: {args.wav_file}")
        sys.exit(1)
    
    reader = SmartTapeReader(args.wav_file, baud=args.baud)
    result = reader.run(
        verbose=args.verbose,
        compare_all=args.compare_all,
        show_patch_hex=args.show_hex,
    )
    
    if args.json:
        import json
        print(json.dumps(result, indent=2, default=str))
    elif not args.verbose:
        # Minimal output
        q = result["quality"]
        d = result["decision"]
        w = d.get("winner_decoder", "N/A")
        p = d.get("winner_patches", 0)
        print(f"  Calidad: {q['label']} (score={q['score']:.3f})")
        print(f"  Ganador: {w} — {p} patches")
        if d["alternatives"]:
            print(f"  Alternativas: {len(d['alternatives'])}")
        if not d["auto_selected"]:
            print("  (multiple resultados disponibles)")


if __name__ == '__main__':
    main()
