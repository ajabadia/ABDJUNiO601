"""
Regression Test: Smart Import All Formats (Tape, SysEx, CSV, JSON)
Runs in sequence: injects data via JS bridge, verifies badge, metadata, section visibility.
"""
import sys, os, time, json, math

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from cdp_helpers import CDPClient, get_cdp_ws_url


def close_modal(client):
    """Close the Smart Import modal by calling closeSmartImport()."""
    client.eval_js("""
    (function() {
        if (typeof closeSmartImport === 'function') {
            closeSmartImport();
            return 'closed';
        }
        var modal = document.getElementById('modal-smartImport');
        if (modal) modal.style.display = 'none';
        return 'force-closed';
    })();
    """)
    time.sleep(0.5)


def get_full_state(client):
    """Get complete Smart Import modal state as dict."""
    raw = client.eval_js("""JSON.stringify({
        modal: (function(){
            var m = document.getElementById('modal-smartImport');
            if (!m) return 'NOT FOUND';
            return window.getComputedStyle(m).display !== 'none' ? 'visible' : 'hidden';
        })(),
        badge: (document.getElementById('si-format-badge')||{}).textContent||'',
        badgeBg: (function(){
            var b = document.getElementById('si-format-badge');
            return b ? window.getComputedStyle(b).backgroundColor : '';
        })(),
        tapeSec: (function(){
            var s = document.getElementById('si-tape-section');
            return s ? window.getComputedStyle(s).display !== 'none' : false;
        })(),
        sysexSec: (function(){
            var s = document.getElementById('si-sysex-section');
            return s ? window.getComputedStyle(s).display !== 'none' : false;
        })(),
        csvSec: (function(){
            var s = document.getElementById('si-csv-section');
            return s ? window.getComputedStyle(s).display !== 'none' : false;
        })(),
        deviceId: (document.getElementById('si-sysex-device')||{}).textContent||'',
        funcCode: (document.getElementById('si-sysex-function')||{}).textContent||'',
        checksum: (document.getElementById('si-sysex-checksum')||{}).textContent||'',
        hexPrev: (document.getElementById('si-sysex-hex')||{}).textContent||'',
        csvCols: (document.getElementById('si-csv-columns')||{}).textContent||'',
        csvParams: (document.getElementById('si-csv-params')||{}).textContent||'',
        csvColList: (document.getElementById('si-csv-column-list')||{}).textContent||'',
        namesVis: (function(){
            var s = document.getElementById('si-preset-names-section');
            return s ? window.getComputedStyle(s).display !== 'none' : false;
        })(),
        namesCount: (function(){
            var e = document.getElementById('si-preset-names');
            return e ? (e.innerHTML.match(/<br>/g)||[]).length + 1 : 0;
        })(),
        btnEnabled: (function(){
            var b = document.getElementById('btn-si-import');
            return b ? !b.disabled : false;
        })(),
        sumVis: (function(){
            var s = document.getElementById('si-preset-summary');
            return s ? window.getComputedStyle(s).display !== 'none' : false;
        })(),
        pCount: (document.getElementById('si-preset-count')||{}).textContent||'',
        bankCount: (document.getElementById('si-bank-count')||{}).textContent||'',
        quality: (document.getElementById('si-quality-badge')||{}).textContent||'',
        snr: (document.getElementById('si-snr')||{}).textContent||'',
        jitter: (document.getElementById('si-jitter')||{}).textContent||''
    })""", timeout=10)
    if raw:
        try: return json.loads(raw)
        except: return {"raw": raw[:200]}
    return {}


def run_format_test(client, fmt_name, inject_js, expected_checks):
    """
    Run a test for one format.
    inject_js: JS code that calls processSmartImportResult with format-specific data.
    expected_checks: list of (check_name, lambda_state_bool) tuples.
    Returns (passed, failed, results_list).
    """
    print(f"\n{'='*60}")
    print(f"TESTING: {fmt_name}")
    print(f"{'='*60}")

    # Close any open modal first
    close_modal(client)

    # Inject data
    print(f"\n  Injecting {fmt_name} data...")
    result = client.eval_js(inject_js)
    print(f"  Result: {result}")
    time.sleep(2)

    # Read state
    state = get_full_state(client)

    # Run checks
    passed = 0
    failed = 0
    results = []

    for check_name, check_fn in expected_checks:
        ok = check_fn(state)
        detail = ""
        if not ok:
            if check_name == "Badge text":
                detail = f" (got: '{state.get('badge','')}')"
            elif check_name == "Badge color":
                detail = f" (got: '{state.get('badgeBg','')}')"
            elif "visible" in check_name.lower():
                key = {"TAPE": "tapeSec", "SYSEX": "sysexSec", "CSV": "csvSec", "JSON": "csvSec"}.get(fmt_name, "")
                if key:
                    detail = f" (got: {state.get(key, 'N/A')})"
        if ok:
            passed += 1
            results.append((check_name, True))
        else:
            failed += 1
            results.append((check_name, False))
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {check_name}{detail}")

    return passed, failed, results, state


