#!/usr/bin/env python3
"""
Dump all decoded patch hex bytes from all 8 tapes using the Python pipeline
(visualize_tape.py with np.interp upsampler).
Outputs JSON to stdout for machine comparison.

Usage:
    python scripts/dump_tape_hex.py > _python_hex.json
"""

import sys
import os
import json
import importlib.util

sys.path.insert(0, 'scripts')
spec = importlib.util.spec_from_file_location('vis', 'scripts/visualize_tape.py')
vis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vis)

TAPES = [
    ('docs/Juno-60 (1)/JUNO-60 Bank A.wav',         0, 'JUNO-60 Bank A'),
    ('docs/Juno-60 (1)/JUNO-60 Bank B.wav',         0, 'JUNO-60 Bank B'),
    ('docs/JUNO-106/JUNO106 Bank A.wav',            0, 'JUNO-106 Bank A'),
    ('docs/JUNO-106/JUNO106 Bank B.wav',            0, 'JUNO-106 Bank B'),
    ('docs/JUNO-106/Roland Juno-60 factory programs group 1.wav', 0, 'Juno-60 G1'),
    ('docs/JUNO-106/Roland Juno-60 factory programs group 2.wav', 0, 'Juno-60 G2'),
    ('JUNO106/original/tapes/roland_juno106_factory/j106ma.wav',  0, 'j106ma'),
    ('JUNO106/original/tapes/roland_juno106_factory/j106mb.wav',  0, 'j106mb'),
]

results = {}

for path, forced_baud, label in TAPES:
    if not os.path.isfile(path):
        print(f"WARNING: {label}: file not found", file=sys.stderr)
        continue

    try:
        r, sr, ch = vis.load_wav(path)
        r2, sr = vis.preprocess(r, sr, ch)

        if forced_baud:
            baud = forced_baud
        else:
            baud = vis.detect_format(r2, sr)
            if baud not in [340, 1200]:
                baud = 1200

        result = vis.decode_fsk(r2, sr, baud, fast=True)

        if result and result['validated']:
            patches = []
            for pi in range(result['num_patches']):
                off = pi * 18
                data = list(result['validated'][off:off+18])
                patches.append(data)
            
            entry = {
                "baud": baud,
                "sr": sr,
                "num_patches": len(patches),
                "upsampler": "np.interp (linear interpolation)",
                "patches": patches,
            }
            results[label] = entry
            print(f"[OK] {label}: {len(patches)} patches (baud={baud}, sr={sr})", file=sys.stderr)
        else:
            results[label] = {
                "baud": baud,
                "sr": sr,
                "num_patches": 0,
                "upsampler": "np.interp (linear interpolation)",
                "patches": [],
            }
            print(f"[--] {label}: NO patches decoded (baud={baud})", file=sys.stderr)

    except Exception as e:
        print(f"[ERR] {label}: {e}", file=sys.stderr)
        results[label] = {"error": str(e)}

print(json.dumps(results, indent=2))
