"""
Verify "Analysis complete!" appears in the Smart Import progress log
for all 4 formats: Tape, SysEx, CSV, JSON.
"""
import sys, os, time, json, math

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from cdp_helpers import CDPClient, get_cdp_ws_url


def check_analysis_complete(client, fmt_name, data_js):
    """Inject Smart Import data and check progress log for 'Analysis complete!'"""
    # Close any open modal
    client.eval_js('if (typeof closeSmartImport === "function") closeSmartImport()')
    time.sleep(0.5)

    # Inject data
    client.eval_js(data_js)
    time.sleep(1.5)

    # Read progress log text
    log_text = client.eval_js("""
    (function() {
        var log = document.getElementById('si-progress-log');
        if (!log) return null;
        return log.innerText;
    })();
    """, timeout=10)

    if log_text is None:
        print(f"  [FAIL] {fmt_name}: progress-log element not found")
        return False

    # Check for 'Analysis complete' (case-insensitive)
    found = 'Analysis complete' in log_text
    status = 'PASS' if found else 'FAIL'
    print(f"  [{status}] {fmt_name}: 'Analysis complete!' in progress-log")
    lines = [l.strip() for l in log_text.split('\n') if l.strip()]
    if lines:
        last_line = lines[-1]
        print(f"    Last line: > {last_line}")
    return found


def main():
    ws_url = get_cdp_ws_url(port=9222, max_retries=10, retry_delay=2)
    if not ws_url:
        print("FAIL: Cannot connect to CDP")
        return False

    client = CDPClient(ws_url)
    client.send("Page.enable")
    time.sleep(1)
    client._drain()

    # Common preset names
    tape_names = json.dumps(["PATCH 01", "PATCH 02", "PATCH 03", "PATCH 04"])
    sysex_names = json.dumps(["Strings 1", "Organ 1", "Brass", "Piano 1"])
    csv_names = json.dumps(["Bass 1", "Lead 1", "Pad 1", "Strings 1"])
    json_names = json.dumps(["Deep Pad", "Bright Lead", "Warm Brass", "Analog Strings"])
    waveform = json.dumps([round(0.3 * math.sin(i * 0.1), 4) for i in range(50)])

    # Format injection scripts
    formats = [
        ("TAPE", f"""
            (function() {{
                if (typeof processSmartImportResult !== 'function') return 'NO HANDLER';
                processSmartImportResult({{
                    format: 'tape', success: true, fileName: 'test_tape.wav',
                    totalPatches: 4, banksNeeded: 1, isSinglePatch: false,
                    presetNames: {tape_names},
                    snrDb: 28.5, jitterPct: 3.2, dropoutPct: 1.1, durationS: 42,
                    qualityScore: 85, qualityLabel: 'GOOD', detectedBaudRate: 1200,
                    autoSelected: true, winnerIndex: 0,
                    decoderResults: [{{label: 'J106', patchCount: 4, rawBytes: 72, elapsedS: 0.35, rank: 1, duplicates: 0}}],
                    waveform: {waveform}
                }});
                return 'INJECTED';
            }})();
        """),
        ("SYSEX", f"""
            (function() {{
                if (typeof processSmartImportResult !== 'function') return 'NO HANDLER';
                processSmartImportResult({{
                    format: 'sysex', success: true, fileName: 'test_bank.syx',
                    totalPatches: 4, banksNeeded: 1, isSinglePatch: false,
                    presetNames: {sysex_names},
                    deviceId: 0x30, functionCode: 1, checksumValid: true,
                    hexPreview: 'F0 41 30 02 01 00 00 00 40 00 7F 00 40 00 40 7F 00 20 40 20 ...'
                }});
                return 'INJECTED';
            }})();
        """),
        ("CSV", f"""
            (function() {{
                if (typeof processSmartImportResult !== 'function') return 'NO HANDLER';
                processSmartImportResult({{
                    format: 'csv', success: true, fileName: 'test_patches.csv',
                    totalPatches: 4, banksNeeded: 1, isSinglePatch: false,
                    presetNames: {csv_names},
                    columnCount: 16, columnNames: ['name', 'lfoRate', 'vcfFreq', 'envAttack', 'envDecay']
                }});
                return 'INJECTED';
            }})();
        """),
        ("JSON", f"""
            (function() {{
                if (typeof processSmartImportResult !== 'function') return 'NO HANDLER';
                processSmartImportResult({{
                    format: 'json', success: true, fileName: 'my_bank.json',
                    totalPatches: 4, banksNeeded: 1, isSinglePatch: false,
                    presetNames: {json_names},
                    libraryName: 'My Bank', category: 'User'
                }});
                return 'INJECTED';
            }})();
        """),
    ]

    all_pass = True
    print("=" * 60)
    print('VERIFY "Analysis complete!" IN PROGRESS LOG')
    print("=" * 60)

    for fmt_name, js_code in formats:
        ok = check_analysis_complete(client, fmt_name, js_code)
        if not ok:
            all_pass = False

    print()
    print("=" * 60)
    if all_pass:
        print("ALL FORMATS: Analysis complete! verified")
    else:
        print("SOME FORMATS FAILED")
    print("=" * 60)

    client.close()
    return all_pass


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