def main():
    grand_passed = 0
    grand_failed = 0
    all_results = []

    print("=" * 60)
    print("REGRESSION TEST: Smart Import All Formats (Tape, SysEx, CSV, JSON)")
    print("=" * 60)

    # 1. Connect CDP
    print("\n[1] Connecting to CDP...")
    ws_url = get_cdp_ws_url(port=9222, max_retries=10, retry_delay=2)
    if not ws_url:
        print("FAIL: Cannot connect to CDP")
        return False
    client = CDPClient(ws_url)
    client.send("Page.enable")
    time.sleep(1)
    client._drain()
    print("  Connected OK\n")

    # ═══════════════════════════════════════════════
    # FORMAT 1: TAPE
    # ═══════════════════════════════════════════════

    tape_patches = ["PATCH 01","PATCH 02","PATCH 03","PATCH 04","PATCH 05","PATCH 06","PATCH 07","PATCH 08"]
    tape_names_json = json.dumps(tape_patches)
    tape_decoder_entry = json.dumps({
        "label": "Juno-106 (1200 baud) [AUTO]",
        "patchCount": 8,
        "rawBytes": 144,
        "elapsedS": 0.35,
        "rank": 1,
        "duplicates": 0
    })
    waveform_data = [round(0.3 * math.sin(i * 0.1), 4) for i in range(50)]

    tape_js = f"""
    (function() {{
        if (typeof processSmartImportResult !== 'function') return 'NO HANDLER';
        processSmartImportResult({{
            format: 'tape',
            fileName: 'test_tape.wav',
            success: true,
            detailedFormat: 'Juno Cassette Tape',
            totalPatches: 33,
            banksNeeded: 1,
            isSinglePatch: false,
            presetNames: {tape_names_json},
            snrDb: 28.5,
            jitterPct: 3.2,
            dropoutPct: 1.1,
            durationS: 42.0,
            qualityScore: 85,
            qualityLabel: 'GOOD',
            detectedBaudRate: 1200,
            autoSelected: true,
            winnerIndex: 0,
            decoderResults: [{tape_decoder_entry}],
            waveform: {json.dumps(waveform_data)}
        }});
        return 'INJECTED';
    }})();
    """

    tape_checks = [
        ("Modal visible", lambda s: s.get("modal") == "visible"),
        ("Badge text", lambda s: s.get("badge", "").upper() == "TAPE"),
        ("Badge color (amber)", lambda s: "255, 170, 0" in s.get("badgeBg", "") or "rgb(255, 170, 0)" in s.get("badgeBg", "")),
        ("Tape section visible", lambda s: s.get("tapeSec") == True),
        ("Sysex section hidden", lambda s: s.get("sysexSec") == False),
        ("CSV section hidden", lambda s: s.get("csvSec") == False),
        ("Preset names visible", lambda s: s.get("namesVis") == True),
        ("Preset names > 0", lambda s: s.get("namesCount", 0) > 0),
        ("Import button enabled", lambda s: s.get("btnEnabled") == True),
        ("Summary visible", lambda s: s.get("sumVis") == True),
        ("Preset count shown", lambda s: s.get("pCount", "") not in ("", "-")),
        ("Quality badge", lambda s: s.get("quality", "") != ""),
    ]

    p, f, r, state = run_format_test(client, "TAPE", tape_js, tape_checks)
    grand_passed += p
    grand_failed += f
    all_results.extend([("TAPE", n, ok) for n, ok in r])

    # ═══════════════════════════════════════════════
    # FORMAT 2: SYSEX
    # ═══════════════════════════════════════════════

    sysex_names = ['Strings 1','Strings 2','Organ 1','Brass','Piano 1','Bass 1','Saw Lead','PWM Pad']
    sysex_names_json = json.dumps(sysex_names)

    sysex_js = f"""
    (function() {{
        if (typeof processSmartImportResult !== 'function') return 'NO HANDLER';
        processSmartImportResult({{
            format: 'sysex',
            fileName: 'test_juno106_bank.syx',
            success: true,
            detailedFormat: 'Roland Juno SysEx',
            totalPatches: 33,
            banksNeeded: 1,
            isSinglePatch: false,
            presetNames: {sysex_names_json},
            deviceId: 0x30,
            functionCode: 1,
            checksumValid: false,
            hexPreview: 'F0 41 30 02 01 00 00 00 40 00 7F 00 40 00 40 7F 00 20 40 20 ...',
            fileSize: 1159
        }});
        return 'INJECTED';
    }})();
    """

    sysex_checks = [
        ("Modal visible", lambda s: s.get("modal") == "visible"),
        ("Badge text", lambda s: s.get("badge", "").upper() == "SYSEX"),
        ("Badge color (blue)", lambda s: "0, 170, 255" in s.get("badgeBg", "") or "rgb(0, 170, 255)" in s.get("badgeBg", "")),
        ("Sysex section visible", lambda s: s.get("sysexSec") == True),
        ("Tape section hidden", lambda s: s.get("tapeSec") == False),
        ("CSV section hidden", lambda s: s.get("csvSec") == False),
        ("Device ID present", lambda s: s.get("deviceId", "") not in ("", "-")),
        ("Function code present", lambda s: s.get("funcCode", "") not in ("", "-")),
        ("Checksum present", lambda s: s.get("checksum", "") not in ("", "-")),
        ("Hex preview present", lambda s: len(s.get("hexPrev", "")) > 10),
        ("Preset names visible", lambda s: s.get("namesVis") == True),
        ("Preset names > 0", lambda s: s.get("namesCount", 0) > 0),
        ("Import button enabled", lambda s: s.get("btnEnabled") == True),
        ("Summary visible", lambda s: s.get("sumVis") == True),
        ("Preset count shown", lambda s: s.get("pCount", "") not in ("", "-")),
    ]

    p, f, r, state = run_format_test(client, "SYSEX", sysex_js, sysex_checks)
    grand_passed += p
    grand_failed += f
    all_results.extend([("SYSEX", n, ok) for n, ok in r])

    # ═══════════════════════════════════════════════
    # FORMAT 3: CSV
    # ═══════════════════════════════════════════════

    csv_names = ['Bass 1','Lead 1','Pad 1','Strings 1','Brass 1','FX 1','Keys 1','Organ 1']
    csv_names_json = json.dumps(csv_names)
    csv_columns = json.dumps(["name","kLfoRate","kLfoDelay","kDcoLfo","kDcoPwm","kDcoNoise","kVcfFreq","kVcfRes","kVcfEnv","kVcfLfo","kVcfKbd","kVcaLevel","kEnvA","kEnvD","kEnvS","kEnvR"])

    csv_js = f"""
    (function() {{
        if (typeof processSmartImportResult !== 'function') return 'NO HANDLER';
        processSmartImportResult({{
            format: 'csv',
            fileName: 'test_patches.csv',
            success: true,
            detailedFormat: 'Juno CSV Presets',
            totalPatches: 24,
            banksNeeded: 1,
            isSinglePatch: false,
            presetNames: {csv_names_json},
            columnCount: 16,
            columnNames: {csv_columns}
        }});
        return 'INJECTED';
    }})();
    """

    csv_checks = [
        ("Modal visible", lambda s: s.get("modal") == "visible"),
        ("Badge text", lambda s: s.get("badge", "").upper() == "CSV"),
        ("Badge color (green)", lambda s: "0, 255, 0" in s.get("badgeBg", "") or "rgb(0, 255, 0)" in s.get("badgeBg", "")),
        ("CSV section visible", lambda s: s.get("csvSec") == True),
        ("Tape section hidden", lambda s: s.get("tapeSec") == False),
        ("Sysex section hidden", lambda s: s.get("sysexSec") == False),
        ("Column count shown", lambda s: s.get("csvCols", "") not in ("", "-")),
        ("Parameters shown", lambda s: s.get("csvParams", "") not in ("", "-")),
        ("Column names list present", lambda s: len(s.get("csvColList", "")) > 10),
        ("Preset names visible", lambda s: s.get("namesVis") == True),
        ("Preset names > 0", lambda s: s.get("namesCount", 0) > 0),
        ("Import button enabled", lambda s: s.get("btnEnabled") == True),
        ("Summary visible", lambda s: s.get("sumVis") == True),
        ("Preset count shown", lambda s: s.get("pCount", "") not in ("", "-")),
    ]

    p, f, r, state = run_format_test(client, "CSV", csv_js, csv_checks)
    grand_passed += p
    grand_failed += f
    all_results.extend([("CSV", n, ok) for n, ok in r])

    # ═══════════════════════════════════════════════
    # FORMAT 4: JSON
    # ═══════════════════════════════════════════════

    json_names = ['Deep Pad','Bright Lead','Warm Brass','Analog Strings','Bass 1','FX Sweep','Arp Sequence','Organ']
    json_names_json = json.dumps(json_names)
    json_lib_name = "My Bank"

    json_js = f"""
    (function() {{
        if (typeof processSmartImportResult !== 'function') return 'NO HANDLER';
        processSmartImportResult({{
            format: 'json',
            fileName: 'my_bank.json',
            success: true,
            detailedFormat: 'JSON Bank',
            totalPatches: 8,
            banksNeeded: 1,
            isSinglePatch: false,
            presetNames: {json_names_json},
            libraryName: '{json_lib_name}',
            category: 'User',
            columnNames: ['name', 'lfoRate', 'lfoDelay', 'dcoPwm', 'vcfFreq', 'envA', 'envD', 'envS']
        }});
        return 'INJECTED';
    }})();
    """

    json_checks = [
        ("Modal visible", lambda s: s.get("modal") == "visible"),
        ("Badge text", lambda s: s.get("badge", "").upper() == "JSON"),
        ("Badge color (pink)", lambda s: "255, 0, 170" in s.get("badgeBg", "") or "rgb(255, 0, 170)" in s.get("badgeBg", "")),
        # JSON reuses the CSV section (#si-csv-section) for display
        ("CSV section visible (reused for JSON)", lambda s: s.get("csvSec") == True),
        ("Tape section hidden", lambda s: s.get("tapeSec") == False),
        ("Sysex section hidden", lambda s: s.get("sysexSec") == False),
        # JSON-specific metadata
        ("Library name shown in csvCols", lambda s: s.get("csvCols", "") not in ("", "-") and json_lib_name in s.get("csvCols", "")),
        ("Category shown in csvParams", lambda s: "Category:" in s.get("csvParams", "") and "User" in s.get("csvParams", "")),
        ("JSON bank info in column-list", lambda s: "JSON Bank" in s.get("csvColList", "") or "8 patches" in s.get("csvColList", "")),
        ("Preset names visible", lambda s: s.get("namesVis") == True),
        ("Preset names > 0", lambda s: s.get("namesCount", 0) > 0),
        ("Import button enabled", lambda s: s.get("btnEnabled") == True),
        ("Summary visible", lambda s: s.get("sumVis") == True),
        ("Preset count shown", lambda s: s.get("pCount", "") not in ("", "-")),
    ]

    p, f, r, state = run_format_test(client, "JSON", json_js, json_checks)
    grand_passed += p
    grand_failed += f
    all_results.extend([("JSON", n, ok) for n, ok in r])

    # ═══════════════════════════════════════════════
    # FINAL RESULTS
    # ═══════════════════════════════════════════════

    print(f"\n{'='*60}")
    print("FINAL RESULTS")
    print(f"{'='*60}")

    # Breakdown by format
    for fmt in ["TAPE", "SYSEX", "CSV", "JSON"]:
        fmt_results = [(n, ok) for (f, n, ok) in all_results if f == fmt]
        fmt_pass = sum(1 for _, ok in fmt_results if ok)
        fmt_total = len(fmt_results)
        print(f"\n  {fmt}: {fmt_pass}/{fmt_total} passed")

    print(f"\n  TOTAL: {grand_passed}/{grand_passed + grand_failed} checks passed, {grand_failed} failed")

    if grand_failed == 0:
        print("\n[ALL FORMATS PASSED]")
    else:
        print("\nFAILURES:")
        for fmt, name, ok in all_results:
            if not ok:
                print(f"  [{fmt}] {name}")

    client.close()
    return grand_failed == 0


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
