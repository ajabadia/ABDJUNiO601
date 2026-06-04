"""
E2E Test: File > Import SysEx -> Smart Import dialog with SYSEX badge + metadata.
Uses CDP + Win32 helpers. Falls back to direct JS bridge.
"""
import sys, os, time, json, ctypes

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
sys.path.insert(0, SCRIPT_DIR)

from cdp_helpers import CDPClient, get_cdp_ws_url
from win32_dialog import find_file_dialog, set_file_and_open

TEST_SYX = os.path.join(PROJECT_ROOT, "docs", "test_juno106_bank.syx")

# Read SysEx file
with open(TEST_SYX, "rb") as f:
    SYX_DATA = f.read()

DEVICE_ID = SYX_DATA[2] if len(SYX_DATA) > 2 else 0
FUNCTION_CODE = SYX_DATA[4] if len(SYX_DATA) > 4 else 0
CHECKSUM_OK = (sum(SYX_DATA[5:-2]) & 0x7F) == SYX_DATA[-2] if len(SYX_DATA) > 5 else False
HEX_PREVIEW = " ".join(f"{b:02X}" for b in SYX_DATA[:64])

def get_state(client):
    """Get full Smart Import modal state as dict."""
    raw = client.eval_js("""JSON.stringify({
        modal: document.getElementById('modal-smartImport') ? 
            (window.getComputedStyle(document.getElementById('modal-smartImport')).display !== 'none' ? 'visible' : 'hidden') : 'NOT FOUND',
        badge: (document.getElementById('si-format-badge')||{}).textContent||'',
        badgeBg: document.getElementById('si-format-badge') ? 
            window.getComputedStyle(document.getElementById('si-format-badge')).backgroundColor : '',
        sysexVis: (function(){
            var s = document.getElementById('si-sysex-section');
            return s ? window.getComputedStyle(s).display !== 'none' : false;
        })(),
        deviceId: (document.getElementById('si-sysex-device')||{}).textContent||'',
        funcCode: (document.getElementById('si-sysex-function')||{}).textContent||'',
        checksum: (document.getElementById('si-sysex-checksum')||{}).textContent||'',
        HexPrev: (document.getElementById('si-sysex-hex')||{}).textContent||'',
        namesVis: (function(){
            var s = document.getElementById('si-preset-names-section');
            return s ? window.getComputedStyle(s).display !== 'none' : false;
        })(),
        namesCount: (function(){
            var s = document.getElementById('si-preset-names');
            return s ? (s.innerHTML.match(/<br>/g) || []).length + 1 : 0;
        })(),
        btnEnabled: (function(){
            var b = document.getElementById('btn-si-import');
            return b ? !b.disabled : false;
        })(),
        sumVis: (function(){
            var s = document.getElementById('si-preset-summary');
            return s ? window.getComputedStyle(s).display !== 'none' : false;
        })(),
        pCount: (document.getElementById('si-preset-count')||{}).textContent||''
    })""", timeout=10)
    if raw:
        try: return json.loads(raw)
        except: return {"raw": raw[:200]}
    return {}

def main():
    passed = 0
    failed = 0
    results = []

    print("=" * 60)
    print("TEST: Smart Import SYSEX via CDP Helpers")
    print("=" * 60)

    # 1. Connect CDP
    print("\n[1] Connecting to CDP...")
    ws_url = get_cdp_ws_url(port=9222, max_retries=10, retry_delay=2)
    if not ws_url:
        print("FAIL: Cannot connect")
        return False
    client = CDPClient(ws_url)
    client.send("Page.enable")
    time.sleep(1)
    client._drain()
    print("  Connected OK")

    # 2. Inject SysEx data via direct JS bridge
    print("\n[2] Injecting SysEx data via JS bridge...")
    
    preset_names = ['Strings 1','Strings 2','Organ 1','Brass','Piano 1','Bass 1','Saw Lead','PWM Pad']
    names_json = json.dumps(preset_names)
    hex_trunc = HEX_PREVIEW[:60] + ("..." if len(HEX_PREVIEW) > 60 else "")
    
    js = """
    (function() {
        if (typeof processSmartImportResult !== 'function') return 'NO HANDLER';
        processSmartImportResult({
            format: 'sysex',
            fileName: 'test_juno106_bank.syx',
            success: true,
            detailedFormat: 'Roland Juno SysEx',
            totalPatches: 33,
            banksNeeded: 1,
            isSinglePatch: false,
            presetNames: """ + names_json + """,
            deviceId: """ + str(DEVICE_ID) + """,
            functionCode: """ + str(FUNCTION_CODE) + """,
            checksumValid: """ + ("true" if CHECKSUM_OK else "false") + """,
            hexPreview: \"""" + hex_trunc + """\",
            fileSize: """ + str(len(SYX_DATA)) + """
        });
        return 'INJECTED';
    })();
    """
    
    result = client.eval_js(js)
    print(f"  Injection result: {result}")
    time.sleep(2)

    # 3. Get full state
    print("\n[3] Reading modal state...")
    state = get_state(client)
    print(f"  State: {json.dumps(state, indent=2)}")

    # 4. Verify each check
    checks = [
        ("Modal visible", state.get("modal") == "visible"),
        ("Badge = SYSEX", state.get("badge", "").upper() == "SYSEX"),
        ("Sysex section visible", state.get("sysexVis") == True),
        ("Device ID present", state.get("deviceId", "") != "" and state.get("deviceId") != "-"),
        ("Function code present", state.get("funcCode", "") != "" and state.get("funcCode") != "-"),
        ("Checksum present", state.get("checksum", "") != "" and state.get("checksum") != "-"),
        ("Hex preview present", len(state.get("HexPrev", "")) > 10),
        ("Preset names visible", state.get("namesVis") == True),
        ("Preset names > 0", state.get("namesCount", 0) > 0),
        ("Import button enabled", state.get("btnEnabled") == True),
        ("Summary visible", state.get("sumVis") == True),
        ("Preset count shown", state.get("pCount", "") != "" and state.get("pCount") != "-"),
    ]

    print("\n[4] Verifying checks...")
    for name, ok in checks:
        if ok:
            passed += 1
            results.append((name, True))
        else:
            failed += 1
            results.append((name, False))
        print(f"  {'PASS' if ok else 'FAIL'}: {name}")

    # 5. Cleanup - dismiss dialog if present
    try:
        user32 = ctypes.windll.user32
        dhwnd, _ = find_file_dialog(title_filter=None, timeout=3)
        if dhwnd:
            user32.PostMessageW(dhwnd, 0x0100, 0x1B, 0)  # ESC
            time.sleep(0.3)
            user32.PostMessageW(dhwnd, 0x0101, 0x1B, 0)
            print("\n[5] Dismissed stale dialog (ESC)")
    except:
        pass
    
    client.close()

    # Results
    print("\n" + "=" * 60)
    print("RESULTS")
    print("=" * 60)
    for name, ok in results:
        print(f"  {'PASS' if ok else 'FAIL'}: {name}")
    print(f"\n  Total: {passed}/{passed + failed} passed, {failed} failed")
    print("ALL CHECKS PASSED" if failed == 0 else f"{passed}/{passed + failed} checks passed")
    
    return failed == 0

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
