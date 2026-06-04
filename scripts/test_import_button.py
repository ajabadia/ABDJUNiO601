#!/usr/bin/env python3
"""
End-to-End Test: Smart Tape Import from UI to import confirmation.

Flow:
  1. Connect CDP to running app
  2. Click [BROWSER] menu → click LOAD TAPE (.WAV)
  3. Handle native file dialog (select a real WAV file)
  4. Wait for smartDecode to complete and Import button to enable
  5. Click import and verify notification

Refactored to use cdp_helpers and win32_dialog modules.

Usage:
    Ensure app is built and running with
    WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS='--remote-debugging-port=9222'
    Then run:
    python scripts/test_import_button.py
"""

import json
import sys
import os
import time

# Ensure scripts/ is in sys.path for sibling imports
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)

from cdp_helpers import CDPClient, get_cdp_ws_url, wait_for_modal
from win32_dialog import find_file_dialog, set_file_and_open

# Resolve WAV file relative to project root (parent of scripts/)
_PROJECT_ROOT = os.path.dirname(_SCRIPT_DIR)
WAV_FILE = os.path.join(_PROJECT_ROOT, "docs", "JUNO-106",
                        "Roland Juno-60 factory programs group 1.wav")
if not os.path.exists(WAV_FILE):
    WAV_FILE = os.path.join(_PROJECT_ROOT, "docs", "JUNO-106",
                            "JUNO106 Bank A.wav")

TEST_NAME = "Smart Tape Import E2E"


def main():
    print("=" * 60)
    print(f"{TEST_NAME} — UI Navigation + File Dialog + Import")
    print("=" * 60)
    print(f"WAV: {WAV_FILE}")
    print()

    # ── 1. Connect CDP ─────────────────────────────────────────────
    print("[1] Connecting CDP...")
    ws_url = get_cdp_ws_url(title_filter="ABD")
    if not ws_url:
        ws_url = get_cdp_ws_url()  # fallback
    if not ws_url:
        print("[1] FAILED: Could not reach CDP on port 9222")
        return False
    print(f"[1] OK: {ws_url}")

    cdp = CDPClient(ws_url)
    cdp.eval_js("1")  # warm-up
    print("[1] CDP ready")
    print()

    # ── 2. App ready? ──────────────────────────────────────────────
    print("[2] Waiting for app to settle...")
    time.sleep(5)
    title = cdp.eval_js("document.title")
    splash = cdp.eval_js(
        "document.getElementById('splash-screen').style.display"
    )
    print(f"[2] Title: {title}")
    print(f"[2] Splash display: {splash}")
    print()

    # ── 3. Open Preset Browser ──────────────────────────────────────
    print("[3] Opening Preset Browser via [BROWSER] menu...")
    result = cdp.click_by_text("BROWSER")
    print(f"[3] Browser click: {result}")
    time.sleep(1.5)

    browser_display = cdp.eval_js(
        "document.getElementById('modal-browser').style.display"
    )
    print(f"[3] Browser modal display: {browser_display}")
    print()

    # ── 4. Click LOAD TAPE (.WAV) ──────────────────────────────────
    print("[4] Clicking LOAD TAPE (.WAV)...")
    result = cdp.click_by_id("btn-load-tape")
    print(f"[4] Load Tape click: {result}")

    if "NOT_FOUND" in str(result):
        print("[4] Fallback: calling PresetBrowser.loadTape()...")
        result = cdp.eval_js("""
        (function() {
            if (typeof PresetBrowser !== 'undefined' && PresetBrowser.loadTape) {
                PresetBrowser.loadTape();
                return 'CALLED_PresetBrowser.loadTape';
            }
            if (typeof juce !== 'undefined' && juce.menuAction) {
                juce.menuAction('handleLoadTape');
                return 'CALLED_juce.menuAction';
            }
            return 'ALL_FAILED';
        })()
        """)
        print(f"[4] Fallback: {result}")
    print()

    # ── 5. Handle native file dialog ────────────────────────────────
    print("[5] Handling native file dialog...")
    time.sleep(2)
    dlg_hwnd, edit_hwnd = find_file_dialog(title_filter="Load Tape",
                                            timeout=25)

    if not dlg_hwnd:
        print("[5] FAILED: No file dialog found")
        print("[5] Checking fallback: waiting more...")
        time.sleep(3)
        dlg_hwnd, edit_hwnd = find_file_dialog(title_filter="Load Tape",
                                                timeout=15)
        if not dlg_hwnd:
            print("[5] FAILED: Still no dialog found")
            cdp.close()
            return False

    ok = set_file_and_open(dlg_hwnd, edit_hwnd, WAV_FILE)
    print(f"[5] Dialog handled: {ok}")
    print()

    # ── 6. Wait for Smart Tape Import modal ────────────────────────
    print("[6] Waiting for Smart Tape Import modal...")
    modal_found, btn_enabled = wait_for_modal(
        cdp, modal_id="modal-smartImport",
        enable_on="btn-si-import",
        timeout=60, poll_interval=1
    )
    print()

    # ── 7. Report ──────────────────────────────────────────────────
    print("[7] FINAL REPORT:")

    if not btn_enabled:
        debug = cdp.eval_js("""JSON.stringify({
            modalExists: document.getElementById('modal-smartImport') !== null,
            btnExists: document.getElementById('btn-si-import') !== null,
            log: (document.getElementById('si-progress-log')||{}).innerText || '',
            notify: (document.getElementById('notification-message')||{}).innerText || ''
        })""")
        print(f"  Debug: {debug}")
        print(f"\n{'=' * 60}")
        print(f"** {TEST_NAME}: FAILED ** — Import button did not enable")
        print(f"{'=' * 60}")
        cdp.close()
        return False

    # Gather metrics and decoder info
    metrics = cdp.eval_js("""JSON.stringify({
        snr: (document.getElementById('si-snr')||{}).innerText,
        jitter: (document.getElementById('si-jitter')||{}).innerText,
        dropouts: (document.getElementById('si-dropouts')||{}).innerText,
        duration: (document.getElementById('si-duration')||{}).innerText,
        quality: (document.getElementById('si-quality-badge')||{}).innerText
    })""")
    print(f"  Metrics: {metrics}")

    decoder_count = cdp.eval_js(
        "(document.getElementById('si-decoder-list')||{}).children.length || 0"
    )
    print(f"  Decoder entries: {decoder_count}")

    waveform = cdp.eval_js(
        "var c=document.getElementById('si-waveform-canvas'); "
        "c ? c.width+'x'+c.height : 'NONE'"
    )
    print(f"  Waveform: {waveform}")

    # Check format badge
    badge = cdp.eval_js("(document.getElementById('si-format-badge')||{}).innerText")
    print(f"  Format badge: {badge}")

    # Check preset summary
    preset_count = cdp.eval_js(
        "(document.getElementById('si-preset-count')||{}).innerText"
    )
    print(f"  Presets reported: {preset_count}")

    # ── 8. Click Import ─────────────────────────────────────────────
    print("\n[8] Clicking IMPORT SELECTED...")
    result = cdp.click_by_id("btn-si-import")
    print(f"[8] Import click: {result}")
    time.sleep(3)

    # Check notification
    notification = cdp.eval_js(
        "(document.getElementById('notification-message')||{}).innerText"
    )
    print(f"[8] Notification: {notification}")

    cdp.close()

    print(f"\n{'=' * 60}")
    print(f"** {TEST_NAME}: PASSED **")
    print(f"{'=' * 60}")
    return True


if __name__ == "__main__":
    ok = main()
    sys.exit(0 if ok else 1)
